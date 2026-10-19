<div align="center">

# 🏒 ScoreBoard for OBS

**A scoreboard for OBS Studio that writes the clock and every other value directly into your Text sources, the moment they change.**
**Un tabellone per OBS Studio che scrive il cronometro e tutti gli altri valori direttamente nelle tue sorgenti Text, nel momento in cui cambiano.**

[English](#english) · [Italiano](#italiano)

[![Latest release](https://img.shields.io/github/v/release/angeloruggieridj/obs-scoreboard?include_prereleases&sort=semver&label=release)](https://github.com/angeloruggieridj/obs-scoreboard/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/angeloruggieridj/obs-scoreboard/total?label=downloads)](https://github.com/angeloruggieridj/obs-scoreboard/releases)
[![License: GPL v2+](https://img.shields.io/badge/license-GPL--2.0--or--later-blue)](LICENSE)
![OBS Studio](https://img.shields.io/badge/OBS%20Studio-30.0%2B-302e31?logo=obsstudio)
![Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey)

[![Build](https://github.com/angeloruggieridj/obs-scoreboard/actions/workflows/push.yaml/badge.svg)](https://github.com/angeloruggieridj/obs-scoreboard/actions/workflows/push.yaml)
[![Core tests](https://github.com/angeloruggieridj/obs-scoreboard/actions/workflows/core-tests.yaml/badge.svg)](https://github.com/angeloruggieridj/obs-scoreboard/actions/workflows/core-tests.yaml)
[![Selftest](https://github.com/angeloruggieridj/obs-scoreboard/actions/workflows/selftest.yaml/badge.svg)](https://github.com/angeloruggieridj/obs-scoreboard/actions/workflows/selftest.yaml)
[![Integrations](https://github.com/angeloruggieridj/obs-scoreboard/actions/workflows/integrations.yaml/badge.svg)](https://github.com/angeloruggieridj/obs-scoreboard/actions/workflows/integrations.yaml)

<table>
  <tr>
    <td align="center"><img src="docs/images/dock-hockey.png" width="324" alt="The ScoreBoard dock in OBS Studio"><br><sub>The dock in OBS (ice hockey, before the match)</sub></td>
    <td align="center"><img src="docs/images/dock-hockey-live.svg" width="324" alt="The ScoreBoard dock during a match: clock 14:32, score 2-1, penalties"><br><sub>Illustration: a match in progress</sub></td>
  </tr>
</table>

</div>

## English

### The problem

Most scoreboard plugins write each value to a `.txt` file and let an OBS Text source read it. OBS only checks that
file once a second, and it decides whether to reload it by looking at the modification time, which is accurate to the
second. If two updates happen within the same second, the second one is missed. On stream the clock skips, for
example from `19:29` straight to `19:27`. Writing the files more carefully doesn't fix this.

ScoreBoard for OBS skips the files. It changes the text of your Text source through the OBS API as soon as a
value changes, and OBS draws it on the next frame. Your font, colors and position stay as they are.

### What it does

| | |
|---|---|
| **Direct updates** | 28 values (clock, period, team names, scores, shots, fouls, penalties, strength, play time and more), each linked to a Text source. A source is only touched when its text actually changes. The links hold up when you rename the source, switch scene, restart OBS or change scene collection. |
| **Clock** | Tenths of a second, counting up or down. The arrows add or remove a minute or a second and never go past the length of the period. Optional two-digit minutes and stoppage time. |
| **14 sports** | Ice hockey, basketball, soccer, futsal, American football, lacrosse, rugby union, rugby sevens, field hockey, water polo, handball, rink hockey, floorball and a generic one. Every preset can be edited. |
| **Timed penalties** | Up to 8 per team with 2 running at a time, compound penalties like 2+2, editing or cancelling a single penalty, and the strength text (`5-4`). |
| **The dock** | A narrow panel with everything on one screen and no scrolling. Score and foul colors are yours to change. The match is saved as you play, so you can resume it after a restart. |
| **Hotkeys** | 40 OBS hotkeys, saved with the scene collection. |
| **Remote control** | An obs-websocket vendor, a Stream Deck plugin and a Bitfocus Companion module. |
| **Extras** | Optional `.txt` output, an event log that exports YouTube chapters, and a notice when a new version is out. |

### Install

You need **OBS Studio 30.0 or newer**. Download the package for your system from the
[releases page](https://github.com/angeloruggieridj/obs-scoreboard/releases):

| System | Package |
|---|---|
| Windows | `…-windows-x64-setup.exe`, or the `.zip` for portable OBS (extract it into the plugins folder) |
| macOS | `…-macos-universal.pkg` (not signed: on first launch, right-click it and choose *Open*) |
| Linux (Ubuntu 24.04) | the `.deb` |

Close OBS, install, then turn on **Docks → ScoreBoard**. The first time, the plugin asks which sport you want.
Each release comes with SHA-256 checksums and a build attestation you can check with
`gh attestation verify <file> --repo angeloruggieridj/obs-scoreboard`. The OBS versions it has been tested on are
listed in [obs-compat.json](obs-compat.json).

### Getting started

1. Add a **Text** source for the clock. You can also let the plugin create it: dock menu **⋮ → Source links… → +**.
2. In *Source links…*, choose that source for **Clock**. Do the same for any other value you want on screen.
3. Press **Start**. OBS shows the new text on the very next frame.

### Remote control

| Client | Details |
|---|---|
| obs-websocket | Vendor `obs-scoreboard` with 20 requests and 4 events. It is looked up at run time, so the plugin works fine without obs-websocket. See [docs/remote-control.md](docs/remote-control.md). |
| Stream Deck | A plugin with one action per command and live feedback on the keys: [streamdeck/](streamdeck/). |
| Bitfocus Companion | A module with actions, feedbacks, variables and presets: [companion/](companion/). |

Turn the server on in OBS under **Tools → WebSocket Server Settings**. The Stream Deck plugin keeps the connection
password in its global settings as plain text.

### Output files and YouTube chapters

- **File output** is off by default. Under *Match settings → Output* you choose a folder and the plugin keeps one
  `.txt` per value in it, rewriting a file only when its content changes. It's meant for other tools: your Text
  sources are updated directly either way.
- **Event log…** (dock menu) lists every event with its time. You can export it as YouTube chapters (`0:00 Title`),
  counted from the start of the stream, the start of the recording or the game time, and copy it or save it as a
  `.txt`. It only produces text; nothing is written into the recording.

### Updates

Once each time OBS starts, the plugin asks GitHub for the latest release. The only thing it sends is its own version
number. If a newer one exists, **Update available** shows up in the dock menu and opens the release page. The plugin
never downloads or installs anything by itself, and you can turn the check off from the same menu.

### Building from source

You need CMake, a C++17 compiler, Qt 6 and the OBS Studio 30.0+ development files. The
[CMake presets](CMakePresets.json) do most of the work.

```bash
cmake -S tests -B build_tests && cmake --build build_tests --config Debug      # core tests, no OBS or Qt needed
ctest --test-dir build_tests -C Debug --output-on-failure
cmake --preset windows-x64 && cmake --build --preset windows-x64               # the plugin (also: macos, ubuntu-x86_64)
```

More details, including how to load a build into OBS and check the translations, are in
[CONTRIBUTING.md](CONTRIBUTING.md). The history is in [CHANGELOG.md](CHANGELOG.md). Bugs and ideas go to the
[issue tracker](https://github.com/angeloruggieridj/obs-scoreboard/issues).

### License

GPL-2.0-or-later, the same as OBS Studio: see [LICENSE](LICENSE). The Companion module in [companion/](companion/)
is MIT. The clock font, [DSEG7](third_party/dseg/DSEG-LICENSE.txt), is under the SIL Open Font License 1.1.

---

## Italiano

### Il problema

La maggior parte dei plugin per tabelloni scrive ogni valore in un file `.txt` e lascia a una sorgente Text di OBS il
compito di leggerlo. OBS controlla quel file una volta al secondo e, per decidere se rileggerlo, guarda la data di
modifica, che è precisa solo al secondo. Se due aggiornamenti arrivano nello stesso secondo, il secondo va perso. In
diretta il cronometro salta: per esempio passa da `19:29` a `19:27`. Scrivere i file con più cura non risolve il problema.

ScoreBoard for OBS salta i file. Cambia il testo della tua sorgente Text tramite le API di OBS appena il valore
cambia, e OBS lo disegna al fotogramma successivo. Font, colori e posizione restano come li hai scelti.

### Cosa fa

| | |
|---|---|
| **Aggiornamento diretto** | 28 valori (cronometro, periodo, nomi delle squadre, punteggi, tiri, falli, penalità, inferiorità numerica, tempo di gioco e altri), ognuno collegato a una sorgente Text. La sorgente viene toccata solo quando il testo cambia davvero. I collegamenti reggono se rinomini la sorgente, cambi scena, riavvii OBS o cambi collezione di scene. |
| **Cronometro** | Decimi di secondo, in avanti o all'indietro. Le frecce aggiungono o tolgono un minuto o un secondo senza mai superare la durata del periodo. Minuti a due cifre e recupero a scelta. |
| **14 sport** | Hockey su ghiaccio, basket, calcio, futsal, football americano, lacrosse, rugby a 15, rugby a 7, hockey su prato, pallanuoto, pallamano, hockey su pista, floorball e uno generico. Ogni preset si può modificare. |
| **Penalità a tempo** | Fino a 8 per squadra, 2 alla volta in corso, penalità composte come 2+2, modifica o annullamento della singola penalità e testo dell'inferiorità numerica (`5-4`). |
| **Il pannello** | Stretto, con tutto in una schermata e senza scorrimento. I colori di punteggi e falli si possono cambiare. La partita viene salvata mentre si gioca e dopo un riavvio si può riprendere. |
| **Hotkey** | 40 hotkey di OBS, salvate insieme alla collezione di scene. |
| **Controllo remoto** | Un vendor obs-websocket, un plugin per Stream Deck e un modulo per Bitfocus Companion. |
| **Extra** | Output `.txt` facoltativo, un registro eventi che esporta i capitoli per YouTube e un avviso quando esce una nuova versione. |

### Installazione

Serve **OBS Studio 30.0 o successivo**. Scarica il pacchetto per il tuo sistema dalla
[pagina delle release](https://github.com/angeloruggieridj/obs-scoreboard/releases):

| Sistema | Pacchetto |
|---|---|
| Windows | `…-windows-x64-setup.exe`, oppure lo `.zip` per OBS portatile (da estrarre nella cartella dei plugin) |
| macOS | `…-macos-universal.pkg` (non firmato: al primo avvio fai clic destro e scegli *Apri*) |
| Linux (Ubuntu 24.04) | il `.deb` |

Chiudi OBS, installa e poi attiva **Pannelli → ScoreBoard**. La prima volta il plugin ti chiede quale sport vuoi usare.
Ogni release include i checksum SHA-256 e un'attestazione della build, che puoi verificare con
`gh attestation verify <file> --repo angeloruggieridj/obs-scoreboard`. Le versioni di OBS su cui è stato provato sono
elencate in [obs-compat.json](obs-compat.json).

### Primi passi

1. Aggiungi una sorgente **Text** per il cronometro. Puoi anche farla creare al plugin: menu del pannello **⋮ → Collegamenti sorgenti… → +**.
2. In *Collegamenti sorgenti…* scegli quella sorgente per il **Cronometro**, e lo stesso per ogni altro valore che vuoi mostrare.
3. Premi **Avvia**. OBS mostra il nuovo testo già al fotogramma successivo.

### Controllo remoto

| Client | Dettagli |
|---|---|
| obs-websocket | Vendor `obs-scoreboard` con 20 richieste e 4 eventi. Viene cercato mentre il plugin gira, quindi il plugin funziona benissimo anche senza obs-websocket. Vedi [docs/remote-control.md](docs/remote-control.md). |
| Stream Deck | Un plugin con un'azione per ogni comando e feedback in tempo reale sui tasti: [streamdeck/](streamdeck/). |
| Bitfocus Companion | Un modulo con azioni, feedback, variabili e preset: [companion/](companion/). |

Attiva il server in OBS da **Strumenti → Impostazioni server WebSocket**. Il plugin per Stream Deck conserva la password
di connessione nelle sue impostazioni globali, in chiaro.

### Output su file e capitoli YouTube

- L'**output su file** è spento di default. In *Impostazioni partita → Output* scegli una cartella e il plugin ci tiene un
  `.txt` per ogni valore, riscrivendo un file solo quando il contenuto cambia. Serve per altri strumenti: le tue sorgenti
  Text vengono comunque aggiornate direttamente.
- **Registro eventi…** (menu del pannello) elenca ogni evento con il suo orario. Puoi esportarlo come capitoli per YouTube
  (`0:00 Titolo`), calcolati dall'inizio della diretta, dall'inizio della registrazione o dal tempo di gioco, e copiarlo
  oppure salvarlo in un `.txt`. Produce solo testo: nella registrazione non viene scritto nulla.

### Aggiornamenti

A ogni avvio di OBS il plugin chiede a GitHub qual è l'ultima release. L'unica informazione che invia è il proprio numero
di versione. Se ne esiste una più recente, nel menu del pannello compare **Aggiornamento disponibile**, che apre la pagina
della release. Il plugin non scarica né installa niente da solo, e puoi disattivare il controllo dallo stesso menu.

### Compilare dai sorgenti

Servono CMake, un compilatore C++17, Qt 6 e i file di sviluppo di OBS Studio 30.0 o successivo. I
[preset CMake](CMakePresets.json) fanno quasi tutto.

```bash
cmake -S tests -B build_tests && cmake --build build_tests --config Debug      # test del core, senza OBS né Qt
ctest --test-dir build_tests -C Debug --output-on-failure
cmake --preset windows-x64 && cmake --build --preset windows-x64               # il plugin (anche: macos, ubuntu-x86_64)
```

Altri dettagli, tra cui come caricare una build in OBS e controllare le traduzioni, sono in
[CONTRIBUTING.md](CONTRIBUTING.md). Lo storico è in [CHANGELOG.md](CHANGELOG.md). Per segnalare problemi o proporre idee
c'è il [tracker delle issue](https://github.com/angeloruggieridj/obs-scoreboard/issues).

### Licenza

GPL-2.0-or-later, come OBS Studio: vedi [LICENSE](LICENSE). Il modulo Companion in [companion/](companion/) è MIT.
Il font del cronometro, [DSEG7](third_party/dseg/DSEG-LICENSE.txt), è sotto la SIL Open Font License 1.1.
