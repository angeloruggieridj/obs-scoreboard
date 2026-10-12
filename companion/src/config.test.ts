// SPDX-License-Identifier: MIT
import { describe, expect, it } from "vitest";
import { DEFAULT_HOST, DEFAULT_PORT, getConfigFields } from "./config";

describe("getConfigFields", () => {
  it("declares host, port and the password secret", () => {
    const fields = getConfigFields();
    const byId = Object.fromEntries(fields.map((field) => [field.id, field]));
    expect(Object.keys(byId).sort()).toEqual(["host", "password", "port"]);
    expect(byId.host).toMatchObject({ type: "textinput", default: DEFAULT_HOST });
    expect(byId.port).toMatchObject({ type: "number", default: DEFAULT_PORT });
    expect(byId.password).toMatchObject({ type: "secret-text" });
  });

  it("keeps the password out of the config (secrets store only)", () => {
    const nonSecret = getConfigFields()
      .filter((field) => field.type !== "secret-text")
      .map((field) => field.id);
    expect(nonSecret).not.toContain("password");
  });

  it("gives every field a positive width", () => {
    for (const field of getConfigFields()) expect(field.width).toBeGreaterThan(0);
  });
});
