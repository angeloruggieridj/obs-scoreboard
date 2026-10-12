// SPDX-License-Identifier: GPL-2.0-or-later
import { describe, expect, it } from "vitest";
import { FakeVendor, url } from "./vendor";

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
});
