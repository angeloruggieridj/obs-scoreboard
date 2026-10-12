# ScoreBoard for OBS — Stream Deck plugin

Native [Elgato Stream Deck](https://www.elgato.com/stream-deck) plugin for **ScoreBoard for OBS**.
It controls the scoreboard of the OBS plugin — clock, period, scores, stats, penalties and stoppage —
from your Stream Deck, and shows the live clock, score and period on the keys.

The plugin talks to OBS **only through obs-websocket** (the `obs-scoreboard` vendor protocol exposed
by the OBS plugin); it never uses a second, private protocol. The wire protocol is documented in
[`docs/remote-control.md`](../docs/remote-control.md).

## Requirements

- **Stream Deck 7.1** or newer (the plugin uses SDK v3 and bundles a Node 24 runtime).
- **OBS Studio 30.0** or newer with the **ScoreBoard for OBS** plugin installed and **obs-websocket**
  enabled. The OBS plugin still loads and works without obs-websocket; the Stream Deck plugin then
  simply shows its connection as failed.

## Install

Build the plugin and produce a `.streamDeckPlugin` file:

```bash
cd streamdeck
npm ci
npm run build
npm run pack
```

`npm run pack` writes `com.angeloruggieri.obs-scoreboard.streamDeckPlugin` in this folder. Double-click
that file to install it in Stream Deck. To install an already-built `.sdPlugin` folder manually, copy
`com.angeloruggieri.obs-scoreboard.sdPlugin` into
`%APPDATA%\Elgato\StreamDeck\Plugins\` (Windows) or `~/Library/Application Support/com.elgato.StreamDeck/Plugins/`
(macOS), then restart Stream Deck.

The packaged file is **not code-signed**; Stream Deck may warn about an unknown publisher.

## Enable obs-websocket in OBS

1. In OBS open **Tools → WebSocket Server Settings**.
2. Tick **Enable WebSocket server**. The default port is **4455**.
3. If **Enable Authentication** is on, note the **password** (or set your own).

## Configure the connection

The host, port and password are **global settings** for the whole plugin (they are shared by every
key), so you only configure them once:

1. Add any ScoreBoard for OBS action to your Stream Deck.
2. Open the action's settings (the Property Inspector opens on the right).
3. Under the action's own parameters you can reach the connection fields — on actions without
   parameters (for example **Toggle Clock**) the connection panel is shown directly. Set **Host**
   (default `127.0.0.1`), **Port** (default `4455`) and **Password**, then close the inspector.

Changing a field reconnects the plugin immediately; no Stream Deck restart is needed.

> **Security note.** In line with the Stream Deck SDK, the obs-websocket **password is stored in plain
> text** in the plugin's global settings (a per-plugin JSON store managed by Stream Deck). Anyone with
> read access to your Stream Deck profile can read it. Prefer a dedicated obs-websocket password and,
> if possible, keep obs-websocket bound to the local machine.

## Actions

One action per protocol command. The empty-body commands (toggle/start/stop/reset clock, end match)
need no configuration. Every parameterised action has its own Property Inspector with just the fields
it uses: clock-adjust/clock-set take minutes and seconds, score/stat take team/stat/delta,
set-team-name takes team and name, penalty-add takes team, player and a penalty option,
penalty-edit/penalty-cancel take a penalty id (from the panel's rows), penalty-cancel-active takes
team and slot, stoppage takes minutes, and new-match takes a sport and the Confirmed flag.

| Action | UUID suffix | Request |
|---|---|---|
| Toggle Clock | `clock-toggle` | `ClockToggle` |
| Start Clock | `clock-start` | `ClockStart` |
| Stop Clock | `clock-stop` | `ClockStop` |
| Reset Clock | `clock-reset` | `ClockReset` |
| Adjust Clock | `clock-adjust` | `ClockAdjust` |
| Set Clock | `clock-set` | `ClockSet` |
| Next Period | `period-next` | `PeriodNext` |
| Previous Period | `period-prev` | `PeriodPrev` |
| Score | `score` | `AddStat` (`score`) |
| Stat | `stat` | `AddStat` (`shots`/`fouls`/`fouls2`) |
| Set Team Name | `set-team-name` | `SetTeamName` |
| Add Penalty | `penalty-add` | `PenaltyAdd` |
| Edit Penalty | `penalty-edit` | `PenaltyEdit` |
| Cancel Penalty | `penalty-cancel` | `PenaltyCancel` |
| Cancel Running Penalty | `penalty-cancel-active` | `PenaltyCancelActive` |
| Set Stoppage | `stoppage-set` | `StoppageSet` (absolute minutes) |
| Adjust Stoppage | `stoppage-add` | `StoppageAdd` (relative minutes, signed) |
| End Match | `end-match` | `EndMatch` |
| New Match | `new-match` | `NewMatch` (`sport`, `confirmed`) |

`NewMatch` and `PeriodNext`/`PeriodPrev` may answer `confirmation` (for example while a match is live):
the key shows an alert, and you must turn **Confirmed** on in that key's settings and press again.
The tooltip on those keys says so too.

## Key feedback

The clock, score and period keys follow the four vendor events (`StateChanged`, `PeriodEnded`,
`MatchEnded`, `ScoreChanged`) and repaint their title on every change. On every connect **and**
reconnect the plugin also reads `GetFields`/`GetState` once and seeds the keys from it, so a stopped
clock (which emits no `StateChanged`) still shows its value instead of staying blank. The current
period comes from `StateChanged["period"]` (the `PeriodEnded` label is the period that just ended).

## Development

```bash
npm ci          # install the pinned dependencies from the committed lockfile
npm test        # vitest: protocol builders, vendor client, action mapping, key feedback
npm run build   # rollup -> com.angeloruggieri.obs-scoreboard.sdPlugin/bin/plugin.js
npm run pack    # package the .sdPlugin folder into a .streamDeckPlugin file
```

The Node toolchain targets **Node 22** (Stream Deck itself bundles the Node 24 runtime declared in the
manifest).

## License

GPL-2.0-or-later. See the repository `LICENSE`.
