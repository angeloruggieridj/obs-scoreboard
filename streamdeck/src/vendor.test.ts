// SPDX-License-Identifier: GPL-2.0-or-later
import { afterEach, describe, expect, it, vi } from "vitest";
import { OBSWebSocket } from "obs-websocket-js";
import { FakeVendor, ObsVendorClient, url, RECONNECT_MAX_MS } from "./vendor";

// A minimal stand-in for obs-websocket-js's OBSWebSocket, injected into ObsVendorClient so the real
// reconnect/backoff logic can be tested without a socket (finding 1/2).
class FakeSocket {
  handlers = new Map<string, ((data?: unknown) => void)[]>();
  connectCalls = 0;
  disconnectCalls = 0;
  failConnect = false;
  calls: { vendorName: string; requestType: string; requestData: unknown }[] = [];
  responses: Record<string, unknown> = {};
  throwOn = new Set<string>();

  on(event: string, cb: (data?: unknown) => void): this {
    const list = this.handlers.get(event) ?? [];
    list.push(cb);
    this.handlers.set(event, list);
    return this;
  }
  emit(event: string, data?: unknown): void {
    for (const cb of this.handlers.get(event) ?? []) cb(data);
  }
  connect(): Promise<unknown> {
    this.connectCalls += 1;
    if (this.failConnect) {
      // obs-websocket-js emits ConnectionClosed for every failed connect too (finding 1).
      this.emit("ConnectionClosed");
      return Promise.reject(new Error("connect failed"));
    }
    return Promise.resolve();
  }
  disconnect(): Promise<void> {
    this.disconnectCalls += 1;
    this.emit("ConnectionClosed");
    return Promise.resolve();
  }
  call(_type: string, data: { vendorName: string; requestType: string; requestData: unknown }) {
    this.calls.push(data);
    if (this.throwOn.has(data.requestType)) return Promise.reject(new Error("transport error"));
    return Promise.resolve({ responseData: this.responses[data.requestType] ?? { result: "ok" } });
  }
}

const asSocket = (socket: FakeSocket): OBSWebSocket => socket as unknown as OBSWebSocket;
const config = { host: "127.0.0.1", port: 4455 };

describe("url", () => {
  it("builds the websocket url", () => {
    expect(url({ host: "127.0.0.1", port: 4455 })).toBe("ws://127.0.0.1:4455");
  });
});

