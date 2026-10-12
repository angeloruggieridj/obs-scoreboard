// SPDX-License-Identifier: MIT
import { describe, expect, it } from "vitest";
import { ModuleState } from "./state";
import type { PeriodEndedEvent, ScoreChangedEvent, StateChangedEvent } from "./protocol";

const stateChanged = (
  changed: Record<string, string>,
  clockRunning = false,
  matchEnded = false,
): StateChangedEvent => ({ changed, clockRunning, matchEnded });

describe("ModuleState", () => {
  it("keeps the last known fields and applies only the changed ones", () => {
    const state = new ModuleState();
    state.applyStateChanged(
      stateChanged({ clock: "20:00", "home.name": "HOME", "period": "1" }, false, false),
    );
    expect(state.variables()["clock"]).toBe("20:00");
    expect(state.variables()["home_name"]).toBe("HOME");

    // Only the clock changed: the name and period must stay.
    state.applyStateChanged(stateChanged({ clock: "19:59" }, true, false));
    const vars = state.variables();
    expect(vars["clock"]).toBe("19:59");
    expect(vars["home_name"]).toBe("HOME");
    expect(vars["period"]).toBe("1");
    expect(vars["clock_running"]).toBe("true");
  });

  it("maps every field key to a snake_case variable and keeps counters numeric", () => {
    const state = new ModuleState();
    state.applyStateChanged(
      stateChanged({
        "home.score": "3",
        "away.score": "1",
        "home.shots": "7",
        "home.fouls2": "2",
        "home.penalty1.player": "24",
        "home.penalty1.time": "2:00",
        "away.penalty2.player": "9",
        "stoppage": "+3",
        "playtime": "05:00",
      }),
    );
    const vars = state.variables();
    expect(vars["home_score"]).toBe(3);
    expect(vars["away_score"]).toBe(1);
    expect(vars["home_shots"]).toBe(7);
    expect(vars["home_fouls2"]).toBe(2);
    expect(vars["home_penalty1_player"]).toBe("24");
    expect(vars["home_penalty1_time"]).toBe("2:00");
    expect(vars["away_penalty2_player"]).toBe("9");
    expect(vars["stoppage"]).toBe("+3");
    expect(vars["playtime"]).toBe("05:00");
  });

  it("takes the current period only from StateChanged, never from PeriodEnded", () => {
    const state = new ModuleState();
    state.applyStateChanged(stateChanged({ period: "1" }));
    const ended: PeriodEndedEvent = { period: 1, label: "1" };
    state.applyPeriodEnded(ended);
    // The label of PeriodEnded is the period that just ended: the current period is unchanged.
    expect(state.variables()["period"]).toBe("1");
    expect(state.feedbackValue("period_ended")).toBe(true);

    // A real period change clears the ended flag and moves the current period.
    state.applyStateChanged(stateChanged({ period: "2" }));
    expect(state.variables()["period"]).toBe("2");
    expect(state.feedbackValue("period_ended")).toBe(false);
  });

  it("applies ScoreChanged to the matching team and exposes a clock flag", () => {
    const state = new ModuleState();
    const score: ScoreChangedEvent = { team: "away", score: 4, homeScore: 2, awayScore: 4 };
    state.applyScoreChanged(score);
    expect(state.variables()["away_score"]).toBe(4);
    expect(state.variables()["home_score"]).toBe(2);
    expect(state.feedbackValue("clock_running")).toBe(false);
    state.applyStateChanged(stateChanged({}, true, false));
    expect(state.feedbackValue("clock_running")).toBe(true);
  });

  it("reads any variable through feedbackValue and is empty for unknown ids", () => {
    const state = new ModuleState();
    state.applyStateChanged(stateChanged({ "home.name": "HOME" }, false, true));
    expect(state.feedbackValue("home_name")).toBe("HOME");
    expect(state.feedbackValue("match_ended")).toBe(true);
    expect(state.feedbackValue("does_not_exist")).toBe("");
  });
});
