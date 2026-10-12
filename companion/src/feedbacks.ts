// SPDX-License-Identifier: MIT
import type { CompanionFeedbackDefinition } from "@companion-module/base";
import type { ModuleState } from "./state";

// The feedback ids: the clock-running and match-ended booleans, then the live values clients put on
// a button (score, period, penalties). Every callback reads `ModuleState.feedbackValue(id)`.
export const feedbackIds = Object.freeze([
  "clock_running",
  "match_ended",
  "home_score",
  "away_score",
  "period",
  "penalties",
]);

// The penalty time fields (one per team and slot); a non-empty text means a penalty is shown.
const PENALTY_TIME_IDS = [
  "home_penalty1_time",
  "home_penalty2_time",
  "away_penalty1_time",
  "away_penalty2_time",
];

function penaltiesRunning(state: ModuleState): number {
  let count = 0;
  for (const id of PENALTY_TIME_IDS) if (String(state.feedbackValue(id)) !== "") count += 1;
  return count;
}

// Builds the feedback definitions over the shared ModuleState; each callback reads the live state.
export function buildFeedbackDefinitions(
  state: ModuleState,
): Record<string, CompanionFeedbackDefinition> {
  return {
    clock_running: {
      name: "Clock running",
      type: "boolean",
      defaultStyle: { bgcolor: 0x0b3d2e, color: 0xffffff },
      options: [],
      callback: () => Boolean(state.feedbackValue("clock_running")),
    },
    match_ended: {
      name: "Match ended",
      type: "boolean",
      defaultStyle: { bgcolor: 0x5a1a1a, color: 0xffffff },
      options: [],
      callback: () => Boolean(state.feedbackValue("match_ended")),
    },
    home_score: {
      name: "Home score",
      type: "value",
      options: [],
      callback: () => state.feedbackValue("home_score"),
    },
    away_score: {
      name: "Away score",
      type: "value",
      options: [],
      callback: () => state.feedbackValue("away_score"),
    },
    period: {
      name: "Period",
      type: "value",
      options: [],
      callback: () => state.feedbackValue("period"),
    },
    penalties: {
      name: "Penalties running",
      type: "value",
      options: [],
      callback: () => penaltiesRunning(state),
    },
  };
}
