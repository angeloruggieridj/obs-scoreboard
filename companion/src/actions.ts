// SPDX-License-Identifier: MIT
import type {
  CompanionActionDefinition,
  CompanionActionEvent,
  CompanionInputFieldCheckbox,
  CompanionInputFieldDropdown,
  CompanionInputFieldNumber,
  CompanionInputFieldTextInput,
  LogLevel,
  SomeCompanionActionInputField,
} from "@companion-module/base";
import { requests, type StatName, type Team } from "./protocol";
import type { JsonObject, VendorClient } from "./vendor";

// The user-editable options of every action, with the same shape as the Stream Deck settings
// (Task 7.2.3): the pure mapping below reads them with sensible defaults.
export type ActionOptions = {
  team?: Team;
  stat?: StatName;
  delta?: number;
  player?: string;
  tenths?: number;
  sport?: string;
  confirmed?: boolean;
};

export type LogFn = (level: LogLevel, message: string) => void;

// The action ids: the snake_case of `requestNames`, one action per protocol request.
export const actionIds = Object.freeze([
  "clock_toggle",
  "clock_start",
  "clock_stop",
  "clock_reset",
  "clock_adjust",
  "clock_set",
  "period_next",
  "period_prev",
  "add_stat",
  "set_team_name",
  "penalty_add",
  "penalty_edit",
  "penalty_cancel",
  "penalty_cancel_active",
  "stoppage_set",
  "stoppage_add",
  "end_match",
  "new_match",
  "get_state",
  "get_fields",
]);

// Pure action -> request mapping (mirrors streamdeck/src/settings.ts). `undefined` for an id the
// plugin does not know.
export function requestFor(
  actionId: string,
  o: ActionOptions,
): { type: string; data: JsonObject } | undefined {
  const team = o.team ?? "home";
  switch (actionId) {
    case "clock_toggle":
      return { type: "ClockToggle", data: requests.clockToggle() };
    case "clock_start":
      return { type: "ClockStart", data: requests.clockStart() };
    case "clock_stop":
      return { type: "ClockStop", data: requests.clockStop() };
    case "clock_reset":
      return { type: "ClockReset", data: requests.clockReset() };
    case "clock_adjust":
      return { type: "ClockAdjust", data: requests.clockAdjust(o.delta ?? 0) };
    case "clock_set":
      return { type: "ClockSet", data: requests.clockSet(o.tenths ?? 0) };
    case "period_next":
      return { type: "PeriodNext", data: requests.periodNext(o.confirmed ?? false) };
    case "period_prev":
      return { type: "PeriodPrev", data: requests.periodPrev(o.confirmed ?? false) };
    case "add_stat":
      return { type: "AddStat", data: requests.addStat(team, o.stat ?? "score", o.delta ?? 1) };
    case "set_team_name":
      return { type: "SetTeamName", data: requests.setTeamName(team, o.player ?? "") };
    case "penalty_add": {
      // `PenaltyPhaseIn` is an interface, so it lacks the implicit index signature `JsonObject`
      // requires: cast through `unknown` (the value is a plain JSON object at runtime).
      const data = requests.penaltyAdd(team, o.player ?? "", [{ tenths: o.tenths ?? 1200 }]);
      return { type: "PenaltyAdd", data: data as unknown as JsonObject };
    }
    case "penalty_edit":
      return { type: "PenaltyEdit", data: requests.penaltyEdit(o.tenths ?? 0, o.delta ?? 0) };
    case "penalty_cancel":
      return { type: "PenaltyCancel", data: requests.penaltyCancel(o.tenths ?? 0) };
    case "penalty_cancel_active":
      return { type: "PenaltyCancelActive", data: requests.penaltyCancelActive(team, o.delta ?? 0) };
    case "stoppage_set":
      return { type: "StoppageSet", data: requests.stoppageSet(o.delta ?? 0) };
    case "stoppage_add":
      return { type: "StoppageAdd", data: requests.stoppageAdd(o.delta ?? 0) };
    case "end_match":
      return { type: "EndMatch", data: requests.endMatch() };
    case "new_match":
      return { type: "NewMatch", data: requests.newMatch(o.sport ?? "ice_hockey", o.confirmed ?? false) };
    case "get_state":
      return { type: "GetState", data: requests.getState() };
    case "get_fields":
      return { type: "GetFields", data: requests.getFields() };
    default:
      return undefined;
  }
}

const NUMBER_MAX = 2147483647;
const SPORTS = [
  "ice_hockey",
  "basketball",
  "soccer",
  "futsal",
  "american_football",
  "lacrosse",
  "rugby_union",
  "rugby_sevens",
  "field_hockey",
  "water_polo",
  "handball",
  "rink_hockey",
  "floorball",
  "generic",
] as const;

