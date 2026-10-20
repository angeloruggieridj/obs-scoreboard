# obs-scoreboard remote control (obs-websocket vendor)

This document is the user copy of the **normative** `obs-scoreboard` wire protocol; the README links here
instead of repeating it. The plugin exposes its scoreboard through [obs-websocket][obsws]
as a **vendor**, resolved at runtime; the plugin still loads and works when obs-websocket is absent
(no vendor is registered in that case).

[obsws]: https://github.com/obsproject/obs-websocket

**Vendor name:** `obs-scoreboard`.

## Transport

Every request goes through the obs-websocket `CallVendorRequest` request (OpCode 6,
`requestType: "CallVendorRequest"`):

```jsonc
// requestData of CallVendorRequest
{ "vendorName": "obs-scoreboard", "requestType": "<Name>", "requestData": <object> }
// responseData of CallVendorRequest
{ "vendorName": "obs-scoreboard", "requestType": "<Name>", "responseData": <envelope> }
```

Events arrive as `VendorEvent` (OpCode 5) and require the `EventSubscription::Vendors`
subscription (`1 << 9`):

```jsonc
{ "eventType": "VendorEvent", "eventIntent": 512,
  "eventData": { "vendorName": "obs-scoreboard", "eventType": "<EventName>", "eventData": <payload> } }
```

## Response envelope

Every response is one JSON object with a `result` field:

| `result` | Fields | Meaning |
|---|---|---|
| `ok` | `state?`, `fields?` | command applied (or `GetState`/`GetFields`) |
| `rejected` | `reason` | command refused by the engine (`CommandResult::reason`) |
| `confirmation` | `reason` | the command asks for confirmation: resend with `confirmed:true` |
| `error` | `error` | malformed request: a stable code (below) |

**Stable error codes:** `unknownRequest`, `badRequest`, `badField`, `unknownSport`, `notReady`.
Clients add one **client-side only** code, `transport`, when the call to the vendor does not get
through at all (obs-websocket answers 600 or throws); the plugin never produces it.

## Requests

There are **20** requests. Every request except `GetState`/`GetFields` produces a core `Command`
(for `NewMatch` the plugin builds a new match engine; `StoppageAdd` is the relative
`cmd::StoppageAdd` command).

| `<Name>` | `requestData` | Response beyond `result` |
|---|---|---|
| `ClockToggle` | `{}` | — |
| `ClockStart` | `{}` | — |
| `ClockStop` | `{}` | — |
| `ClockReset` | `{}` | — |
| `ClockAdjust` | `{"deltaTenths": int}` | — |
| `ClockSet` | `{"tenths": int}` | — |
| `PeriodNext` | `{"confirmed"?: bool}` (default `false`) | — |
| `PeriodPrev` | `{"confirmed"?: bool}` | — |
| `AddStat` | `{"team":"home"\|"away","stat":"score"\|"shots"\|"fouls"\|"fouls2","delta": int}` | — |
| `SetTeamName` | `{"team":"home"\|"away","name": string}` | — |
| `PenaltyAdd` | `{"team":"home"\|"away","player": string,"phases":[...]}` | — |
| `PenaltyEdit` | `{"id": int, "tenths": int}` | — |
| `PenaltyCancel` | `{"id": int}` | — |
| `PenaltyCancelActive` | `{"team":"home"\|"away","slot": int}` | — |
| `StoppageSet` | `{"minutes": int}` (absolute, ≥ 0) | — |
| `StoppageAdd` | `{"minutes": int}` (relative, signed) | — |
| `EndMatch` | `{}` | — |
| `NewMatch` | `{"sport": string, "confirmed"?: bool}` (id of one of the 14 presets; `confirmed` default `false`) | — |
| `GetState` | `{}` | `state` (object below) |
| `GetFields` | `{}` | `fields` (all 28 `fieldKey` keys) |

### `PenaltyAdd` rules

`phases` is an array with **at least one** phase (2+2 = two phases): each phase is
`{"tenths": int ≥ 0, "reducesStrength"?: bool}`; `reducesStrength` defaults to `true` when absent;
`tenths` is in tenths of a second and must be within `[0, 2^31-1]`. A penalty's `id`, `player` and
`time` are **read-only** and arrive **only** from `GetState` (events carry text, not ids); the valid
ids for `PenaltyEdit`/`PenaltyCancel` come from there.

## `GetState` → `state`

```jsonc
{
  "protocol": 1,
  "sport": "ice_hockey", "period": 1, "periodLabel": "1", "regulationPeriods": 3,
  "clock": "20:00", "clockTenths": 12000, "clockRunning": false, "clockDirection": "down",
  "homeName": "HOME", "awayName": "AWAY",
  "homeScore": 0, "awayScore": 0, "homeShots": 0, "awayShots": 0,
  "homeFouls": 0, "awayFouls": 0, "homeFouls2": 0, "awayFouls2": 0,
  "strength": "", "stoppage": "", "stoppageAnnounced": "", "stoppageAnnouncedMinutes": 0,
  "playTime": "00:00", "matchEnded": false,
  "penalties": {
    "home": [{"id": 1, "player": "24", "time": "2:00", "phase2": "", "queued": false}],
    "away": []
  }
}
```

