// SPDX-License-Identifier: MIT
import type {
  CompanionButtonStyleProps,
  CompanionPresetDefinitions,
  CompanionPresetSection,
  CompanionSimplePresetDefinition,
} from "@companion-module/base";
import type { ModuleSchema } from "./main";

type SimplePreset = CompanionSimplePresetDefinition<ModuleSchema>;

// The preset ids shipped with the module: the buttons a user drops on a Stream Deck-like surface.
export const presetIds = Object.freeze([
  "clock_toggle",
  "home_score_add",
  "away_score_add",
  "period_next",
] as const);

const style = (text: string, bgcolor: number): CompanionButtonStyleProps => ({
  text,
  size: "auto",
  color: 0xffffff,
  bgcolor,
  alignment: "center:center",
});

// A one-step button: the action runs on press, the (optional) feedbacks paint the live value.
function button(
  name: string,
  actionId: string,
  options: Record<string, string | number | boolean>,
  buttonStyle: CompanionButtonStyleProps,
  feedbacks: SimplePreset["feedbacks"] = [],
): SimplePreset {
  return {
    type: "simple",
    name,
    style: buttonStyle,
    steps: [{ down: [{ actionId, options }], up: [] }],
    feedbacks,
  };
}

// The preset definitions and the section structure handed to `setPresetDefinitions`. Pure: no
// Companion instance, no vendor, no state — only the shapes the SDK validates.
export function buildPresets(): {
  structure: CompanionPresetSection<ModuleSchema>[];
  presets: CompanionPresetDefinitions<ModuleSchema>;
} {
  const presets: CompanionPresetDefinitions<ModuleSchema> = {
    clock_toggle: button("Clock: start / stop", "clock_toggle", {}, style("CLOCK", 0x1f6f4a), [
      {
        feedbackId: "clock_running",
        options: {},
        style: { bgcolor: 0x0b3d2e, color: 0xffffff },
      },
    ]),
    home_score_add: button(
      "Score: +1 home",
      "add_stat",
      { team: "home", stat: "score", delta: 1 },
      style("HOME +1", 0x1d3f7a),
    ),
    away_score_add: button(
      "Score: +1 away",
      "add_stat",
      { team: "away", stat: "score", delta: 1 },
      style("AWAY +1", 0x7a1d3f),
    ),
    period_next: button("Period: next", "period_next", { confirmed: false }, style("PERIOD +", 0x4a3a10), [
      { feedbackId: "period", options: {} },
    ]),
  };

  const structure: CompanionPresetSection<ModuleSchema>[] = [
    {
      id: "scoreboard",
      name: "ScoreBoard for OBS",
      description: "Clock, score and period buttons for the running match.",
      definitions: [...presetIds],
    },
  ];

  return { structure, presets };
}
