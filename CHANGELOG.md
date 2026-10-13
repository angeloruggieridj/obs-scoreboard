# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Project skeleton: OBS module with an empty ScoreBoard dock, core library and unit tests,
  continuous integration on Windows, macOS and Linux.
- Core game logic: clock, periods, counters, penalties, strength, 14 sport presets,
  match engine, resumable JSON state.
- The dock drives the match clock (start/stop, ±1 min and ±1 s arrows with auto-repeat,
  reset) and writes it straight into a Text source you pick, with no `.txt` file in between.
- The match is saved as you play and can be resumed after a restart with "Resume previous match" (always asks first; OBS starts with a fresh match).
- Every scoreboard value can be linked to its own Text source from "Source links…", including a one-click "create a new Text source" in the current scene (the preview scene in Studio Mode); links are saved with the scene collection.
- A complete dock: big clock with minute and second arrows on either side, period stepper, stoppage time for count-up sports, a start/pause button, and a column for each team with segmented counters (score buttons follow the sport), shots, fouls, a second foul counter and the timed-penalty list with a cancel cross. The layout adapts to the dock width and scrolls when it is short; rows appear only for the sports that use them.
- Match settings window (menu ⋮ → Match settings…): sport (14 presets, the last settings of each sport are remembered), team names, periods, lengths, direction, overtime, players, score buttons, period labels (1T, 2T, 1TS…), two-digit minutes, tenths in the last minute, stoppage mode, penalty label with preview, strength text, total-time format. Text changes apply to the running match without stopping the clock; changes to the sport's structure start a new match (asking first when the match has begun).
- Score and foul colors with a "Default" button; the dock link indicator (● 14/28) and a menu with Match settings, Source links, New match, Resume previous match, End match and About.
- 40 OBS hotkeys (clock, ±1 minute/second with auto-repeat, periods, score +1/+2/+3/+5/+6/−1, shots, fouls, second fouls, cancel penalty 1/2, stoppage ±1 minute), saved in the scene collection. Hotkeys for a button the current sport does not have are ignored.
- On the first launch a small window asks which sport you score; the choice (and the last settings of each sport) is remembered across restarts and can be changed in Match settings.
- Add a timed penalty from the dock: a "+" button on each team (only for the sports that have timed penalties) opens a small window with the sport's penalty options (including the compound ones, e.g. 2+2) and the player number. Each penalty row also has an "edit" action that changes its remaining time by penalty id; a penalty that expires while the window is open is answered with a clear "that penalty no longer exists" message instead of an error. The window is not modal (the dock and the clock keep responding) and a new match, a resume or a sport change closes it. The clock keeps running throughout.
- The penalty fields (player, time, combined label) and the strength text are linkable to Text sources; the combined label uses the {player} {time} {phase2} placeholders and the penalty windows show a live preview of it.
- The in-OBS selftest now proves the Futsal fouls per half (with the restore when going back and after a resume) and the basketball fouls per quarter (overtime continues the 4th), and adds, edits and cancels a penalty through the real dock, checking the strength text.
- Remote control over obs-websocket: the plugin registers an `obs-scoreboard` vendor with one request per command (`StoppageAdd` is the relative stoppage; `NewMatch` asks for confirmation while a match is live) plus `GetState` and `GetFields`, and emits `StateChanged` (all 28 fields after a new match), `PeriodEnded`, `MatchEnded` and `ScoreChanged`. The vendor API is resolved at run time, so the plugin still loads and works when obs-websocket is absent. `ResumePrevious` is intentionally not exposed remotely. The wire protocol is documented in `docs/remote-control.md` and summarized in the README.
- A native Stream Deck plugin (`streamdeck/`, GPL-2.0-or-later): one action per command, key feedback for clock, score and period from the vendor events, reconnect plus initial state, and configurable obs-websocket connection (the password is stored in plain text in the global settings).
- A native Bitfocus Companion module (`companion/`, MIT): actions for every command, feedbacks and variables (clock, scores, period, penalties) updated from the vendor events, with reconnect and initial state.
- Optional `.txt` output, OFF by default: one file per value (named after its field key) in a folder you choose. A file is rewritten only when its text changed and with an atomic write; the files are never read back. It is meant for compatibility with other tools: OBS Text sources still poll files once a second with whole-second timestamps, so the direct Text update remains the main output.
- Event log window (dock menu "… → Event log…", non-modal so the dock stays compact): every event (period start/end, score, penalty, match end) with its time, team and a localized description, plus the YouTube chapter export (`0:00 Title`, then `M:SS`/`H:MM:SS` lines ten seconds apart) relative to the stream start, the recording start or the game time. Copy it to the clipboard or save it to a `.txt` file. YouTube needs at least three chapters: the window says so when there are fewer. Events of a match resumed from a previous OBS session are ignored for the stream/recording base, because their timestamps come from the previous run.

### Changed

- Command rejections ("the match has ended", "OBS is closing", …) now appear in the dock, translated.
- A Text source removed by the user now unlinks at once, even while OBS still holds a reference to it.

### Fixed

- The plugin loads in OBS Studio 30, 31 and 32 (it is now built against the OBS 30.0 dependencies).
