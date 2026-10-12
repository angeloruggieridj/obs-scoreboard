# ScoreBoard for OBS — Companion module

This module drives the **ScoreBoard for OBS** plugin through **obs-websocket**. It talks to the
`obs-scoreboard` vendor exposed by the plugin: no extra protocol, no extra server. The live clock,
scores, period and penalties are mirrored on your buttons through the vendor events.

## Requirements

- OBS Studio 30.0 or newer with the **ScoreBoard for OBS** plugin installed.
- **obs-websocket** enabled in OBS (Tools → WebSocket Server Settings). The plugin keeps working
  when obs-websocket is disabled, but this module then cannot connect.

## Installation

Build the package with `npm ci && npm run package` and import the produced
`obs-scoreboard-0.1.0.tgz` in Companion (*Settings → Modules → Import module package*), then add a
new **ScoreBoard for OBS** connection.

## Configuration

- **Host**: the machine running OBS (`127.0.0.1` when Companion runs on the same machine).
- **Port**: the obs-websocket port (default `4455`).
- **Password**: the obs-websocket password, if authentication is enabled. It is stored as a
  **secret**, not in the plain connection config.

## Actions

One action per protocol request (20 total): clock toggle/start/stop/reset/adjust/set, period
next/previous, counter add, team set name, penalty add/edit/cancel/cancel active, stoppage
set/add, match end, match new, state get, fields get. **Match: new** asks for confirmation while a
match is running. **Stoppage: add** is relative (`+/-` minutes), **Stoppage: set** is absolute.

## Feedbacks

**Clock running**, **Match ended** (booleans); **Home score**, **Away score**, **Period** and
**Penalties running** (values). The **Clock: start / stop** preset button already uses the clock
feedback.

## Variables

Every scoreboard field is exposed, plus `clock_running`: `clock`, `period`, `home_name`,
`away_name`, `home_score`, `away_score`, `home_shots`, `away_shots`, `home_fouls`, `away_fouls`,
`home_fouls2`, `away_fouls2`, the penalty slots (`home_penalty1_player`, …, `away_penalty2_label`),
`strength`, `stoppage`, `stoppage_announced` and `playtime`.

## Presets

Ready-made buttons: **Clock: start / stop**, **Score: +1 home**, **Score: +1 away** and
**Period: next**.

## Protocol

The normative wire protocol is documented in the plugin repository:
[`docs/remote-control.md`](../docs/remote-control.md).

**License:** MIT (this module only; the plugin is GPL-2.0-or-later).
