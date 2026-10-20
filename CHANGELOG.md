# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and
this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- **Remote web page** (⚙ → Remote): off by default; *View only* or *View and control* from a phone, tablet or another computer on the same network. Two random links (view, control), *New links* to replace them. Covers the clock, periods, scores, shots, fouls and penalty cancellation.
- *Check for updates now* (Settings → Updates), with a message for "up to date" and for "could not check". A newer release shows its changelog, taken from GitHub, and an "Update" button in the dock.
- The sport of the match is always shown in the dock.
- Custom accent color for each team card (*Match settings → Look*).
- A progress border around the clock: it shrinks with the time left of the period (or fills with the time played). On by default; mode and color in *Match settings → Look*, or off.
- Clock color: off by default; one color, or steps by time left (for example amber at 1:00 and red at 0:30, up to four), optionally blended.
- Penalty types in the add-penalty window are chips instead of a drop-down.
- *Match settings → Look → Dock density*: Compact (as before) or Comfortable (taller buttons and fields).

### Changed

- A new look for the dock and every window: one button style with a single height, segmented `[ -1 | 2 | +1 ]` counters with a larger score button, the clock in a framed display with the period and stoppage steppers under it, a card per team with its own accent color, icons drawn by the plugin (menu, plus, pencil, cross, play/pause, reset) instead of text glyphs, and a thin bar of the time left under each running penalty. Colors now come from the OBS theme, so the dock and the windows follow the dark and light themes (and the others OBS ships); only start green, stop red and the score and foul colors stay fixed.
- Match settings, Penalty and Source links windows share the same look (tabs with an underline, a highlighted OK, colored swatches that show the color in use).
- The ⋮ menu is replaced by a gear icon that opens the settings window, now with a vertical list of pages: Match, Texts, Look, Output, Remote, Source links, Match control (new, resume, end, event log) and Updates. Fields keep the size of their content. The About window is gone: the version is on the Updates page.
- The sport, the source-link indicator and the settings button sit in a bordered footer at the bottom of the dock.
- Tooltips and descriptions are shorter, and the Italian ones are written in Italian rather than translated word for word.

### Fixed

- The goal, shots and fouls cells line up with their buttons and have the same width; the Stop button is no longer cut at the bottom; the Reset icon is drawn properly.

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
