// SPDX-License-Identifier: MIT
import type { PeriodEndedEvent, ScoreChangedEvent, StateChangedEvent } from "./protocol";

// The variable names (snake_case) whose value is a number: the scoreboard counters.
const NUMERIC_VARIABLES = new Set([
  "home_score",
  "away_score",
  "home_shots",
  "away_shots",
  "home_fouls",
  "away_fouls",
  "home_fouls2",
  "away_fouls2",
]);

export type FeedbackValue = string | boolean | number;

// The last known scoreboard, rebuilt from the vendor events only. Pure: no obs-websocket, no
// Companion types. Every field key is exposed as a snake_case variable (`.` -> `_`), plus
// `clock_running`. The current period is driven **only** by StateChanged["period"]: the label of
// PeriodEnded is the period that just ended, so applyPeriodEnded sets a separate flag instead.
export class ModuleState {
  private readonly fields: Record<string, string> = {};
  private running = false;
  private matchEnded = false;
  private periodEnded = false;

  applyStateChanged(event: StateChangedEvent): void {
    const changed = event.changed ?? {};
    const periodChanged = "period" in changed;
    for (const [key, value] of Object.entries(changed)) this.fields[key] = value;
    this.running = event.clockRunning;
    this.matchEnded = event.matchEnded;
    if (periodChanged) this.periodEnded = false; // a new period started
  }

  applyScoreChanged(event: ScoreChangedEvent): void {
    // The payload carries both scores, so keep the two in sync from it (not just `team`/`score`).
    this.fields["home.score"] = String(event.homeScore);
    this.fields["away.score"] = String(event.awayScore);
  }

  applyPeriodEnded(_event: PeriodEndedEvent): void {
    // The label of PeriodEnded is the period that just ended (item 12), not the current period.
    this.periodEnded = true;
  }

  variables(): Record<string, string | number> {
    const out: Record<string, string | number> = {};
    for (const [key, value] of Object.entries(this.fields)) {
      const name = key.replace(/\./g, "_");
      out[name] = NUMERIC_VARIABLES.has(name) ? toNumber(value) : value;
    }
    out["clock_running"] = this.running ? "true" : "false";
    return out;
  }

  feedbackValue(id: string): FeedbackValue {
    switch (id) {
      case "clock_running":
        return this.running;
      case "match_ended":
        return this.matchEnded;
      case "period_ended":
        return this.periodEnded;
      default: {
        const variables = this.variables();
        return id in variables ? variables[id] : "";
      }
    }
  }
}

function toNumber(value: string): string | number {
  if (value.trim() === "") return value;
  const n = Number(value);
  return Number.isFinite(n) ? n : value;
}
