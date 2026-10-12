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
    // A stopped clock emits no StateChanged, so without this the keys would stay blank or stale
    // until the first change (finding 3). On every connect/reconnect the client fetches the state
    // and tells us to seed from it.
    this.vendor.onSynced(() => this.sync());
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

  // Seeds every key from the initial GetFields/GetState the client fetched on connect. A field the
  // vendor did not return keeps its current value; the clock-running flag comes from GetState.
  sync(): void {
    const clock = this.vendor.getField("clock");
    const homeScore = this.vendor.getField("home.score");
    const awayScore = this.vendor.getField("away.score");
    const period = this.vendor.getField("period");
    const strength = this.vendor.getField("strength");
    if (clock !== undefined) this.clock = clock;
    if (homeScore !== undefined) this.homeScore = homeScore;
    if (awayScore !== undefined) this.awayScore = awayScore;
    if (period !== undefined) this.period = period;
    if (strength !== undefined) this.strength = strength;
    const state = this.vendor.getState();
    if (state) this.running = state.clockRunning;
  }

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
