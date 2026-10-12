// SPDX-License-Identifier: MIT
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import {
  FakeVendor,
  ObsVendorClient,
  url,
  type JsonObject,
  type VendorEventPayload,
  type VendorSocket,
} from "./vendor";

describe("FakeVendor", () => {
  it("builds the websocket url", () => {
    expect(url({ host: "127.0.0.1", port: 4455 })).toBe("ws://127.0.0.1:4455");
  });
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
  it("reads the initial state on connect", async () => {
    const vendor = new FakeVendor();
    vendor.replies["GetFields"] = { result: "ok", fields: { clock: "20:00" } };
    vendor.replies["GetState"] = { result: "ok", state: { protocol: 1 } as never };
    await vendor.connect();
    expect(vendor.getField("clock")).toBe("20:00");
    expect(vendor.calls.map((c) => c.type)).toEqual(["GetFields", "GetState"]);
  });
  it("notifies synced handlers on every syncState (finding 4)", async () => {
    const vendor = new FakeVendor();
    vendor.replies["GetState"] = { result: "ok", state: { protocol: 1 } as never };
    let synced = 0;
    vendor.onSynced(() => (synced += 1));
    await vendor.connect();
    expect(synced).toBe(1);
    await vendor.syncState();
    expect(synced).toBe(2);
  });
});

// The slice of obs-websocket-js used by ObsVendorClient, driven by hand: `connect`/`call` can be
// made to reject, and `ConnectionClosed`/`VendorEvent` emitted, without a real socket.
class FakeSocket {
  connectCalls: { url: string; password?: string; options?: { eventSubscriptions?: number } }[] = [];
  callCalls: { requestType: string; data?: JsonObject }[] = [];
  disconnectCalls = 0;
  connectImpl: () => Promise<unknown> = () => Promise.resolve({});
  callImpl: (data?: JsonObject) => Promise<{ responseData?: unknown }> = () => Promise.resolve({});
  disconnectImpl: () => Promise<unknown> = () => Promise.resolve({});
  private readonly closed: (() => void)[] = [];
  private readonly vendors: ((event: VendorEventPayload) => void)[] = [];

  on(event: string, listener: (...args: unknown[]) => void): void {
    if (event === "ConnectionClosed") this.closed.push(listener as () => void);
    else this.vendors.push(listener as (event: VendorEventPayload) => void);
  }

  async connect(
    connectUrl: string,
    password?: string,
    options?: { eventSubscriptions?: number },
  ): Promise<unknown> {
    this.connectCalls.push({ url: connectUrl, password, options });
    return this.connectImpl();
  }

  async call(_requestType: string, data?: JsonObject): Promise<{ responseData?: unknown }> {
    this.callCalls.push({ requestType: _requestType, data });
    return this.callImpl(data);
  }

  async disconnect(): Promise<unknown> {
    this.disconnectCalls += 1;
    return this.disconnectImpl();
  }

  emitClosed(): void {
    for (const listener of this.closed) listener();
  }

  emitVendor(event: VendorEventPayload): void {
    for (const listener of this.vendors) listener(event);
  }
}

const clientFor = (socket: FakeSocket): ObsVendorClient =>
  new ObsVendorClient(socket as unknown as VendorSocket);

// The requestType of the inner CallVendorRequest, as the plugin vendor receives it.
const vendorType = (data?: JsonObject): string | undefined =>
  (data as { requestType?: string } | undefined)?.requestType;

// A socket whose connect rejects and which, like obs-websocket-js, also emits ConnectionClosed for
// the failed attempt (the condition that used to start two reconnect loops, finding 1).
const failingConnect = (socket: FakeSocket): void => {
  socket.connectImpl = () => {
    socket.emitClosed();
    return Promise.reject(new Error("connection refused"));
  };
};

const okState = (requestType: string | undefined): { responseData: unknown } =>
  requestType === "GetFields"
    ? { responseData: { result: "ok", fields: { clock: "20:00" } } }
    : { responseData: { result: "ok", state: { protocol: 1, clockRunning: false, matchEnded: false } } };

