// SPDX-License-Identifier: GPL-2.0-or-later
import type { JsonObject } from "./vendor";
import { requests, type StatName, type Team } from "./protocol";

export interface ActionSettings {
  team?: Team; stat?: StatName; delta?: number; player?: string; tenths?: number;
  sport?: string; confirmed?: boolean;
}

// Pure action -> request mapping; tested without the SDK.
export function command(
  actionId: string,
  s: ActionSettings,
): { type: string; data: JsonObject } | undefined {
  const team = s.team ?? "home";
  switch (actionId) {
    case "clock-toggle": return { type: "ClockToggle", data: requests.clockToggle() };
    case "clock-start": return { type: "ClockStart", data: requests.clockStart() };
    case "clock-stop": return { type: "ClockStop", data: requests.clockStop() };
    case "clock-reset": return { type: "ClockReset", data: requests.clockReset() };
    case "clock-adjust": return { type: "ClockAdjust", data: requests.clockAdjust(s.delta ?? 0) };
    case "clock-set": return { type: "ClockSet", data: requests.clockSet(s.tenths ?? 0) };
    case "period-next": return { type: "PeriodNext", data: requests.periodNext(s.confirmed ?? false) };
    case "period-prev": return { type: "PeriodPrev", data: requests.periodPrev(s.confirmed ?? false) };
    case "score": return { type: "AddStat", data: requests.addStat(team, "score", s.delta ?? 1) };
    case "stat": return { type: "AddStat", data: requests.addStat(team, s.stat ?? "shots", s.delta ?? 1) };
    case "set-team-name": return { type: "SetTeamName", data: requests.setTeamName(team, s.player ?? "") };
    case "stoppage-set": return { type: "StoppageSet", data: requests.stoppageSet(s.delta ?? 0) };
    case "stoppage-add": return { type: "StoppageAdd", data: requests.stoppageAdd(s.delta ?? 0) };
    case "penalty-add": {
      // `PenaltyPhaseIn` is an interface, so it lacks the implicit index signature that `JsonObject`
      // requires: cast through `unknown` (the value is a plain JSON object at runtime).
      const data = requests.penaltyAdd(team, s.player ?? "", [{ tenths: s.tenths ?? 1200 }]);
      return { type: "PenaltyAdd", data: data as unknown as JsonObject };
    }
    case "penalty-edit": return { type: "PenaltyEdit", data: requests.penaltyEdit(s.tenths ?? 0, s.delta ?? 0) };
    case "penalty-cancel": return { type: "PenaltyCancel", data: requests.penaltyCancel(s.tenths ?? 0) };
    case "penalty-cancel-active": return { type: "PenaltyCancelActive", data: requests.penaltyCancelActive(team, s.delta ?? 0) };
    case "end-match": return { type: "EndMatch", data: requests.endMatch() };
    case "new-match": return { type: "NewMatch", data: requests.newMatch(s.sport ?? "ice_hockey", s.confirmed ?? false) };
    default: return undefined;
  }
}
