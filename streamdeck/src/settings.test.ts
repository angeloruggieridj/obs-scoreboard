// SPDX-License-Identifier: GPL-2.0-or-later
import { describe, expect, it } from "vitest";
import { command } from "./settings";

// One test per action id: the pure action -> request mapping, no SDK involved.
describe("command", () => {
  it("maps clock-toggle", () => {
    expect(command("clock-toggle", {})).toEqual({ type: "ClockToggle", data: {} });
  });
  it("maps clock-start", () => {
    expect(command("clock-start", {})).toEqual({ type: "ClockStart", data: {} });
  });
  it("maps clock-stop", () => {
    expect(command("clock-stop", {})).toEqual({ type: "ClockStop", data: {} });
  });
  it("maps clock-reset", () => {
    expect(command("clock-reset", {})).toEqual({ type: "ClockReset", data: {} });
  });
  it("maps clock-adjust to ClockAdjust{tenths} and defaults the delta to 0", () => {
    expect(command("clock-adjust", { delta: -600 })).toEqual({
      type: "ClockAdjust",
      data: { deltaTenths: -600 },
    });
    expect(command("clock-adjust", {})).toEqual({ type: "ClockAdjust", data: { deltaTenths: 0 } });
  });
  it("maps clock-set to ClockSet{tenths} and defaults to 0", () => {
    expect(command("clock-set", { tenths: 600 })).toEqual({ type: "ClockSet", data: { tenths: 600 } });
    expect(command("clock-set", {})).toEqual({ type: "ClockSet", data: { tenths: 0 } });
  });
  it("maps period-next and carries the confirmation flag", () => {
    expect(command("period-next", {})).toEqual({ type: "PeriodNext", data: { confirmed: false } });
    expect(command("period-next", { confirmed: true })).toEqual({
      type: "PeriodNext",
      data: { confirmed: true },
    });
  });
  it("maps period-prev and carries the confirmation flag", () => {
    expect(command("period-prev", {})).toEqual({ type: "PeriodPrev", data: { confirmed: false } });
    expect(command("period-prev", { confirmed: true })).toEqual({
      type: "PeriodPrev",
      data: { confirmed: true },
    });
  });
  it("maps score to AddStat{score} with a +1 default and the home team default", () => {
    expect(command("score", { team: "away", delta: 2 })).toEqual({
      type: "AddStat",
      data: { team: "away", stat: "score", delta: 2 },
    });
    expect(command("score", {})).toEqual({
      type: "AddStat",
      data: { team: "home", stat: "score", delta: 1 },
    });
  });
  it("maps stat to AddStat{stat} with a shots/+1 default", () => {
    expect(command("stat", { team: "away", stat: "fouls2", delta: -1 })).toEqual({
      type: "AddStat",
      data: { team: "away", stat: "fouls2", delta: -1 },
    });
    expect(command("stat", {})).toEqual({
      type: "AddStat",
      data: { team: "home", stat: "shots", delta: 1 },
    });
  });
  it("maps set-team-name to SetTeamName with an empty default name", () => {
    expect(command("set-team-name", { team: "away", player: "Ospiti" })).toEqual({
      type: "SetTeamName",
      data: { team: "away", name: "Ospiti" },
    });
    expect(command("set-team-name", {})).toEqual({
      type: "SetTeamName",
      data: { team: "home", name: "" },
    });
  });
  it("maps stoppage-set to StoppageSet{minutes}", () => {
    expect(command("stoppage-set", { delta: 3 })).toEqual({ type: "StoppageSet", data: { minutes: 3 } });
    expect(command("stoppage-set", {})).toEqual({ type: "StoppageSet", data: { minutes: 0 } });
  });
  it("maps stoppage-add to StoppageAdd{minutes} and keeps the sign", () => {
    expect(command("stoppage-add", { delta: -2 })).toEqual({
      type: "StoppageAdd",
      data: { minutes: -2 },
    });
    expect(command("stoppage-add", {})).toEqual({ type: "StoppageAdd", data: { minutes: 0 } });
  });
  it("maps penalty-add to PenaltyAdd with one 2:00 phase by default", () => {
    expect(command("penalty-add", { team: "away", player: "24", tenths: 1200 })).toEqual({
      type: "PenaltyAdd",
      data: { team: "away", player: "24", phases: [{ tenths: 1200 }] },
    });
    expect(command("penalty-add", {})).toEqual({
      type: "PenaltyAdd",
      data: { team: "home", player: "", phases: [{ tenths: 1200 }] },
    });
  });
  it("maps penalty-edit to PenaltyEdit{id,tenths}", () => {
    expect(command("penalty-edit", { tenths: 7, delta: 600 })).toEqual({
      type: "PenaltyEdit",
      data: { id: 7, tenths: 600 },
    });
    expect(command("penalty-edit", {})).toEqual({ type: "PenaltyEdit", data: { id: 0, tenths: 0 } });
  });
  it("maps penalty-cancel to PenaltyCancel{id}", () => {
    expect(command("penalty-cancel", { tenths: 3 })).toEqual({ type: "PenaltyCancel", data: { id: 3 } });
    expect(command("penalty-cancel", {})).toEqual({ type: "PenaltyCancel", data: { id: 0 } });
  });
  it("maps penalty-cancel-active to PenaltyCancelActive{team,slot}", () => {
    expect(command("penalty-cancel-active", { team: "away", delta: 1 })).toEqual({
      type: "PenaltyCancelActive",
      data: { team: "away", slot: 1 },
    });
    expect(command("penalty-cancel-active", {})).toEqual({
      type: "PenaltyCancelActive",
      data: { team: "home", slot: 0 },
    });
  });
  it("maps end-match", () => {
    expect(command("end-match", {})).toEqual({ type: "EndMatch", data: {} });
  });
  it("maps new-match to NewMatch{sport,confirmed} with the ice_hockey default", () => {
    expect(command("new-match", { sport: "basketball", confirmed: true })).toEqual({
      type: "NewMatch",
      data: { sport: "basketball", confirmed: true },
    });
    expect(command("new-match", {})).toEqual({
      type: "NewMatch",
      data: { sport: "ice_hockey", confirmed: false },
    });
  });
  it("returns undefined for an unknown action id", () => {
    expect(command("does-not-exist", {})).toBeUndefined();
    expect(command("", {})).toBeUndefined();
  });
});
