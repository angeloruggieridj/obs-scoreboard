# ScoreBoard for OBS — Bitfocus Companion module

A native [Bitfocus Companion](https://bitfocus.io/companion) module for the **ScoreBoard for OBS**
OBS Studio plugin. It drives the scoreboard over **obs-websocket** and mirrors the live clock,
scores, period and penalties on your Companion buttons through the plugin's vendor events.

The module talks **only** to the `obs-scoreboard` vendor exposed by the plugin: no extra protocol,
no extra server. See the plugin's [`docs/remote-control.md`](../docs/remote-control.md) for the
full wire protocol.

- **License:** MIT (this module only; the plugin is GPL-2.0-or-later).
- **Runtime:** Node.js 22 or newer.

## Requirements

- **OBS Studio 30.0 or newer** with the **ScoreBoard for OBS** plugin installed.
- **obs-websocket** enabled in OBS: *Tools → WebSocket Server Settings → Enable WebSocket server*.
  Note the port (default `4455`) and the password, if authentication is enabled.

The plugin keeps working when obs-websocket is disabled; in that case the module simply cannot
connect and shows a connection failure.

## Installation

### From a package

1. Build (or download) the module package:

   ```sh
   npm ci
   npm run package
   ```

   This produces `obs-scoreboard-0.1.0.tgz` in this folder.

2. In Companion: *Settings → Modules → Import module package* and pick the `.tgz` file.
3. Add a new **ScoreBoard for OBS** connection.

### From source (development)

```sh
npm ci
npm run build
npm run package
```

Then import the produced `.tgz` as above, or link the repository with the Companion developer
tooling. `npm test` runs the unit tests (protocol, vendor client, state, actions, feedbacks,
variables, config and presets).

## Configuration

| Field      | Description                                                                 |
| ---------- | --------------------------------------------------------------------------- |
| **Host**   | The machine running OBS (`127.0.0.1` when Companion runs on the same host). |
| **Port**   | The obs-websocket port (default `4455`).                                     |
| **Password** | The obs-websocket password, if authentication is enabled. **Stored as a secret**, not in the plain connection config. |

## Actions

One action per protocol request (20 in total):

`Clock: toggle`, `Clock: start`, `Clock: stop`, `Clock: reset`, `Clock: adjust (tenths)`,
`Clock: set (tenths)`, `Period: next`, `Period: previous`, `Counter: add`, `Team: set name`,
`Penalty: add`, `Penalty: edit`, `Penalty: cancel`, `Penalty: cancel active`,
`Stoppage: set (minutes)`, `Stoppage: add (minutes)`, `Match: end`, `Match: new`,
`State: get`, `Fields: get`.

`Match: new` asks for confirmation (`Confirm now`) when a match is already running, exactly like
the plugin's dock. `Stoppage: add` is relative (`+/-` minutes); `Stoppage: set` is absolute.

## Feedbacks

- **Clock running** (boolean) — the clock is counting.
- **Match ended** (boolean).
- **Home score** / **Away score** (value).
- **Period** (value).
- **Penalties running** (value) — how many penalty slots currently show a time.

## Variables

Every scoreboard field is exposed as a variable, plus a clock flag:

`clock`, `clock_running`, `period`, `home_name`, `away_name`, `home_score`, `away_score`,
`home_shots`, `away_shots`, `home_fouls`, `away_fouls`, `home_fouls2`, `away_fouls2`,
`home_penalty1_player`, `home_penalty1_time`, `home_penalty1_label`, `home_penalty2_player`,
`home_penalty2_time`, `home_penalty2_label`, `away_penalty1_player`, `away_penalty1_time`,
`away_penalty1_label`, `away_penalty2_player`, `away_penalty2_time`, `away_penalty2_label`,
`strength`, `stoppage`, `stoppage_announced`, `playtime`.

## Presets

The module ships ready-made buttons: **Clock: start / stop**, **Score: +1 home**,
**Score: +1 away** and **Period: next**.

## Protocol

The module uses the `obs-scoreboard` obs-websocket vendor: `CallVendorRequest` for commands and
`VendorEvent` (`EventSubscription::Vendors`) for the live updates. The normative description is in
the plugin repository: [`docs/remote-control.md`](../docs/remote-control.md).