describe("ObsVendorClient", () => {
  afterEach(() => vi.useRealTimers());

  it("syncs GetFields/GetState on connect and notifies synced once", async () => {
    const socket = new FakeSocket();
    socket.responses["GetFields"] = { result: "ok", fields: { clock: "20:00" } };
    socket.responses["GetState"] = { result: "ok", state: { protocol: 1, clockRunning: false } };
    const vendor = new ObsVendorClient(() => asSocket(socket));
    const synced = vi.fn();
    vendor.onSynced(synced);

    await vendor.connect(config);

    expect(vendor.getField("clock")).toBe("20:00");
    expect(vendor.getState()?.protocol).toBe(1);
    expect(socket.calls.map((c) => c.requestType)).toEqual(["GetFields", "GetState"]);
    expect(synced).toHaveBeenCalledTimes(1);
  });

  it("maps a throwing vendor call to error:transport", async () => {
    const socket = new FakeSocket();
    socket.throwOn.add("GetFields");
    const vendor = new ObsVendorClient(() => asSocket(socket));
    expect(await vendor.request("GetFields")).toEqual({ result: "error", error: "transport" });
  });

  it("reconnects with a single loop and a doubling backoff", async () => {
    vi.useFakeTimers();
    const socket = new FakeSocket();
    socket.failConnect = true;
    const vendor = new ObsVendorClient(() => asSocket(socket));

    await expect(vendor.connect(config)).rejects.toThrow();
    expect(socket.connectCalls).toBe(1); // the failed connect

    // Exactly ONE attempt per backoff step: 1 s, then 2 s, then 4 s. The ConnectionClosed event
    // fired by each failed connect must NOT start a second, parallel loop.
    await vi.advanceTimersByTimeAsync(1000);
    expect(socket.connectCalls).toBe(2);
    await vi.advanceTimersByTimeAsync(1000);
    expect(socket.connectCalls).toBe(2); // 2 s have not elapsed yet
    await vi.advanceTimersByTimeAsync(1000);
    expect(socket.connectCalls).toBe(3);
    await vi.advanceTimersByTimeAsync(4000);
    expect(socket.connectCalls).toBe(4);
  });

  it("caps the backoff at the maximum", async () => {
    vi.useFakeTimers();
    const socket = new FakeSocket();
    socket.failConnect = true;
    const vendor = new ObsVendorClient(() => asSocket(socket));
    await expect(vendor.connect(config)).rejects.toThrow();

    // Drive the backoff up to the cap, then check a single max-delay step.
    for (const delay of [1000, 2000, 4000, 8000, 16000]) {
      await vi.advanceTimersByTimeAsync(delay);
    }
    const before = socket.connectCalls;
    await vi.advanceTimersByTimeAsync(RECONNECT_MAX_MS);
    expect(socket.connectCalls).toBe(before + 1);
  });

  it("cancels a pending reconnect on disconnect", async () => {
    vi.useFakeTimers();
    const socket = new FakeSocket();
    socket.failConnect = true;
    const vendor = new ObsVendorClient(() => asSocket(socket));
    await expect(vendor.connect(config)).rejects.toThrow();
    expect(socket.connectCalls).toBe(1);

    await vendor.disconnect();
    await vi.advanceTimersByTimeAsync(120000);
    expect(socket.connectCalls).toBe(1); // no attempt after the timer was cancelled
  });

  it("keeps a single loop after connect() is called again", async () => {
    vi.useFakeTimers();
    const socket = new FakeSocket();
    socket.failConnect = true;
    const vendor = new ObsVendorClient(() => asSocket(socket));
    await expect(vendor.connect(config)).rejects.toThrow();

    // plugin.ts reconnects with disconnect() then connect(): the old sleeping loop must not resume
    // alongside the new one.
    await vendor.disconnect();
    await expect(vendor.connect(config)).rejects.toThrow();
    const afterSecondConnect = socket.connectCalls;
    expect(afterSecondConnect).toBe(2); // the two explicit connect() calls only

    await vi.advanceTimersByTimeAsync(1000);
    expect(socket.connectCalls).toBe(3); // exactly one reconnect from the new loop
  });

  it("recovers and syncs again after a reconnect", async () => {
    vi.useFakeTimers();
    const socket = new FakeSocket();
    socket.failConnect = true;
    const vendor = new ObsVendorClient(() => asSocket(socket));
    const synced = vi.fn();
    vendor.onSynced(synced);
    await expect(vendor.connect(config)).rejects.toThrow();

    socket.responses["GetFields"] = { result: "ok", fields: { clock: "05:00" } };
    socket.responses["GetState"] = { result: "ok", state: { protocol: 1, clockRunning: true } };
    socket.failConnect = false;
    await vi.advanceTimersByTimeAsync(1000); // the reconnect attempt succeeds

    expect(vendor.getField("clock")).toBe("05:00");
    expect(vendor.getState()?.clockRunning).toBe(true);
    expect(synced).toHaveBeenCalledTimes(1);

    // A later dropped connection schedules a fresh attempt at the reset 1 s delay.
    socket.emit("ConnectionClosed");
    await vi.advanceTimersByTimeAsync(1000);
    expect(socket.connectCalls).toBeGreaterThanOrEqual(2);
  });
});

describe("FakeVendor", () => {
  it("records requests and returns the canned envelope", async () => {
    const vendor = new FakeVendor();
    vendor.replies["ClockStart"] = { result: "rejected", reason: "clock.running" };
    expect(await vendor.request("ClockStart")).toEqual({ result: "rejected", reason: "clock.running" });
    expect(vendor.calls).toEqual([{ type: "ClockStart", data: {} }]);
    expect(await vendor.request("ClockStop")).toEqual({ result: "ok" });
  });
  it("maps a transport failure to error:transport", async () => {
    const vendor = new FakeVendor();
    vendor.throwOn.add("GetFields");
    expect(await vendor.request("GetFields")).toEqual({ result: "error", error: "transport" });
  });
  it("dispatches vendor events to every handler", () => {
    const vendor = new FakeVendor();
    const seen: unknown[] = [];
    vendor.on("ScoreChanged", (data) => seen.push(data));
    vendor.on("ScoreChanged", (data) => seen.push(data));
    vendor.emit("ScoreChanged", { team: "home", score: 1 });
    expect(seen).toEqual([
      { team: "home", score: 1 },
      { team: "home", score: 1 },
    ]);
  });
  it("reads the initial state and notifies synced on connect", async () => {
    const vendor = new FakeVendor();
    vendor.replies["GetFields"] = { result: "ok", fields: { clock: "20:00" } };
    vendor.replies["GetState"] = { result: "ok", state: { protocol: 1 } as never };
    const synced = vi.fn();
    vendor.onSynced(synced);
    await vendor.connect();
    expect(vendor.getField("clock")).toBe("20:00");
    expect(vendor.calls.map((c) => c.type)).toEqual(["GetFields", "GetState"]);
    expect(synced).toHaveBeenCalledTimes(1);
  });
});
