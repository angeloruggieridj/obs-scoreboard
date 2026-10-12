// SPDX-License-Identifier: MIT
import { describe, expect, it } from "vitest";
import { actionIds } from "./actions";
import { buildPresets } from "./presets";
import type { CompanionSimplePresetDefinition, CompanionSomePresetDefinition } from "@companion-module/base";
import type { ModuleSchema } from "./main";

function simple(
  preset: CompanionSomePresetDefinition<ModuleSchema> | undefined,
): CompanionSimplePresetDefinition<ModuleSchema> {
  if (!preset || preset.type !== "simple") throw new Error("not a simple preset");
  return preset;
}

describe("buildPresets", () => {
  it("references only presets defined in the same record", () => {
    const { structure, presets } = buildPresets();
    const referenced: string[] = [];
    for (const section of structure)
      for (const entry of section.definitions) if (typeof entry === "string") referenced.push(entry);
    expect(referenced.length).toBeGreaterThan(0);
    for (const id of referenced) expect(presets[id]).toBeDefined();
  });

  it("ships the common buttons: toggle, +1 home/away and next period", () => {
    const { presets } = buildPresets();
    expect(Object.keys(presets)).toEqual(
      expect.arrayContaining(["clock_toggle", "home_score_add", "away_score_add", "period_next"]),
    );
  });

  it("only uses action ids the module defines", () => {
    const { presets } = buildPresets();
    for (const preset of Object.values(presets)) {
      if (!preset || preset.type !== "simple") continue;
      for (const step of preset.steps)
        for (const entry of step.down) {
          if ("actionId" in entry) expect(actionIds).toContain(entry.actionId);
        }
    }
  });

  it("maps +1 home point to AddStat score", () => {
    const { presets } = buildPresets();
    const step = simple(presets["home_score_add"]).steps[0];
    const entry = step.down[0];
    if (!("actionId" in entry)) throw new Error("expected a plain action entry");
    expect(entry.actionId).toBe("add_stat");
    expect(entry.options).toMatchObject({ team: "home", stat: "score", delta: 1 });
  });

  it("maps the clock button to ClockToggle with the clock-running feedback", () => {
    const { presets } = buildPresets();
    const preset = simple(presets["clock_toggle"]);
    const entry = preset.steps[0].down[0];
    if (!("actionId" in entry)) throw new Error("expected a plain action entry");
    expect(entry.actionId).toBe("clock_toggle");
    expect(preset.feedbacks.map((feedback) => feedback.feedbackId)).toContain("clock_running");
  });
});
