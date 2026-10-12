// SPDX-License-Identifier: GPL-2.0-or-later
import { existsSync, readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { describe, expect, it } from "vitest";

// The manifest lives next to the source; these tests are a drift guard for the property inspectors
// (finding 5): every parameterised action must point at an existing inspector file.
const sdPlugin = new URL("../com.angeloruggieri.obs-scoreboard.sdPlugin/", import.meta.url);
const manifest = JSON.parse(readFileSync(new URL("manifest.json", sdPlugin), "utf8")) as {
  UUID: string;
  PropertyInspectorPath?: string;
  Actions: { UUID: string; PropertyInspectorPath?: string }[];
};

const PREFIX = "com.angeloruggieri.obs-scoreboard.";
const PARAMETERISED = [
  "clock-adjust",
  "clock-set",
  "period-next",
  "period-prev",
  "score",
  "stat",
  "set-team-name",
  "penalty-add",
  "penalty-edit",
  "penalty-cancel",
  "penalty-cancel-active",
  "stoppage-set",
  "stoppage-add",
  "new-match",
];

const suffix = (uuid: string): string => uuid.replace(PREFIX, "");

describe("manifest.json", () => {
  it("gives every action a unique uuid under the plugin uuid", () => {
    const uuids = manifest.Actions.map((a) => a.UUID);
    expect(new Set(uuids).size).toBe(uuids.length);
    for (const uuid of uuids) expect(uuid.startsWith(PREFIX)).toBe(true);
  });

  it("points every parameterised action at an existing property inspector", () => {
    for (const id of PARAMETERISED) {
      const action = manifest.Actions.find((a) => suffix(a.UUID) === id);
      expect(action, `missing action ${id}`).toBeDefined();
      expect(action?.PropertyInspectorPath, `missing inspector for ${id}`).toBeTruthy();
      const file = fileURLToPath(new URL(action!.PropertyInspectorPath!, sdPlugin));
      expect(existsSync(file), `inspector file not found: ${action!.PropertyInspectorPath}`).toBe(true);
    }
  });

  it("leaves the empty-body actions without their own inspector (so the connection panel shows)", () => {
    const withInspector = manifest.Actions.filter((a) => a.PropertyInspectorPath).map((a) => suffix(a.UUID));
    expect(withInspector.sort()).toEqual([...PARAMETERISED].sort());
  });

  it("keeps the plugin-level connection inspector", () => {
    expect(manifest.PropertyInspectorPath).toBe("ui/connection.html");
    const file = fileURLToPath(new URL(manifest.PropertyInspectorPath!, sdPlugin));
    expect(existsSync(file)).toBe(true);
  });
});
