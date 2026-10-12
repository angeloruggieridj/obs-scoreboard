// SPDX-License-Identifier: GPL-2.0-or-later
import type { JsonObject } from "./vendor";
import { requests, type PenaltyPhaseIn, type StatName, type Team } from "./protocol";

// The settings the property inspector writes. sdpi text fields store strings, so every numeric
// setting accepts a string too and is coerced in `command()` (finding 5).
export interface ActionSettings {
  team?: string;
  stat?: string;
  delta?: number | string | null;
  minutes?: number | string | null;
  seconds?: number | string | null;
  player?: string;
  name?: string;
  id?: number | string | null;
  slot?: number | string | null;
  tenths?: number | string | null;
  option?: string;
  sport?: string;
  confirmed?: boolean | string | null;
}

const STATS: string[] = ["score", "shots", "fouls", "fouls2"];

// PenaltyAdd phases per Option: the protocol sends one phase per entry (2+2 = two phases).
const PENALTY_PRESETS: Record<string, number[]> = {
  "2": [1200],
  "5": [3000],
  "10": [6000],
  "2+2": [1200, 1200],
  "2+5": [1200, 3000],
  "2+10": [1200, 6000],
};

function team(s: ActionSettings): Team {
  return s.team === "away" ? "away" : "home";
}

function stat(s: ActionSettings): StatName {
  return STATS.includes(s.stat ?? "") ? (s.stat as StatName) : "shots";
}

// Coerce a settings value to a finite number. `undefined` means "rejected": the value is present
// but not numeric (NaN), so the command is not sent instead of reaching the C++ codec as a string.
function number(v: number | string | null | undefined, fallback: number): number | undefined {
  if (v === undefined || v === null || v === "") return fallback;
  const n = Number(v);
  return Number.isFinite(n) ? n : undefined;
}

function flag(v: boolean | string | null | undefined, fallback: boolean): boolean {
  if (v === undefined || v === null || v === "") return fallback;
  if (typeof v === "boolean") return v;
  return v === "true" || v === "1";
}

// Minutes/seconds (both signed) to tenths of a second, the unit of ClockAdjust/ClockSet.
function clockTenths(s: ActionSettings): number | undefined {
  const minutes = number(s.minutes, 0);
  const seconds = number(s.seconds, 0);
  if (minutes === undefined || seconds === undefined) return undefined;
  return Math.round((minutes * 60 + seconds) * 10);
}

function penaltyPhases(s: ActionSettings): PenaltyPhaseIn[] | undefined {
  // `tenths` is a direct override (advanced); otherwise Option picks a preset.
  if (s.tenths !== undefined && s.tenths !== null && s.tenths !== "") {
    const tenths = number(s.tenths, 1200);
    return tenths === undefined ? undefined : [{ tenths }];
  }
  const preset = PENALTY_PRESETS[s.option ?? "2"];
  if (!preset) return undefined;
  return preset.map((tenths) => ({ tenths }));
}

// Pure action -> request mapping; tested without the SDK.
export function command(
  actionId: string,
  s: ActionSettings,
): { type: string; data: JsonObject } | undefined {
  switch (actionId) {
    case "clock-toggle": return { type: "ClockToggle", data: requests.clockToggle() };
    case "clock-start": return { type: "ClockStart", data: requests.clockStart() };
    case "clock-stop": return { type: "ClockStop", data: requests.clockStop() };
    case "clock-reset": return { type: "ClockReset", data: requests.clockReset() };
    case "clock-adjust": {
      const tenths = clockTenths(s);
      if (tenths === undefined) return undefined;
      return { type: "ClockAdjust", data: requests.clockAdjust(tenths) };
    }
    case "clock-set": {
      const tenths = clockTenths(s);
      if (tenths === undefined) return undefined;
      return { type: "ClockSet", data: requests.clockSet(tenths) };
    }
    case "period-next": return { type: "PeriodNext", data: requests.periodNext(flag(s.confirmed, false)) };
    case "period-prev": return { type: "PeriodPrev", data: requests.periodPrev(flag(s.confirmed, false)) };
    case "score": {
      const delta = number(s.delta, 1);
      if (delta === undefined) return undefined;
      return { type: "AddStat", data: requests.addStat(team(s), "score", delta) };
    }
    case "stat": {
      const delta = number(s.delta, 1);
      if (delta === undefined) return undefined;
      return { type: "AddStat", data: requests.addStat(team(s), stat(s), delta) };
    }
    case "set-team-name":
      return { type: "SetTeamName", data: requests.setTeamName(team(s), s.name ?? s.player ?? "") };
    case "stoppage-set": {
      const minutes = number(s.minutes, 0);
      if (minutes === undefined) return undefined;
      return { type: "StoppageSet", data: requests.stoppageSet(minutes) };
    }
    case "stoppage-add": {
      const minutes = number(s.minutes, 0);
      if (minutes === undefined) return undefined;
      return { type: "StoppageAdd", data: requests.stoppageAdd(minutes) };
    }
    case "penalty-add": {
      const phases = penaltyPhases(s);
      if (!phases) return undefined;
      // `PenaltyPhaseIn` is an interface, so it lacks the implicit index signature that `JsonObject`
      // requires: cast through `unknown` (the value is a plain JSON object at runtime).
      const data = requests.penaltyAdd(team(s), s.player ?? "", phases);
      return { type: "PenaltyAdd", data: data as unknown as JsonObject };
    }
    case "penalty-edit": {
      const id = number(s.id, 0);
      const tenths = number(s.tenths, 0);
      if (id === undefined || tenths === undefined) return undefined;
      return { type: "PenaltyEdit", data: requests.penaltyEdit(id, tenths) };
    }
    case "penalty-cancel": {
      const id = number(s.id, 0);
      if (id === undefined) return undefined;
      return { type: "PenaltyCancel", data: requests.penaltyCancel(id) };
    }
    case "penalty-cancel-active": {
      const slot = number(s.slot, 0);
      if (slot === undefined) return undefined;
      return { type: "PenaltyCancelActive", data: requests.penaltyCancelActive(team(s), slot) };
    }
    case "end-match": return { type: "EndMatch", data: requests.endMatch() };
    case "new-match": return { type: "NewMatch", data: requests.newMatch(s.sport ?? "ice_hockey", flag(s.confirmed, false)) };
    default: return undefined;
  }
}