// Fresh option fields (the input model must be unique per action).
const teamField = (): CompanionInputFieldDropdown => ({
  id: "team",
  type: "dropdown",
  label: "Team",
  choices: [
    { id: "home", label: "Home" },
    { id: "away", label: "Away" },
  ],
  default: "home",
});

const statField = (): CompanionInputFieldDropdown => ({
  id: "stat",
  type: "dropdown",
  label: "Stat",
  choices: [
    { id: "score", label: "Score" },
    { id: "shots", label: "Shots" },
    { id: "fouls", label: "Fouls" },
    { id: "fouls2", label: "Second fouls" },
  ],
  default: "score",
});

const deltaField = (label: string, def = 1): CompanionInputFieldNumber => ({
  id: "delta",
  type: "number",
  label,
  default: def,
  min: -NUMBER_MAX,
  max: NUMBER_MAX,
  step: 1,
});

const tenthsField = (label: string, def = 0): CompanionInputFieldNumber => ({
  id: "tenths",
  type: "number",
  label,
  default: def,
  min: 0,
  max: NUMBER_MAX,
  step: 1,
});

const playerField = (label = "Player"): CompanionInputFieldTextInput => ({
  id: "player",
  type: "textinput",
  label,
  default: "",
});

const sportField = (): CompanionInputFieldDropdown => ({
  id: "sport",
  type: "dropdown",
  label: "Sport",
  choices: SPORTS.map((id) => ({ id, label: id })),
  default: "ice_hockey",
});

const confirmedField = (): CompanionInputFieldCheckbox => ({
  id: "confirmed",
  type: "checkbox",
  label: "Confirm now",
  default: false,
});

// Builds one action definition per protocol request. Each callback runs the pure mapping and
// forwards the request to the vendor; a `rejected`/`error` envelope is logged, never ignored.
export function buildActionDefinitions(
  vendor: VendorClient,
  log: LogFn,
): Record<string, CompanionActionDefinition> {
  const action = (
    name: string,
    options: SomeCompanionActionInputField[],
    actionId: string,
  ): CompanionActionDefinition => ({
    name,
    options,
    callback: async (event: CompanionActionEvent) => {
      const call = requestFor(actionId, event.options as ActionOptions);
      if (!call) return;
      const result = await vendor.request(call.type, call.data);
      if (result.result === "rejected") log("warn", `${call.type} rejected: ${result.reason ?? ""}`);
      else if (result.result === "error") log("warn", `${call.type} error: ${result.error ?? ""}`);
    },
  });

  return {
    clock_toggle: action("Clock: toggle", [], "clock_toggle"),
    clock_start: action("Clock: start", [], "clock_start"),
    clock_stop: action("Clock: stop", [], "clock_stop"),
    clock_reset: action("Clock: reset", [], "clock_reset"),
    clock_adjust: action("Clock: adjust (tenths)", [deltaField("Delta in tenths", 0)], "clock_adjust"),
    clock_set: action("Clock: set (tenths)", [tenthsField("Tenths from 0:00")], "clock_set"),
    period_next: action("Period: next", [confirmedField()], "period_next"),
    period_prev: action("Period: previous", [confirmedField()], "period_prev"),
    add_stat: action("Counter: add", [teamField(), statField(), deltaField("Delta")], "add_stat"),
    set_team_name: action("Team: set name", [teamField(), playerField("Name")], "set_team_name"),
    penalty_add: action(
      "Penalty: add",
      [teamField(), playerField(), tenthsField("Duration in tenths", 1200)],
      "penalty_add",
    ),
    penalty_edit: action(
      "Penalty: edit",
      [tenthsField("Penalty id"), deltaField("Remaining tenths", 0)],
      "penalty_edit",
    ),
    penalty_cancel: action("Penalty: cancel", [tenthsField("Penalty id")], "penalty_cancel"),
    penalty_cancel_active: action(
      "Penalty: cancel active",
      [teamField(), deltaField("Slot", 0)],
      "penalty_cancel_active",
    ),
    stoppage_set: action("Stoppage: set (minutes)", [deltaField("Minutes", 0)], "stoppage_set"),
    stoppage_add: action("Stoppage: add (minutes)", [deltaField("Minutes", 0)], "stoppage_add"),
    end_match: action("Match: end", [], "end_match"),
    new_match: action("Match: new", [sportField(), confirmedField()], "new_match"),
    get_state: action("State: get", [], "get_state"),
    get_fields: action("Fields: get", [], "get_fields"),
  };
}
