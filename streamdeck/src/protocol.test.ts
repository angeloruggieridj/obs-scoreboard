// SPDX-License-Identifier: GPL-2.0-or-later
import { existsSync, readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";
import { requests, requestNames } from "./protocol";

describe("request builders", () => {
  it("builds the empty-body commands", () => {
    expect(requests.clockToggle()).toEqual({});
    expect(requests.clockStart()).toEqual({});
    expect(requests.clockStop()).toEqual({});
    expect(requests.clockReset()).toEqual({});
    expect(requests.endMatch()).toEqual({});
    expect(requests.getState()).toEqual({});
    expect(requests.getFields()).toEqual({});
  });
  it("builds the clock and period requests", () => {
    expect(requests.clockAdjust(-600)).toEqual({ deltaTenths: -600 });
    expect(requests.clockSet(600)).toEqual({ tenths: 600 });
    expect(requests.periodNext()).toEqual({ confirmed: false });
    expect(requests.periodNext(true)).toEqual({ confirmed: true });
    expect(requests.periodPrev(true)).toEqual({ confirmed: true });
  });
  it("builds the counters and names", () => {
    expect(requests.addStat("home", "score", 2)).toEqual({ team: "home", stat: "score", delta: 2 });
    expect(requests.setTeamName("away", "Ospiti")).toEqual({ team: "away", name: "Ospiti" });
  });
  it("builds the penalties and the stoppage", () => {
    expect(requests.penaltyAdd("home", "24", [{ tenths: 1200 }])).toEqual({
      team: "home", player: "24", phases: [{ tenths: 1200 }],
    });
    expect(requests.penaltyEdit(7, 600)).toEqual({ id: 7, tenths: 600 });
    expect(requests.penaltyCancel(3)).toEqual({ id: 3 });
    expect(requests.penaltyCancelActive("away", 1)).toEqual({ team: "away", slot: 1 });
    expect(requests.stoppageSet(3)).toEqual({ minutes: 3 });
    expect(requests.stoppageAdd(-2)).toEqual({ minutes: -2 });
  });
  it("names every request the plugin understands", () => {
    expect(requestNames).toHaveLength(20);
    expect(requestNames).toContain("ClockToggle");
    expect(requestNames).toContain("StoppageAdd");
    expect(requestNames).toContain("GetFields");
    expect(requests.newMatch("basketball")).toEqual({ sport: "basketball", confirmed: false });
    expect(requests.newMatch("basketball", true)).toEqual({ sport: "basketball", confirmed: true });
  });
  it("matches the shared golden fixture request list", async () => {
    // The frozen request list from the protocol doc, kept next to the source (docs/remote-control.md).
    const fixture = (await import("./fixtures/requests.json")).default as {
      requestNames: string[];
    };
    expect(requestNames).toEqual(fixture.requestNames);

    // Drift guard: once the core fixture lands (Task 7.1.1), the local copy must stay identical.
    const coreFixture = fileURLToPath(
      new URL("../../tests/fixtures/vendor/requests.json", import.meta.url),
    );
    if (existsSync(coreFixture)) {
      expect(JSON.parse(readFileSync(coreFixture, "utf8"))).toEqual(fixture);
    }
  });
});
