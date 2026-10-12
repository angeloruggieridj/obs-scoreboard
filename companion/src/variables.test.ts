// SPDX-License-Identifier: MIT
import { describe, expect, it } from "vitest";
import { buildVariableDefinitions, fieldKeys, variableIds } from "./variables";

describe("variables", () => {
  it("exposes one variable per field key plus clock_running", () => {
    expect(fieldKeys).toHaveLength(28);
    expect(variableIds).toHaveLength(29);
    expect(new Set(variableIds).size).toBe(29);
  });

  it("maps the field keys to snake_case variable names", () => {
    expect(variableIds).toContain("clock");
    expect(variableIds).toContain("period");
    expect(variableIds).toContain("home_penalty1_player");
    expect(variableIds).toContain("away_penalty2_time");
    expect(variableIds).toContain("stoppage_announced");
    expect(variableIds).toContain("clock_running");
  });

  it("builds one named definition for every variable id", () => {
    const defs = buildVariableDefinitions();
    expect(Object.keys(defs).sort()).toEqual([...variableIds].sort());
    for (const id of variableIds) expect(defs[id]?.name).toBeTruthy();
  });
});
