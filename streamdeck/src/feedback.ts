// SPDX-License-Identifier: GPL-2.0-or-later
import type { VendorClient } from "./vendor";
import type { ScoreChangedEvent, StateChangedEvent } from "./protocol";

export type FeedbackTarget = "clock" | "homeScore" | "awayScore" | "period" | "strength";

// Turns the vendor events into the text a key shows. Pure: no Stream Deck types here.
export class FeedbackHub {
  private clock = "";
  private homeScore = "0";
  private awayScore = "0";
  private period = "";
  private strength = "";
  private running = false;

  constructor(private readonly vendor: VendorClient) {}

  start(): void {
    this.vendor.on("StateChanged", (data) => this.applyStateChanged(data as StateChangedEvent));
    this.vendor.on("ScoreChanged", (data) => this.applyScoreChanged(data as ScoreChangedEvent));
    // No PeriodEnded handler: its label is the period that just ended. The current period comes
    // from StateChanged["period"] (item 10).
  }

  title(target: FeedbackTarget): string {
    switch (target) {
      case "clock": return this.clock;
      case "homeScore": return this.homeScore;
      case "awayScore": return this.awayScore;
      case "period": return this.period;
      case "strength": return this.strength;
    }
  }

  clockRunning(): boolean { return this.running; }

  private applyStateChanged(event: StateChangedEvent): void {
    const changed = event.changed ?? {};
    if ("clock" in changed) this.clock = changed.clock;
    if ("home.score" in changed) this.homeScore = changed["home.score"];
    if ("away.score" in changed) this.awayScore = changed["away.score"];
    if ("period" in changed) this.period = changed.period;
    if ("strength" in changed) this.strength = changed.strength;
    this.running = event.clockRunning;
  }

  private applyScoreChanged(event: ScoreChangedEvent): void {
    if (event.team === "home") this.homeScore = String(event.score);
    else this.awayScore = String(event.score);
  }
}
