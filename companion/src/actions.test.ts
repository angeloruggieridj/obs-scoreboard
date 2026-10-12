// SPDX-License-Identifier: MIT
import { describe, expect, it } from "vitest";
import { requestNames } from "./protocol";
import { FakeVendor, type JsonObject } from "./vendor";
import {
  actionIds,
  buildActionDefinitions,
  requestFor,
  type ActionOptions,
} from "./actions";

const harness = () => {
  const vendor = new FakeVendor();
  const logs: { level: string; message: string }[] = [];
  const defs = buildActionDefinitions(vendor, (level, message) => logs.push({ level, message }));
  return { vendor, logs, defs };
};

async function invoke(
  defs: ReturnType<typeof buildActionDefinitions>,
  id: string,
  options: ActionOptions = {},
): Promise<void> {
  const def = defs[id];
  if (!def) throw new Error(`no action definition for ${id}`);
  await def.callback(
    { id: "a", controlId: "c", actionId: id, options, surfaceId: undefined },
    { signal: new AbortController().signal } as never,
  );
}

type Case = { id: string; options: ActionOptions; type: string; data: JsonObject };

// The mapping with no user options: every action falls back to its default (as in
// streamdeck/src/settings.ts).
const withDefaults: Case[] = [
  { id: "clock_toggle", options: {}, type: "ClockToggle", data: {} },
  { id: "clock_start", options: {}, type: "ClockStart", data: {} },
  { id: "clock_stop", options: {}, type: "ClockStop", data: {} },
  { id: "clock_reset", options: {}, type: "ClockReset", data: {} },
  { id: "clock_adjust", options: {}, type: "ClockAdjust", data: { deltaTenths: 0 } },
  { id: "clock_set", options: {}, type: "ClockSet", data: { tenths: 0 } },
  { id: "period_next", options: {}, type: "PeriodNext", data: { confirmed: false } },
  { id: "period_prev", options: {}, type: "PeriodPrev", data: { confirmed: false } },
  { id: "add_stat", options: {}, type: "AddStat", data: { team: "home", stat: "score", delta: 1 } },
  { id: "set_team_name", options: {}, type: "SetTeamName", data: { team: "home", name: "" } },
  {
    id: "penalty_add",
    options: {},
    type: "PenaltyAdd",
    data: { team: "home", player: "", phases: [{ tenths: 1200 }] },
  },
  { id: "penalty_edit", options: {}, type: "PenaltyEdit", data: { id: 0, tenths: 0 } },
  { id: "penalty_cancel", options: {}, type: "PenaltyCancel", data: { id: 0 } },
  { id: "penalty_cancel_active", options: {}, type: "PenaltyCancelActive", data: { team: "home", slot: 0 } },
  { id: "stoppage_set", options: {}, type: "StoppageSet", data: { minutes: 0 } },
  { id: "stoppage_add", options: {}, type: "StoppageAdd", data: { minutes: 0 } },
  { id: "end_match", options: {}, type: "EndMatch", data: {} },
  { id: "new_match", options: {}, type: "NewMatch", data: { sport: "ice_hockey", confirmed: false } },
  { id: "get_state", options: {}, type: "GetState", data: {} },
  { id: "get_fields", options: {}, type: "GetFields", data: {} },
];

// The mapping with explicit user options: the option values (team/stat/delta/player/tenths/
// sport/confirmed) drive the request payload.
const withOptions: Case[] = [
  { id: "clock_adjust", options: { delta: -600 }, type: "ClockAdjust", data: { deltaTenths: -600 } },
  { id: "clock_set", options: { tenths: 600 }, type: "ClockSet", data: { tenths: 600 } },
  { id: "period_next", options: { confirmed: true }, type: "PeriodNext", data: { confirmed: true } },
  { id: "period_prev", options: { confirmed: true }, type: "PeriodPrev", data: { confirmed: true } },
  {
    id: "add_stat",
    options: { team: "away", stat: "fouls2", delta: -1 },
    type: "AddStat",
    data: { team: "away", stat: "fouls2", delta: -1 },
  },
  {
    id: "set_team_name",
    options: { team: "away", player: "Ospiti" },
    type: "SetTeamName",
    data: { team: "away", name: "Ospiti" },
  },
  {
    id: "penalty_add",
    options: { team: "away", player: "24", tenths: 600 },
    type: "PenaltyAdd",
    data: { team: "away", player: "24", phases: [{ tenths: 600 }] },
  },
  { id: "penalty_edit", options: { tenths: 7, delta: 600 }, type: "PenaltyEdit", data: { id: 7, tenths: 600 } },
  { id: "penalty_cancel", options: { tenths: 3 }, type: "PenaltyCancel", data: { id: 3 } },
  {
    id: "penalty_cancel_active",
    options: { team: "away", delta: 1 },
    type: "PenaltyCancelActive",
    data: { team: "away", slot: 1 },
  },
  { id: "stoppage_set", options: { delta: 3 }, type: "StoppageSet", data: { minutes: 3 } },
  { id: "stoppage_add", options: { delta: -2 }, type: "StoppageAdd", data: { minutes: -2 } },
  {
    id: "new_match",
    options: { sport: "basketball", confirmed: true },
    type: "NewMatch",
    data: { sport: "basketball", confirmed: true },
  },
];

describe("buildActionDefinitions", () => {
  it("has one action definition for each of the 20 protocol requests", () => {
    expect(actionIds).toHaveLength(20);
    expect(actionIds).toHaveLength(requestNames.length);
    const { defs } = harness();
    expect(Object.keys(defs).sort()).toEqual([...actionIds].sort());
  });

  for (const c of withDefaults) {
    it(`maps ${c.id} to ${c.type} with no options`, async () => {
      const { vendor, defs } = harness();
      await invoke(defs, c.id, c.options);
      expect(vendor.calls).toEqual([{ type: c.type, data: c.data }]);
    });
  }

  for (const c of withOptions) {
    it(`maps ${c.id} to ${c.type} from the options`, async () => {
      const { vendor, defs } = harness();
      await invoke(defs, c.id, c.options);
      expect(vendor.calls).toEqual([{ type: c.type, data: c.data }]);
    });
  }

  it("does not log a rejected command as accepted", async () => {
    const { vendor, logs, defs } = harness();
    vendor.replies["ClockStart"] = { result: "rejected", reason: "clock.running" };
    await invoke(defs, "clock_start");
    expect(vendor.calls).toEqual([{ type: "ClockStart", data: {} }]);
    expect(logs).toEqual([{ level: "warn", message: "ClockStart rejected: clock.running" }]);
  });

  it("logs a transport failure instead of ignoring it", async () => {
    const { vendor, logs, defs } = harness();
    vendor.throwOn.add("ClockStop");
    await invoke(defs, "clock_stop");
    expect(logs).toEqual([{ level: "warn", message: "ClockStop error: transport" }]);
  });

  it("does not log an accepted command", async () => {
    const { logs, defs } = harness();
    await invoke(defs, "clock_toggle");
    expect(logs).toEqual([]);
  });
});

describe("requestFor", () => {
  it("returns undefined for an unknown action id", () => {
    expect(requestFor("does_not_exist", {})).toBeUndefined();
    expect(requestFor("", {})).toBeUndefined();
  });
});
