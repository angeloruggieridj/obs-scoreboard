<div align="center">

# 🏒 ScoreBoard for OBS

**Live scoreboard for OBS Studio — the clock and every value are written straight into
your existing Text sources, the instant they change.**
**Tabellone in diretta per OBS Studio — il cronometro e ogni valore vengono scritti
direttamente nelle tue sorgenti Text esistenti, nell'istante in cui cambiano.**

[English](#english) · [Italiano](#italiano)

[![Selftest](https://github.com/angeloruggieridj/obs-scoreboard/actions/workflows/selftest.yaml/badge.svg)](https://github.com/angeloruggieridj/obs-scoreboard/actions/workflows/selftest.yaml)
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

### Remote control

The plugin exposes its scoreboard to
[obs-websocket](https://github.com/obsproject/obs-websocket) as the `obs-scoreboard`
**vendor** (resolved at run time), so external tools can drive it with `CallVendorRequest`
and follow the live values through the vendor `VendorEvent`s. The **normative** wire protocol
is in [docs/remote-control.md](docs/remote-control.md); the tables below are a summary.

To turn it on in OBS: **Tools → WebSocket Server Settings → Enable WebSocket server**
(default port `4455`; set a password if you enable authentication). The plugin still loads and
works **without** obs-websocket — it simply registers no vendor and the dock keeps working.

Every response is one JSON object whose `result` is `ok`, `rejected`, `confirmation` or `error`
(stable error codes: `unknownRequest`, `badRequest`, `badField`, `unknownSport`, `notReady`).

**Requests (20):**

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
| `NewMatch` | `{"sport": string, "confirmed"?: bool}` (id of one of the 14 presets) | — |
| `GetState` | `{}` | `state` (object; includes `protocol` and `stoppageAnnouncedMinutes`) |
| `GetFields` | `{}` | `fields` (all 28 `fieldKey` keys) |

`StoppageAdd` is **relative** (`+/-` minutes) while `StoppageSet` sets the **absolute** announced
value. **`ResumePrevious` is not exposed** remotely: resuming a previous match stays in the dock.
`NewMatch` answers `confirmation`/`match.inProgress` while a match is in progress until it is
resent with `confirmed:true`.

**Events (4):** they arrive as `VendorEvent` and require the `Vendors` subscription (`1 << 9`).

| `<EventName>` | `eventData` |
|---|---|
| `StateChanged` | `{"changed": {"<fieldKey>": "<value>", …}, "clockRunning": bool, "matchEnded": bool}` — only the fields whose text changed; after a new match, all 28 fields |
| `PeriodEnded` | `{"period": int, "label": "<period label>"}` |
| `MatchEnded` | `{"period": int, "label": "<period label>"}` |
| `ScoreChanged` | `{"team":"home"\|"away", "score": int, "homeScore": int, "awayScore": int}` |

Two native clients are built on this protocol:

- [**Stream Deck plugin**](streamdeck) (`streamdeck/`): one action per command, live key feedback
  for clock, score and period, reconnect and initial state.
- [**Bitfocus Companion module**](companion) (`companion/`): actions, feedbacks and variables for
  the clock, scores, period and penalties, with reconnect and initial state.

Both talk to the plugin **only** through this vendor; no extra protocol is added to the plugin.

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

### Controllo remoto

Il plugin espone il proprio tabellone a
[obs-websocket](https://github.com/obsproject/obs-websocket) come **vendor**
`obs-scoreboard` (risolto a runtime), così strumenti esterni possono comandarlo con
`CallVendorRequest` e seguire i valori in diretta tramite i `VendorEvent` del vendor. Il
protocollo wire **normativo** è in [docs/remote-control.md](docs/remote-control.md); le tabelle
seguenti ne sono una sintesi.

Per abilitarlo in OBS: **Strumenti → WebSocket Server Settings → Enable WebSocket server**
(porta predefinita `4455`; imposta una password se attivi l'autenticazione). Il plugin si carica
e funziona **anche senza** obs-websocket: in quel caso non registra alcun vendor e il pannello
continua a funzionare.

Ogni risposta è un oggetto JSON il cui `result` è `ok`, `rejected`, `confirmation` o `error`
(codici d'errore stabili: `unknownRequest`, `badRequest`, `badField`, `unknownSport`, `notReady`).

**Richieste (20):**

| `<Name>` | `requestData` | Risposta oltre a `result` |
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
| `StoppageSet` | `{"minutes": int}` (assoluto, ≥ 0) | — |
| `StoppageAdd` | `{"minutes": int}` (relativo, con segno) | — |
| `EndMatch` | `{}` | — |
| `NewMatch` | `{"sport": string, "confirmed"?: bool}` (id di uno dei 14 preset) | — |
| `GetState` | `{}` | `state` (oggetto; include `protocol` e `stoppageAnnouncedMinutes`) |
| `GetFields` | `{}` | `fields` (tutte e 28 le chiavi `fieldKey`) |

`StoppageAdd` è **relativo** (`+/-` minuti) mentre `StoppageSet` imposta il valore annunciato
**assoluto**. **`ResumePrevious` non è esposto** da remoto: la ripresa della partita precedente
resta nel pannello. `NewMatch` risponde `confirmation`/`match.inProgress` quando una partita è in
corso, finché non viene rimandata con `confirmed:true`.

**Eventi (4):** arrivano come `VendorEvent` e richiedono la sottoscrizione `Vendors` (`1 << 9`).

| `<EventName>` | `eventData` |
|---|---|
| `StateChanged` | `{"changed": {"<fieldKey>": "<value>", …}, "clockRunning": bool, "matchEnded": bool}` — solo i campi il cui testo è cambiato; dopo una nuova partita, tutti e 28 i campi |
| `PeriodEnded` | `{"period": int, "label": "<period label>"}` |
| `MatchEnded` | `{"period": int, "label": "<period label>"}` |
| `ScoreChanged` | `{"team":"home"\|"away", "score": int, "homeScore": int, "awayScore": int}` |

Su questo protocollo sono basati due client nativi:

- [**Plugin Stream Deck**](streamdeck) (`streamdeck/`): un'azione per comando, feedback dal vivo
  sul tasto per cronometro, punteggio e periodo, riconnessione e stato iniziale.
- [**Modulo Bitfocus Companion**](companion) (`companion/`): azioni, feedback e variabili per
  cronometro, punteggi, periodo e penalità, con riconnessione e stato iniziale.

Entrambi parlano al plugin **solo** tramite questo vendor; nessun protocollo aggiuntivo viene
aggiunto al plugin.

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
