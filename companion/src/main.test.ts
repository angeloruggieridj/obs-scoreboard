// SPDX-License-Identifier: MIT
import { describe, expect, it, vi } from "vitest";
import ScoreboardInstance from "./main";
import { FakeVendor, type VendorClient } from "./vendor";

// The minimal Companion host context `InstanceBase` writes through. `isInstanceContext` only needs
// `id: string` and `_isInstanceContext: true`; the methods record what the module publishes.
interface FakeContext {
  id: string;
  label: string;
  _isInstanceContext: true;
  variableValues: Record<string, string | number>[];
  feedbackChecks: string[][];
  statuses: unknown[];
  setActionDefinitions: ReturnType<typeof vi.fn>;
  setFeedbackDefinitions: ReturnType<typeof vi.fn>;
  setPresetDefinitions: ReturnType<typeof vi.fn>;
  setVariableDefinitions: ReturnType<typeof vi.fn>;
  setVariableValues(values: Record<string, string | number>): void;
  checkFeedbacks(ids: string[]): void;
  checkFeedbacksById: ReturnType<typeof vi.fn>;
  updateStatus(status: unknown): void;
  saveConfig: ReturnType<typeof vi.fn>;
}

function makeContext(): FakeContext {
  const variableValues: Record<string, string | number>[] = [];
  const feedbackChecks: string[][] = [];
  const statuses: unknown[] = [];
  return {
    id: "obs-scoreboard:test",
    label: "test",
    _isInstanceContext: true,
    variableValues,
    feedbackChecks,
    statuses,
    setActionDefinitions: vi.fn(),
    setFeedbackDefinitions: vi.fn(),
    setPresetDefinitions: vi.fn(),
    setVariableDefinitions: vi.fn(),
    setVariableValues: (values) => {
      variableValues.push(values);
    },
    checkFeedbacks: (ids) => {
      feedbackChecks.push(ids);
    },
    checkFeedbacksById: vi.fn(),
    updateStatus: (status) => {
      statuses.push(status);
    },
    saveConfig: vi.fn(),
  };
}

// Connects a ScoreboardInstance that uses a FakeVendor instead of a real obs-websocket client.
async function connectInstance(): Promise<{
  instance: ScoreboardInstance;
  vendor: FakeVendor;
  context: FakeContext;
}> {
  const context = makeContext();
  let vendor!: FakeVendor;
  class TestInstance extends ScoreboardInstance {
    protected override createVendor(): VendorClient {
      vendor = new FakeVendor();
      vendor.replies["GetFields"] = { result: "ok", fields: { clock: "20:00", "home.score": "3" } };
      vendor.replies["GetState"] = { result: "ok", state: { protocol: 1 } as never };
      return vendor;
    }
  }
  const instance = new TestInstance(context);
  await instance.init({ host: "127.0.0.1", port: 4455 }, true, undefined);
  return { instance, vendor, context };
}

describe("ScoreboardInstance", () => {
  it("seeds the variables from the initial sync", async () => {
    const { vendor, context } = await connectInstance();
    expect(vendor.calls.map((c) => c.type)).toEqual(["GetFields", "GetState"]);
    expect(context.variableValues.at(-1)).toMatchObject({ clock: "20:00", home_score: 3 });
    expect(context.feedbackChecks.length).toBeGreaterThan(0);
  });

  it("re-seeds the variables on every syncState, not just the first connect (finding 4)", async () => {
    const { vendor, context } = await connectInstance();

    // Simulate a reconnect: obs-websocket-js reconnects by itself and the client re-reads the state.
    vendor.replies["GetFields"] = { result: "ok", fields: { clock: "19:59", "home.score": "4" } };
    vendor.replies["GetState"] = { result: "ok", state: { protocol: 1 } as never };
    await vendor.syncState();

    expect(context.variableValues.at(-1)).toMatchObject({ clock: "19:59", home_score: 4 });
  });

  it("applies vendor events to the variables and feedbacks", async () => {
    const { vendor, context } = await connectInstance();
    vendor.emit("StateChanged", {
      changed: { clock: "19:58" },
      clockRunning: true,
      matchEnded: false,
    });
    expect(context.variableValues.at(-1)).toMatchObject({ clock: "19:58", clock_running: "true" });
    expect(context.feedbackChecks.length).toBeGreaterThan(1);
  });
});
