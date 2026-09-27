<div align="center">

# 🏒 ScoreBoard for OBS

**Live scoreboard for OBS Studio — the clock and every value are written straight into
your existing Text sources, the instant they change.**
**Tabellone in diretta per OBS Studio — il cronometro e ogni valore vengono scritti
direttamente nelle tue sorgenti Text esistenti, nell'istante in cui cambiano.**

[English](#english) · [Italiano](#italiano)

[![Core tests](https://github.com/angeloruggieridj/obs-scoreboard/actions/workflows/core-tests.yaml/badge.svg)](https://github.com/angeloruggieridj/obs-scoreboard/actions/workflows/core-tests.yaml)
[![Build](https://github.com/angeloruggieridj/obs-scoreboard/actions/workflows/push.yaml/badge.svg)](https://github.com/angeloruggieridj/obs-scoreboard/actions/workflows/push.yaml)
[![Latest release](https://img.shields.io/github/v/release/angeloruggieridj/obs-scoreboard?include_prereleases&sort=semver)](https://github.com/angeloruggieridj/obs-scoreboard/releases)
[![License: GPL v2+](https://img.shields.io/github/license/angeloruggieridj/obs-scoreboard)](LICENSE)

![Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20macOS%20%7C%20Linux-blue)
![OBS](https://img.shields.io/badge/OBS%20Studio-30.0%2B-302e31?logo=obsstudio)

</div>

## English

### Description

Existing scoreboard plugins write every value to a `.txt` file and rely on OBS's
**Text** sources ("Read from file") to pick it up. That approach has a structural
flaw that no amount of careful file writing can fix: OBS's Text sources
(*Text (GDI+)* on Windows, *Text (FreeType 2)* on macOS/Linux) poll the file
**once a second**, and they decide whether to re-read it by comparing the file's
last-modified time, which only has **whole-second** precision. If two updates
land in the same calendar second, the second one is never seen — on stream, the
clock visibly skips a second (e.g. `19:29 → 19:27`, `19:28` never appears).

**ScoreBoard for OBS does not go through files.** It writes the text **directly
into the OBS Text source** (through the OBS API) the exact moment a value
changes; OBS shows it on the next frame. You keep using your own Text sources
with whatever font, color and position you already picked — the plugin only
changes their text. Writing to a `.txt` file remains available as an optional,
secondary output for compatibility with other tools.

### Status

**In development — not usable yet.** The project currently ships a build
skeleton (CMake, an OBS module that loads an empty dock, a core library and its
unit tests, and CI on Windows, macOS and Linux) with no scoreboard behavior
implemented yet. Track progress in [CHANGELOG.md](CHANGELOG.md).

### Building from source

Requires CMake, a C++17 compiler, Qt 6 and OBS Studio 30.0+ development files.
The project uses [CMake presets](CMakePresets.json), usable from Visual Studio
or VS Code.

```bash
# core unit tests (no OBS, no Qt)
cmake -S tests -B build_tests
cmake --build build_tests --config Debug
ctest --test-dir build_tests -C Debug --output-on-failure
```

```bash
# the plugin itself
cmake --preset windows-x64 && cmake --build --preset windows-x64   # Windows
cmake --preset macos && cmake --build --preset macos               # macOS
cmake --preset ubuntu-x86_64 && cmake --build --preset ubuntu-x86_64 # Linux
```

The first plugin build downloads OBS, obs-deps and Qt into `.deps/`. See
[CONTRIBUTING.md](CONTRIBUTING.md) for the full command list, including how to
load the built plugin into a local OBS install and how to check translations.

### License

GNU General Public License v2.0 or later (GPL-2.0-or-later), the same license as
OBS Studio — see [LICENSE](LICENSE).

## Italiano

### Descrizione

I plugin di tabellone esistenti scrivono ogni valore in un file `.txt` e si
affidano alle sorgenti **Text** di OBS ("Leggi da file") per rilevarlo. Questo
approccio ha un difetto strutturale che non si può correggere scrivendo meglio i
file: le sorgenti Text di OBS (*Text (GDI+)* su Windows, *Text (FreeType 2)* su
macOS/Linux) controllano il file **una volta al secondo**, e decidono se
rileggerlo confrontando la data di ultima modifica del file, che ha precisione
al **secondo intero**. Se due aggiornamenti cadono nello stesso secondo di
calendario, il secondo non viene mai visto: in diretta il cronometro salta
visibilmente un secondo (es. `19:29 → 19:27`, `19:28` non appare mai).

**ScoreBoard for OBS non passa dai file.** Scrive il testo **direttamente dentro
la sorgente Text di OBS** (tramite l'API di OBS) nel momento esatto in cui il
valore cambia; OBS lo mostra al fotogramma successivo. L'utente continua a usare
le proprie sorgenti Text con il font, il colore e la posizione già scelti: il
plugin ne cambia solo il testo. La scrittura su file `.txt` resta disponibile
come opzione secondaria, per compatibilità con altri strumenti.

### Stato

**In sviluppo — non ancora utilizzabile.** Il progetto al momento comprende
solo lo scheletro di compilazione (CMake, un modulo OBS che carica un pannello
vuoto, una libreria core con i relativi test unitari, e la CI su Windows, macOS
e Linux); nessuna funzionalità di tabellone è ancora implementata. Segui i
progressi in [CHANGELOG.md](CHANGELOG.md).

### Compilare dai sorgenti

Richiede CMake, un compilatore C++17, Qt 6 e i file di sviluppo di OBS Studio
30.0+. Il progetto usa [CMake presets](CMakePresets.json), utilizzabili sia da
Visual Studio sia da VS Code.

```bash
# test unitari del core (senza OBS, senza Qt)
cmake -S tests -B build_tests
cmake --build build_tests --config Debug
ctest --test-dir build_tests -C Debug --output-on-failure
```

```bash
# il plugin vero e proprio
cmake --preset windows-x64 && cmake --build --preset windows-x64   # Windows
cmake --preset macos && cmake --build --preset macos               # macOS
cmake --preset ubuntu-x86_64 && cmake --build --preset ubuntu-x86_64 # Linux
```

Il primo avvio della build del plugin scarica OBS, obs-deps e Qt in `.deps/`.
Vedi [CONTRIBUTING.md](CONTRIBUTING.md) per l'elenco completo dei comandi,
incluso come caricare il plugin compilato in un'installazione locale di OBS e
come verificare le traduzioni.

### Licenza

GNU General Public License v2.0 o successiva (GPL-2.0-or-later), la stessa
licenza di OBS Studio — vedi [LICENSE](LICENSE).
