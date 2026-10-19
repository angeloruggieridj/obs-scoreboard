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

### Features

- **Direct Text-source update** — link any scoreboard value (28 in total: clock, period, team names,
  scores, shots, fouls, penalties, strength, play time...) to a Text source from the dock menu
  (⋮ → *Source links…*, with a one-click "create a new Text source"). Updates happen only when the
  text really changes. Links survive renaming, scene changes, restarts and scene collections.
- **Precise clock** — tenths of a second, counting up or down, driven by a monotonic clock; ±1 minute /
  ±1 second arrows (never past the period length), two-digit minutes, stoppage time.
- **14 sports** — ice hockey, basketball, soccer, futsal, American football, lacrosse, rugby
  union and sevens, field hockey, water polo, handball, rink hockey, floorball and a generic one,
  each with editable defaults (periods, lengths, overtime, score buttons, labels).
- **Timed penalties** (hockey, lacrosse, rugby, ...) — up to 8 per team, 2 running at a time, compound
  penalties, editing and cancelling by penalty, and the "strength" text (`5-4`).
- **Compact dock** — everything visible at a glance in a narrow column, no scrolling; custom score and
  foul colors; the match is saved as you play and can be resumed after a restart.
- **Hotkeys** — 40 OBS hotkeys, saved in the scene collection.
- **Remote control** — an obs-websocket vendor, a native Stream Deck plugin and a Bitfocus Companion
  module (see below).
- **Optional extras** — `.txt` file output and YouTube chapters export (see below).

### Installation