describe("ObsVendorClient", () => {
  it("maps a throwing socket.call to error:transport", async () => {
    const socket = new FakeSocket();
    socket.callImpl = () => Promise.reject(new Error("obs-websocket answered 600"));
    expect(await clientFor(socket).request("GetFields")).toEqual({
      result: "error",
      error: "transport",
    });
  });

  it("returns the responseData envelope and sends the CallVendorRequest shape", async () => {
    const socket = new FakeSocket();
    socket.callImpl = () =>
      Promise.resolve({ responseData: { result: "ok", fields: { clock: "20:00" } } });
    expect(await clientFor(socket).request("GetFields")).toEqual({
      result: "ok",
      fields: { clock: "20:00" },
    });
    expect(socket.callCalls[0]).toEqual({
      requestType: "CallVendorRequest",
      data: { vendorName: "obs-scoreboard", requestType: "GetFields", requestData: {} },
    });
  });

  it("reads GetFields/GetState on connect and notifies synced after status(true)", async () => {
    const socket = new FakeSocket();
    socket.callImpl = (data) => Promise.resolve(okState(vendorType(data)));
    const sequence: string[] = [];
    const client = clientFor(socket);
    client.onStatus((connected) => sequence.push(connected ? "status:true" : "status:false"));
    client.onSynced(() => sequence.push("synced"));

    await client.connect({ host: "127.0.0.1", port: 4455 });

    expect(client.getField("clock")).toBe("20:00");
    expect(client.getState()).toMatchObject({ protocol: 1, clockRunning: false });
    expect(socket.connectCalls[0].url).toBe("ws://127.0.0.1:4455");
    expect(socket.connectCalls[0].options).toEqual({ eventSubscriptions: 512 });
    // status(true) fires before syncState completes, so seed must hang off "synced" (finding 4).
    expect(sequence).toEqual(["status:true", "synced"]);
  });

  it("dispatches only the matching obs-scoreboard vendor events", () => {
    const socket = new FakeSocket();
    const seen: unknown[] = [];
    const client = clientFor(socket);
    client.on("ScoreChanged", (data) => seen.push(data));
    socket.emitVendor({
      vendorName: "obs-scoreboard",
      eventType: "ScoreChanged",
      eventData: { team: "home", score: 1 },
    });
    socket.emitVendor({
      vendorName: "another-vendor",
      eventType: "ScoreChanged",
      eventData: { team: "away", score: 9 },
    });
    socket.emitVendor({
      vendorName: "obs-scoreboard",
      eventType: "StateChanged",
      eventData: { changed: {} },
    });
    expect(seen).toEqual([{ team: "home", score: 1 }]);
  });

  describe("reconnect (fake timers)", () => {
    beforeEach(() => vi.useFakeTimers());
    afterEach(() => vi.useRealTimers());

    it("attempts exactly one reconnect per backoff step after a failed connect", async () => {
      const socket = new FakeSocket();
      failingConnect(socket);
      const client = clientFor(socket);

      await expect(client.connect({ host: "127.0.0.1", port: 4455 })).rejects.toThrow(
        "connection refused",
      );
      expect(socket.connectCalls).toHaveLength(1);

      // First backoff step (1 s): exactly one attempt despite the catch AND ConnectionClosed.
      await vi.advanceTimersByTimeAsync(1000);
      expect(socket.connectCalls).toHaveLength(2);

      // Backoff doubled to 2 s: no attempt at +1 s, one more at +2 s (t = 3 s).
      await vi.advanceTimersByTimeAsync(1000);
      expect(socket.connectCalls).toHaveLength(2);
      await vi.advanceTimersByTimeAsync(1000);
      expect(socket.connectCalls).toHaveLength(3);

      // t = 5 s: still a single loop. The old double loop would have had four attempts by now
      // (the catch re-entry and the ConnectionClosed handler each spawned a new loop at t = 3 s).
      await vi.advanceTimersByTimeAsync(2000);
      expect(socket.connectCalls).toHaveLength(3);
      // Backoff doubled again to 4 s: the next (and only) attempt lands at t = 7 s.
      await vi.advanceTimersByTimeAsync(2000);
      expect(socket.connectCalls).toHaveLength(4);
    });

    it("cancels the pending reconnect timer on disconnect()", async () => {
      const socket = new FakeSocket();
      failingConnect(socket);
      const client = clientFor(socket);
      await expect(client.connect({ host: "127.0.0.1", port: 4455 })).rejects.toThrow(
        "connection refused",
      );

      await client.disconnect();
      const attempts = socket.connectCalls.length;
      await vi.advanceTimersByTimeAsync(60000);
      expect(socket.connectCalls).toHaveLength(attempts);
      expect(socket.disconnectCalls).toBe(1);
    });

    it("reconnects once and re-syncs when an established connection closes", async () => {
      const socket = new FakeSocket();
      socket.callImpl = (data) => Promise.resolve(okState(vendorType(data)));
      const client = clientFor(socket);
      let synced = 0;
      client.onSynced(() => (synced += 1));

      await client.connect({ host: "127.0.0.1", port: 4455 });
      expect(socket.connectCalls).toHaveLength(1);
      expect(synced).toBe(1);

      socket.emitClosed();
      await vi.advanceTimersByTimeAsync(1000);

      expect(socket.connectCalls).toHaveLength(2);
      expect(synced).toBe(2); // the reconnect re-read GetFields/GetState (Review Focus 13)
    });
  });
});
