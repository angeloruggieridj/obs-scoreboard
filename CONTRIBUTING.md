# Contributing

Thanks for looking. This is a small project with a deliberate shape; a few
minutes here will save a round trip on review.

## Build and test

```bash
# core unit tests: no OBS, no Qt, seconds to build
cmake -S tests -B build_tests
cmake --build build_tests --config Debug
ctest --test-dir build_tests -C Debug --output-on-failure

# the plugin itself (needs Qt 6 and the OBS 30.0+ development files)
cmake --preset windows-x64 && cmake --build --preset windows-x64   # Windows
cmake --preset macos && cmake --build --preset macos               # macOS
cmake --preset ubuntu-x86_64 && cmake --build --preset ubuntu-x86_64 # Linux
```

Before pushing, the check CI also runs:

```bash
python tools/check_locale_keys.py   # every locale key exists in both en-US.ini and it-IT.ini
```

Loading a built plugin into a local OBS install, to check it in a real session:

```bash
pwsh -File scripts/check-obs-load.ps1 -ObsVersion 32.2.2 -Layouts legacy
```

Fps selftest of the direct Text-source write (the plugin's key claim), in a real OBS:

```bash
# Windows, portable OBS
pwsh -File scripts/run-selftest.ps1 -Fps 30
# Linux, system OBS (needs xvfb-run)
scripts/run-selftest.sh --fps 30 --plugin <path>/obs-scoreboard.so --locale data/locale
```

## Layout

| Where | What |
|---|---|
| `src/core/` | Plain C++17. No OBS, no Qt, no reads of the system clock — time arrives as `sb::Micros`. Match state, clock, sports/presets, field formatting. |
| `src/plugin/` | The OBS module: dock UI, Text source binding, hotkeys, settings store. |
| `tests/` | doctest unit tests for the core. |
| `data/locale/` | Hand-edited `.ini` files, one key per line, kept in sync by `tools/check_locale_keys.py`. |

**Put logic in `src/core/` when you can.** Anything that lives there can be
tested in seconds without OBS running, which is the difference between a
decision that is verified and one that is hoped for.

## Working agreement

- **Test-first on the core.** Write the failing test, watch it fail for the
  expected reason, write the minimal code to pass it, then run the whole suite.
- **Zero warnings.** The project builds with `-Wall -Wextra -Wpedantic` (and
  `/W4` on MSVC); the CI presets (and `-DSB_WARNINGS_AS_ERRORS=ON` for the core
  tests) treat warnings as errors.
- **English identifiers and comments**; every source file starts with
  `// SPDX-License-Identifier: GPL-2.0-or-later`.
- **Every user-visible string** goes through `obs_module_text()`, with the key
  added to **both** `data/locale/en-US.ini` and `data/locale/it-IT.ini` — run
  `python tools/check_locale_keys.py` before committing.
- **Every user-visible change gets a [CHANGELOG.md](CHANGELOG.md) entry**, under
  `## [Unreleased]`, in the Added / Changed / Fixed section it belongs to
  ([Keep a Changelog](https://keepachangelog.com/en/1.1.0/)).
- **Comment the why, not the what.** The interesting comments here explain a
  decision that looks arbitrary until you know what went wrong without it.
- Never copy code from `StreamnDad/streamn-scoreboard`: this project is a
  clean-room implementation of its own design.

## Commit messages

Short imperative subject, the way the history reads today:
`fix(core): clamp the clock at the period duration`. The CHANGELOG carries the
long explanation, so the commit body can stay brief.
