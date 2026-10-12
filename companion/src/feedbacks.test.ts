// SPDX-License-Identifier: MIT
import { describe, expect, it } from "vitest";
import { buildFeedbackDefinitions, feedbackIds } from "./feedbacks";
import { ModuleState } from "./state";

// The definitions carry the real SDK callback signature (with required arguments); the module state
// is the only input these feedbacks need, so call them directly with no arguments.
type Callable = { callback: () => unknown; type: string };
const asDefs = (state: ModuleState): Record<string, Callable> =>
  buildFeedbackDefinitions(state) as unknown as Record<string, Callable>;

const stateChanged = (
  changed: Record<string, string>,
  clockRunning = false,
  matchEnded = false,
) => ({ changed, clockRunning, matchEnded });

describe("feedbacks", () => {
  it("declares the clock-running, score, period and penalties feedbacks", () => {
    expect(feedbackIds).toContain("clock_running");
    expect(feedbackIds).toContain("home_score");
    expect(feedbackIds).toContain("away_score");
    expect(feedbackIds).toContain("period");
    expect(feedbackIds).toContain("penalties");
  });

  it("reads the live state through ModuleState.feedbackValue", () => {
    const state = new ModuleState();
    const defs = asDefs(state);
    state.applyStateChanged(
      stateChanged({
        clock: "19:59",
        period: "2",
        "home.score": "3",
        "away.score": "1",
        "home.penalty1.time": "2:00",
        "away.penalty1.time": "5:00",
      }, true),
    );
    expect(defs["clock_running"].callback()).toBe(true);
    expect(defs["home_score"].callback()).toBe(3);
    expect(defs["away_score"].callback()).toBe(1);
    expect(defs["period"].callback()).toBe("2");
    expect(defs["penalties"].callback()).toBe(2);
  });

  it("is false and empty before any event, then follows the state", () => {
    const state = new ModuleState();
    const defs = asDefs(state);
    expect(defs["clock_running"].callback()).toBe(false);
    expect(defs["period"].callback()).toBe("");
    expect(defs["penalties"].callback()).toBe(0);
    state.applyStateChanged(stateChanged({}, true));
    expect(defs["clock_running"].callback()).toBe(true);
  });

  it("marks the boolean feedbacks as boolean and the values as value", () => {
    const defs = asDefs(new ModuleState());
    expect(defs["clock_running"].type).toBe("boolean");
    expect(defs["match_ended"].type).toBe("boolean");
    expect(defs["home_score"].type).toBe("value");
    expect(defs["period"].type).toBe("value");
    expect(defs["penalties"].type).toBe("value");
  });
});
