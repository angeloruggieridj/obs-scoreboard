# ScoreBoard for OBS — Companion module

This module drives the **ScoreBoard for OBS** plugin through obs-websocket. It talks to the
`obs-scoreboard` vendor exposed by the plugin: no extra protocol, no extra server.

## Requirements

- OBS Studio 30.0 or newer with the **ScoreBoard for OBS** plugin installed.
- **obs-websocket** enabled in OBS (Tools → WebSocket Server Settings).

## Configuration

- **Host**: the machine running OBS (`127.0.0.1` when Companion runs on the same machine).
- **Port**: the obs-websocket port (default `4455`).
- **Password**: the obs-websocket password, if authentication is enabled.

## What it does

The module maps every ScoreBoard command (clock, periods, scores, fouls, shots, penalties,
stoppage, new match) to a Companion action, and shows the live clock, scores and period on the
buttons through the vendor events.

See the plugin's `docs/remote-control.md` for the full protocol.