Download the package for your system from the
[releases page](https://github.com/angeloruggieridj/obs-scoreboard/releases) (OBS Studio **30.0 or newer**):

| System | Package |
|---|---|
| Windows | `obs-scoreboard-<version>-windows-x64-setup.exe` (installer) or the `.zip` (extract into the OBS plugins folder, also for portable OBS) |
| macOS | `obs-scoreboard-<version>-macos-universal.pkg` — not signed: on first launch right-click → *Open* |
| Linux (Ubuntu 24.04) | the `.deb`, or the `.tar.xz` |

Close OBS before installing. Then open OBS and enable the dock from **Docks → ScoreBoard**. On the
first launch the plugin asks which sport you score.

Every release carries SHA-256 checksums and a
[build provenance attestation](https://docs.github.com/en/actions/security-for-github-actions/using-artifact-attestations)
(`gh attestation verify <file> --repo angeloruggieridj/obs-scoreboard`).

### Quick start

1. Add a **Text** source for the clock (or let the plugin create it: menu ⋮ → *Source links…* → **+**).
2. In *Source links…* pick that source for **Clock**, and the same for any other value you want on screen.
3. Press **Start** in the dock. The text changes in OBS on the very next frame — no files involved.

### Updates

Once per OBS start the plugin asks GitHub whether a newer release exists (only the version number is
exchanged, in the background; it never downloads or installs anything). If there is one, the dock menu
shows **Update available**, which opens the release page. Turn it off from the dock menu
(*Check for updates at start*).

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

### File output and YouTube chapters

Writing a `.txt` file per value is **optional and off by default**. Turn it on in
**Match settings → Output** (or the dock menu) and pick a folder: the plugin keeps one file per
value there, named after the field key (for example `home.score.txt`), and rewrites a file only
when its text changed, with an atomic write. Because OBS Text sources poll files at most once a
second and compare whole-second timestamps (see [Description](#description)), files are for
compatibility with other tools — the direct Text update remains the main output.

The dock's menu also has **Event log…**, a non-modal window (the dock itself stays compact):
it lists every event (period start/end, score, penalty, match end) with its time, team and
description, and exports **YouTube chapters** — the `0:00 Title` format — relative to the start of
your stream, the start of your recording or the game time. Copy the chapters to the clipboard or
save them to a `.txt` file. YouTube needs at least three chapters, ten seconds apart; the window
warns when there are fewer. Chapters are exported as **text**: the plugin does not write into a
recording file.

**Known limit:** a match resumed from a previous OBS session keeps the timestamps of that session.
With the stream/recording base the window ignores those older events, so only the events of the
current session appear there; the game-time base is unaffected.

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

### Funzionalità

- **Aggiornamento diretto delle sorgenti Text** — collega qualsiasi valore del tabellone (28 in totale:
  cronometro, periodo, nomi, punteggi, tiri, falli, penalità, inferiorità, tempo di gioco...) a una
  sorgente Text dal menu del pannello (⋮ → *Collegamenti sorgenti…*, con "crea una nuova sorgente
  Text" in un clic). L'aggiornamento avviene solo quando il testo cambia davvero. I collegamenti
  sopravvivono a rinomina, cambio scena, riavvio e collezioni di scene.
- **Cronometro preciso** — decimi di secondo, avanti o indietro, guidato da un orologio monotono;
  frecce ±1 minuto / ±1 secondo (mai oltre la durata del periodo), minuti a due cifre, recupero.
- **14 sport** — hockey su ghiaccio, basket, calcio, futsal, football americano, lacrosse, rugby a 15 e a 7,
  hockey su prato, pallanuoto, pallamano, hockey su pista, floorball e uno generico, con valori
  predefiniti modificabili (periodi, durate, supplementari, pulsanti punto, etichette).
- **Penalità a tempo** (hockey, lacrosse, rugby, ...) — fino a 8 per squadra, 2 in corso
  contemporaneamente, penalità composte, modifica e annullamento per singola penalità, e testo di
  inferiorità numerica (`5-4`).
- **Pannello compatto** — tutto visibile a colpo d'occhio in una colonna stretta, senza scorrimento; colori
  personalizzabili di punteggi e falli; la partita viene salvata mentre si gioca e si può riprendere
  dopo un riavvio.
- **Hotkey** — 40 hotkey OBS, salvate nella collezione di scene.
- **Controllo remoto** — un vendor obs-websocket, un plugin Stream Deck nativo e un modulo Bitfocus
  Companion (vedi sotto).
- **Extra opzionali** — output su file `.txt` ed esportazione dei capitoli YouTube (vedi sotto).

### Installazione

Scarica il pacchetto per il tuo sistema dalla
[pagina delle release](https://github.com/angeloruggieridj/obs-scoreboard/releases) (OBS Studio **30.0 o successivo**):

| Sistema | Pacchetto |
|---|---|
| Windows | `obs-scoreboard-<versione>-windows-x64-setup.exe` (installer) oppure lo `.zip` (da estrarre nella cartella dei plugin di OBS, anche per OBS portatile) |
| macOS | `obs-scoreboard-<versione>-macos-universal.pkg` — non firmato: al primo avvio tasto destro → *Apri* |
| Linux (Ubuntu 24.04) | il `.deb`, oppure il `.tar.xz` |

Chiudi OBS prima di installare. Poi apri OBS e attiva il pannello da **Pannelli → ScoreBoard**. Al
primo avvio il plugin chiede quale sport stai seguendo.

Ogni release include i checksum SHA-256 e un'
[attestazione di provenienza della build](https://docs.github.com/it/actions/security-for-github-actions/using-artifact-attestations)
(`gh attestation verify <file> --repo angeloruggieridj/obs-scoreboard`).

### Primi passi

1. Aggiungi una sorgente **Text** per il cronometro (oppure fai creare quella al plugin: menu ⋮ → *Collegamenti sorgenti…* → **+**).
2. In *Collegamenti sorgenti…* scegli quella sorgente per **Cronometro**, e lo stesso per ogni altro valore che vuoi a schermo.
3. Premi **Avvia** nel pannello. Il testo cambia in OBS al fotogramma successivo: nessun file di mezzo.

### Aggiornamenti

Una volta a ogni avvio di OBS il plugin chiede a GitHub se esiste una release più recente (viene scambiato
solo il numero di versione, in background; non scarica né installa nulla). Se c'è, il menu del pannello
mostra **Aggiornamento disponibile**, che apre la pagina della release. Si disattiva dal menu del pannello
(*Controlla gli aggiornamenti all'avvio*).

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

### Output su file e capitoli YouTube

La scrittura di un file `.txt` per ogni valore è **opzionale e spenta di default**. Attivala in
**Impostazioni partita → Output** (o dal menu del pannello) e scegli una cartella: il plugin vi
tiene un file per valore, col nome della chiave del campo (per esempio `home.score.txt`), e
riscrive un file solo quando il suo testo cambia, con scrittura atomica. Poiché le sorgenti Text di
OBS leggono i file al massimo una volta al secondo e confrontano date al secondo intero (vedi
[Descrizione](#descrizione)), i file servono per compatibilità con altri strumenti: l'aggiornamento
diretto del Text resta l'output principale.

Il menu del pannello ha anche **Registro eventi…**, una finestra non modale (il pannello resta
compatto): elenca ogni evento (inizio/fine periodo, punto, penalità, fine partita) con orario,
squadra e descrizione, ed esporta i **capitoli YouTube** — il formato `0:00 Titolo` — relativi
all'inizio della diretta, all'inizio della registrazione o al tempo di gioco. I capitoli si copiano
negli appunti o si salvano in un file `.txt`. YouTube richiede almeno tre capitoli, a dieci secondi
l'uno; la finestra lo segnala quando sono meno. I capitoli sono esportati come **testo**: il plugin
non scrive in un file di registrazione.

**Limite noto:** una partita ripresa da una sessione OBS precedente conserva gli orari di quella
sessione. Con la base diretta/registrazione la finestra ignora quegli eventi più vecchi, quindi lì
compaiono solo gli eventi della sessione corrente; la base "tempo di gioco" non è toccata.

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
