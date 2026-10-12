// SPDX-License-Identifier: MIT
import type { CompanionVariableDefinition } from "@companion-module/base";

// The 28 field keys of the wire protocol, in the same order as `src/core/Fields.cpp`. The module
// exposes one variable per key (`.` becomes `_`) plus `clock_running` (from the StateChanged
// payload, not a field).
export const fieldKeys = Object.freeze([
  "clock",
  "period",
  "home.name",
  "away.name",
  "home.score",
  "away.score",
  "home.shots",
  "away.shots",
  "home.fouls",
  "away.fouls",
  "home.fouls2",
  "away.fouls2",
  "home.penalty1.player",
  "home.penalty1.time",
  "home.penalty1.label",
  "home.penalty2.player",
  "home.penalty2.time",
  "home.penalty2.label",
  "away.penalty1.player",
  "away.penalty1.time",
  "away.penalty1.label",
  "away.penalty2.player",
  "away.penalty2.time",
  "away.penalty2.label",
  "strength",
  "stoppage",
  "stoppage.announced",
  "playtime",
]);

// The variable name of a field key: same snake_case mapping as `ModuleState.variables()`.
export const variableId = (key: string): string => key.replace(/\./g, "_");

export const variableIds = Object.freeze([...fieldKeys.map(variableId), "clock_running"]);

// One named definition per variable id, so Companion knows every variable the module publishes.
export function buildVariableDefinitions(): Record<string, CompanionVariableDefinition> {
  const out: Record<string, CompanionVariableDefinition> = {};
  for (const id of variableIds) out[id] = { name: id };
  return out;
}
