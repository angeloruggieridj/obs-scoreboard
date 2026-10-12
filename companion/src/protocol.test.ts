// SPDX-License-Identifier: MIT
import { existsSync, readFileSync } from "node:fs";
import { resolve } from "node:path";
import { describe, expect, it } from "vitest";
import { requests, requestNames } from "./protocol";
import localFixture from "./fixtures/requests.json";

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
  it("matches the shared golden fixture request list", () => {
    // tests/fixtures/vendor/requests.json is the frozen request list from the protocol doc.
    // companion/src/fixtures/requests.json is a committed, byte-identical copy owned by this
    // project (the two Node projects do not share a package).
    expect(requestNames).toEqual(localFixture.requestNames);
  });
});

// The repo-wide fixture is owned by task 7.1.1 (group A); while the groups run in parallel it
// may not be on disk yet, so this check activates as soon as it lands and then guards drift.
const sharedFixturePath = resolve(process.cwd(), "../tests/fixtures/vendor/requests.json");
const hasSharedFixture = existsSync(sharedFixturePath);

describe("shared protocol fixture", () => {
  it.skipIf(!hasSharedFixture)("agrees with tests/fixtures/vendor/requests.json", () => {
    const shared = JSON.parse(readFileSync(sharedFixturePath, "utf8")) as { requestNames: string[] };
    expect(requestNames).toEqual(shared.requestNames);
  });
});
