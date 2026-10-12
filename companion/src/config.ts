// SPDX-License-Identifier: MIT
import type { SomeCompanionConfigField } from "@companion-module/base";

// The connection settings live in Companion's config store; the obs-websocket password lives in
// the secrets store (the `secret-text` field below), never in the config (API v2).
export type ModuleConfig = { host: string; port: number };
export type ModuleSecrets = { password?: string };

export const DEFAULT_HOST = "127.0.0.1";
export const DEFAULT_PORT = 4455;

// The connection UI: host, port and the obs-websocket password.
export function getConfigFields(): SomeCompanionConfigField[] {
  return [
    {
      type: "textinput",
      id: "host",
      label: "Host",
      width: 6,
      default: DEFAULT_HOST,
      tooltip: "The machine running OBS (127.0.0.1 when Companion runs on the same machine).",
    },
    {
      type: "number",
      id: "port",
      label: "Port",
      width: 6,
      default: DEFAULT_PORT,
      min: 1,
      max: 65535,
      step: 1,
    },
    {
      type: "secret-text",
      id: "password",
      label: "Password",
      width: 6,
      default: "",
      tooltip: "The obs-websocket password, if authentication is enabled.",
    },
  ];
}
