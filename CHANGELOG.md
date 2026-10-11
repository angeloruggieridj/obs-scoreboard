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

### Changed

- Command rejections ("the match has ended", "OBS is closing", …) now appear in the dock, translated.
- A Text source removed by the user now unlinks at once, even while OBS still holds a reference to it.

### Fixed

- The plugin loads in OBS Studio 30, 31 and 32 (it is now built against the OBS 30.0 dependencies).
