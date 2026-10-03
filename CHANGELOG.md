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
- Every scoreboard value can be linked to its own Text source from "Source links…", including a one-click "create a new Text source" in the current scene; links are saved with the scene collection.

### Fixed

- The plugin loads in OBS Studio 30, 31 and 32 (it is now built against the OBS 30.0 dependencies).
