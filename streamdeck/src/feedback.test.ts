// SPDX-License-Identifier: GPL-2.0-or-later
import { describe, expect, it } from "vitest";
import { FeedbackHub } from "./feedback";
import { FakeVendor } from "./vendor";

// The hub is pure: it reads the fake vendor's events and returns the key text.
describe("FeedbackHub", () => {
  it("starts with empty text and a stopped clock", () => {
    const hub = new FeedbackHub(new FakeVendor());
    expect(hub.title("clock")).toBe("");
    expect(hub.title("homeScore")).toBe("0");
    expect(hub.title("awayScore")).toBe("0");
    expect(hub.title("period")).toBe("");
    expect(hub.title("strength")).toBe("");
    expect(hub.clockRunning()).toBe(false);
  });
  it("reads the clock, scores, period and strength from StateChanged", () => {
    const vendor = new FakeVendor();
    const hub = new FeedbackHub(vendor);
    hub.start();
    vendor.emit("StateChanged", {
      changed: {
        clock: "19:59",
        "home.score": "1",
        "away.score": "2",
        period: "2",
        strength: "5-4",
      },
      clockRunning: true,
      matchEnded: false,
    });
    expect(hub.title("clock")).toBe("19:59");
    expect(hub.title("homeScore")).toBe("1");
    expect(hub.title("awayScore")).toBe("2");
    expect(hub.title("period")).toBe("2");
    expect(hub.title("strength")).toBe("5-4");
    expect(hub.clockRunning()).toBe(true);
  });
  it("keeps a field whose text did not change", () => {
    const vendor = new FakeVendor();
    const hub = new FeedbackHub(vendor);
    hub.start();
    vendor.emit("StateChanged", {
      changed: { clock: "18:00", "home.score": "3" },
      clockRunning: false,
      matchEnded: false,
    });
    // Only the period changed: the clock and the score stay at their last value.
    vendor.emit("StateChanged", {
      changed: { period: "1" },
      clockRunning: false,
      matchEnded: false,
    });
    expect(hub.title("clock")).toBe("18:00");
    expect(hub.title("homeScore")).toBe("3");
    expect(hub.title("awayScore")).toBe("0");
    expect(hub.title("period")).toBe("1");
    expect(hub.clockRunning()).toBe(false);
  });
  it("updates the scoring team's score from ScoreChanged", () => {
    const vendor = new FakeVendor();
    const hub = new FeedbackHub(vendor);
    hub.start();
    vendor.emit("ScoreChanged", { team: "home", score: 4, homeScore: 4, awayScore: 2 });
    expect(hub.title("homeScore")).toBe("4");
    // Only the event's own team is applied: the other team's text comes from StateChanged.
    expect(hub.title("awayScore")).toBe("0");
    vendor.emit("ScoreChanged", { team: "away", score: 3, homeScore: 4, awayScore: 3 });
    expect(hub.title("homeScore")).toBe("4");
    expect(hub.title("awayScore")).toBe("3");
  });
  it("does not take the period from PeriodEnded (its label is the period that just ended)", () => {
    const vendor = new FakeVendor();
    const hub = new FeedbackHub(vendor);
    hub.start();
    vendor.emit("StateChanged", {
      changed: { period: "2" },
      clockRunning: false,
      matchEnded: false,
    });
    expect(hub.title("period")).toBe("2");
    // Period 1 just ended: its label must NOT become the current period.
    vendor.emit("PeriodEnded", { period: 1, label: "1" });
    expect(hub.title("period")).toBe("2");
  });
});