`protocol` is the wire protocol version (integer, today `1`). `stoppageAnnounced` is the formatted
text (`"+3"` or `""`), `stoppageAnnouncedMinutes` its numeric value (≥ 0).

## `GetFields` → `fields`

An object with **all 28** keys of `sb::fieldKey(FieldId)` (`clock`, `period`, `home.name`, …,
`stoppage.announced`, `playtime`) and the current formatted texts.

## Events

| `<EventName>` | `eventData` |
|---|---|
| `StateChanged` | `{"changed": {"<fieldKey>": "<value>", …}, "clockRunning": bool, "matchEnded": bool}` — `changed` holds **only** the fields whose text changed; after `NewMatch`/`replaceMatch` it holds **all 28** fields (empty ones included) |
| `PeriodEnded` | `{"period": int, "label": "<period label>"}` |
| `MatchEnded` | `{"period": int, "label": "<period label>"}` |
| `ScoreChanged` | `{"team":"home"\|"away", "score": int, "homeScore": int, "awayScore": int}` |

## Semantics

Requests are applied **on the UI thread** and answer with the engine's `CommandResult`; a
`rejected` carries the existing `reason` key (`Reason.<key>`), which clients may translate or show
as is. `StoppageAdd` adds to the announced value and never goes below 0 (like the dock's
"+/- minutes" step). `StateChanged` is emitted after every field change (clock tick, command, new
match); `ScoreChanged` accompanies a score change; `PeriodEnded`/`MatchEnded` derive from the
engine's event log and are not repeated. A new match (`NewMatch`, resume, sport change) emits a
`StateChanged` with **all** fields and resets the event counters. If a match is in progress
(`sb::matchStarted(engine)`), `NewMatch` without `confirmed:true` answers
`{"result":"confirmation","reason":"match.inProgress"}` (like the dock dialog); clients resend with
`confirmed:true`.

**`notReady` is "unknown", not "rejected".** Every request is marshalled to the plugin's UI thread and
the waiting side gives up after a few seconds. If that thread does not answer in time (OBS is busy or
shutting down), the response is `{"result":"error","error":"notReady"}` even though the request may
still be **applied later** on the UI thread. A client must therefore not blindly retry a
non-idempotent command after `notReady`: the first copy could already have taken effect, and a retry
would apply it twice. (`GetState`/`GetFields` only read, so retrying those is harmless.)

**`ResumePrevious` is not exposed remotely** (deliberate deviation: resuming stays in the dock).

## Native clients

The native clients built on this protocol live in [`streamdeck/`](../streamdeck) (Elgato Stream
Deck plugin) and [`companion/`](../companion) (Bitfocus Companion module). Both talk to the plugin
through this vendor only; no extra protocol is added to the plugin.

## Web page

Besides obs-websocket, the plugin can serve a page to a browser on the same network. It is **off by default**
(**⚙ → Remote**), listens on the chosen port (default `8765`) of every network interface, and speaks plain HTTP.

| Mode | Links | What a browser may do |
|---|---|---|
| Off | none | nothing (the port is closed) |
| View only | view link | read the match |
| View and control | view link, control link | read; with the control link also the commands below |

Each link carries its own key (`?k=...`): random (24 hex digits) or typed by the user (6–64 letters, digits, `-`, `_`; the two
must differ). A request without a valid key gets `401`; a view key that asks for a command gets `403`. Changing a key
(*Random*, or typing another) stops every link given out with the old one. The keys are stored in `settings.json`.

| Request | Access | Answer |
|---|---|---|
| `GET /` | view | the page (the language of OBS) |
| `GET /api/state` | view | the vendor `GetState` object plus `layout` (which counters the sport has, the score buttons), `look` (the colors, the progress border and the clock color of the dock) and `control`; each penalty also carries `percent` |
| `GET /font` | view | the clock font of the dock (DSEG7 Classic Bold) |
| `POST /api/<Name>` | control | the vendor response envelope; `<Name>` is any request of the table above except `GetState` and `GetFields` |

The commands go through the same code as the dock and obs-websocket, so a rejection (`rejected`, `confirmation`) is the same
envelope. On a phone the page can vibrate when a button is pressed (`navigator.vibrate`, Android browsers); the viewer switches it on or
off in the footer of the page, and the choice stays in that browser. Requests are limited to 16 KB, a connection lives at most 8 seconds, and at most 32 are open at once.
