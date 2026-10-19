# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2026-10-19

First stable release.

### Added

- **Direct Text-source update**: 28 scoreboard values (clock, period, names, scores, shots, fouls, penalties, strength, play time…), each linkable to a Text source from the dock menu ("Source links…", with a one-click "create a new Text source"); a source is updated only when its text changes, and links survive renaming, scene changes, restarts and scene collections.
- **Precise clock**: tenths of a second, counting up or down from a monotonic time base; ±1 minute / ±1 second arrows with auto-repeat, capped to the period; two-digit minutes; stoppage time.
- **14 sports** with editable presets (periods, lengths, overtime, score buttons, labels); a first-launch window asks which sport you score.
- **Timed penalties**: add from the dock ("+"), edit and cancel per penalty, compound penalties, up to 8 per team with 2 running at once, and the strength text (`5-4`).
- **Compact dock**: clock with labelled time buttons in a digital font, period stepper, one column per team, shots/fouls rows only where the sport uses them, link indicator, no scrolling; custom score and foul colors.
- **Match settings** window (sport, team names, periods, lengths, direction, overtime, players, labels, formats, colors) and a match that is saved as you play and can be resumed after a restart.
- **40 OBS hotkeys**, stored in the scene collection.
- **Remote control**: an `obs-scoreboard` obs-websocket vendor (20 requests, 4 events), a native Stream Deck plugin and a Bitfocus Companion module (MIT).
- **Optional `.txt` output** (off by default, files rewritten only when changed) and an **event log** with **YouTube chapters** export.
- **Update check** (once per OBS start, notify-only, can be switched off) and a **Windows installer** tested for install, upgrade and uninstall in CI.
- Packages for Windows (`.exe`, `.zip`), macOS (`.pkg`) and Linux (`.deb`) with SHA-256 checksums and build provenance attestations.

### Compatibility

OBS Studio 30.0 or newer (see `obs-compat.json` for the versions actually run against).
