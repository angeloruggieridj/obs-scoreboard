// ScoreBoard for OBS landing page: EN/IT toggle and the hero clock.
// English lives in the HTML (readable without JS); this file holds the Italian.
(function () {
  "use strict";

  var IT = {
    "nav.why": "Perché",
    "nav.features": "Funzioni",
    "nav.remote": "Remoto",
    "nav.download": "Scarica",
    "nav.sports": "Sport",
    "sp.eyebrow": "Sport",
    "sp.title": "Scegli lo sport. È già impostato.",
    "sp.body": "14 sport pronti con periodi, cronometro, punti e penalità. Scegline uno e gioca la partita: la demo usa gli stessi valori predefiniti, e in OBS si può cambiare tutto.",
    "sp.ot": "Supplementari",
    "sp.noot": "Nessun supplementare",
    "sp.open": "Segmento libero",
    "sp.down": "conta all’indietro",
    "sp.up": "conta in avanti",
    "sp.hockey": "Hockey su ghiaccio",
    "sp.hockey.x": "Tiri; penalità 2′, 5′, 10′, 2+2, 2+5, 2+10; inferiorità",
    "sp.basketball": "Basket",
    "sp.ot.basketball": "5:00, quanti ne servono",
    "sp.basketball.x": "Falli di squadra per quarto; punti +1 / +2 / +3",
    "sp.soccer": "Calcio",
    "sp.soccer.x": "Cartellini gialli e rossi; inferiorità dai rossi",
    "sp.futsal.x": "Falli accumulati, azzerati nel 2° tempo",
    "sp.football": "Football americano",
    "sp.football.x": "Flag; punti +6 / +1 / +2 / +3",
    "sp.ot.lacrosse": "4:00, quanti ne servono",
    "sp.lacrosse.x": "Tiri; penalità 30″, 1′, 2′, 3′; inferiorità",
    "sp.rugby": "Rugby a 15",
    "sp.rugby.x": "Punti +5 / +2 / +3; penalità 10′, 20′; inferiorità",
    "sp.sevens": "Rugby a 7",
    "sp.ot.sevens": "5:00, quanti ne servono",
    "sp.sevens.x": "Punti +5 / +2 / +3; penalità 2′; inferiorità",
    "sp.field": "Hockey su prato",
    "sp.field.x": "Penalità 2′, 5′, 10′; inferiorità",
    "sp.waterpolo": "Pallanuoto",
    "sp.waterpolo.x": "Tiri; espulsione 20″; inferiorità",
    "sp.handball": "Pallamano",
    "sp.handball.x": "Penalità 2′; inferiorità",
    "sp.rink": "Hockey su pista",
    "sp.rink.x": "Falli di squadra; penalità 2′, 4′; inferiorità",
    "sp.floorball.x": "Tiri; penalità 2′, 5′, 10′, 2+2, 2+10; inferiorità",
    "sp.generic": "Generico",
    "sp.generic.x": "Cronometro e punteggio semplici per qualsiasi altro sport",
    "hero.title": "Nessun secondo saltato in diretta.",
    "hero.lead": "Un tabellone che scrive direttamente nelle tue sorgenti Text: il cronometro in diretta non salta mai. Nessun file .txt di mezzo.",
    "hero.how": "Scopri come funziona ›",
    "duel.file": "Sorgente Text che legge un file .txt",
    "duel.direct": "ScoreBoard for OBS",
    "duel.lost": "Secondi persi",
    "duel.note": "I due cronometri partono insieme. Quello a sinistra è mostrato come OBS legge un file: circa una volta al secondo, in base alla data di modifica.",
    "why.eyebrow": "Il problema",
    "why.title": "I file di testo fanno saltare il cronometro.",
    "why.body": "OBS rilegge il file circa una volta al secondo e ne confronta la data di modifica, al secondo. Due aggiornamenti nello stesso secondo, e uno si perde. La richiesta di correggerlo è stata chiusa come “non pianificata” (<a href=\"https://github.com/obsproject/obs-studio/issues/3012\">obs-studio #3012</a>).",
    "miss.cap": "Il 19:28 non è mai andato in onda.",
    "try.start": "Avvia",
    "try.note": "Valori predefiniti dei preset: nel plugin si può cambiare tutto.",
    "sol.eyebrow": "La soluzione",
    "sol.title": "Tieni le tue sorgenti Text.",
    "sol.body": "ScoreBoard ne cambia solo il testo, tramite le API di OBS, nel momento in cui un valore cambia. Font, colori e posizione restano tuoi; OBS mostra la modifica al fotogramma successivo.",
    "sol.cap1": "Hockey su ghiaccio, tema scuro",
    "sol.cap2": "Calcio, tema chiaro",
    "sol.cap3": "Colori delle squadre, avanzamento del periodo",
    "stats.values": "valori collegabili",
    "stats.sports": "sport, già pronti",
    "stats.hotkeys": "hotkey",
    "stats.tenths": "di secondo, risoluzione del cronometro",
    "feat.eyebrow": "Funzioni",
    "feat.title": "Fatto per le partite in diretta.",
    "feat.rename.t": "I collegamenti reggono alle rinomine.",
    "feat.rename.b": "I collegamenti resistono a rinomine, cambi di scena, riavvii e collezioni di scene. Se elimini la sorgente, il collegamento si scioglie e basta.",
    "feat.chap.t": "I capitoli si scrivono da soli.",
    "feat.chap.b": "Il registro eventi annota periodi, goal e penalità con l’orario e li esporta come capitoli di YouTube.",
    "feat.pen.t": "Penalità che si mettono in coda da sole.",
    "feat.pen.b": "Fino a 8 per squadra, due in corso, le altre in attesa. Composte 2+2, 2+5 e 2+10, con l’inferiorità numerica (5-4) calcolata per te.",
    "feat.clock.t": "Al decimo di secondo.",
    "feat.clock.b": "In avanti o indietro, da un orologio monotono. Le frecce non superano mai la durata del periodo né scendono sotto zero.",
    "feat.hot.t": "Comandi da tastiera.",
    "feat.hot.b": "40 hotkey di OBS per cronometro, punteggi, periodi e penalità, salvate con la tua collezione di scene.",
    "feat.only.t": "Nessun ridisegno inutile.",
    "feat.only.b": "Una sorgente viene scritta solo quando il suo testo cambia davvero: niente ridisegni inutili. L’uscita su file .txt resta, se serve a un altro strumento.",
    "rem.eyebrow": "Controllo remoto",
    "rem.title": "Comandalo dal telefono.",
    "rem.body": "Apri un link su un telefono nella stessa rete. Un link serve solo a guardare; l’altro comanda anche cronometro, punteggi e periodi.",
    "rem.sd": "Plugin per Stream Deck, con tasti che mostrano il punteggio in diretta.",
    "rem.comp": "Modulo per Bitfocus Companion: azioni, feedback, variabili, preset.",
    "rem.ws": "Richieste ed eventi obs-websocket per i tuoi strumenti.",
    "dl.title": "Gratis. Open source. OBS 30 o successivo.",
    "dl.body": "Chiudi OBS, installa, poi attiva Pannelli → ScoreBoard. Al primo avvio ti chiede lo sport.",
    "dl.win": "Installer .exe, oppure .zip per OBS portable",
    "dl.mac": ".pkg universale, Apple silicon e Intel",
    "dl.linux": ".deb per Ubuntu 24.04",
    "dl.go": "Scarica",
    "dl.soon": "Prossimamente",
    "dl.for": "Scarica per",
    "dl.other": "Anche per",
    "dl.sd": "Plugin facoltativo per Stream Deck 7.1 o successivo, con tasti in tempo reale. Richiede il server obs-websocket attivo in OBS.",
    "dl.fine": "Il pacchetto macOS non è firmato: la prima volta, clic destro e scegli Apri. Ogni release include i checksum SHA-256 e un’attestazione di build.",
    "foot.license": "GPL-2.0-or-later. Non affiliato a OBS Project.",
    "foot.remote": "Documentazione controllo remoto",
    "foot.issues": "Segnala un problema"
  };

  var nodes = document.querySelectorAll("[data-i18n]");
  var EN = {};
  nodes.forEach(function (n) { EN[n.getAttribute("data-i18n")] = n.innerHTML; });

  function store(lang) { try { localStorage.setItem("sb-lang", lang); } catch (e) { /* private mode */ } }
  function stored() { try { return localStorage.getItem("sb-lang"); } catch (e) { return null; } }

  var currentLang = "en";
  var langListeners = [];
  function setLang(lang) {
    currentLang = lang;
    var dict = lang === "it" ? IT : EN;
    nodes.forEach(function (n) {
      var v = dict[n.getAttribute("data-i18n")];
      if (v !== undefined && n.innerHTML !== v) n.innerHTML = v;
    });
    document.documentElement.lang = lang;
    document.querySelectorAll(".lang button").forEach(function (b) {
      b.setAttribute("aria-pressed", String(b.getAttribute("data-lang") === lang));
    });
    langListeners.forEach(function (fn) { fn(lang); });
  }

  document.querySelectorAll(".lang button").forEach(function (b) {
    b.addEventListener("click", function () {
      var lang = b.getAttribute("data-lang");
      store(lang);
      setLang(lang);
    });
  });

  var initial = stored() || ((navigator.language || "").toLowerCase().indexOf("it") === 0 ? "it" : "en");
  var REDUCE = !!(window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches);

  function fmt(s) { var m = Math.floor(s / 60), r = s % 60; return (m < 10 ? "0" : "") + m + ":" + (r < 10 ? "0" : "") + r; }
  function put(el, text) { if (el.textContent !== text) el.textContent = text; }
  function t(key) { var v = (currentLang === "it" ? IT : EN)[key]; return v === undefined ? EN[key] || "" : v; }
  function restartClass(el, cls) { el.classList.remove(cls); void el.offsetWidth; el.classList.add(cls); }
  // Calls on(true/false) when el enters/leaves the screen or the tab is hidden/shown.
  function whileVisible(el, threshold, on) {
    var inView = false, active = false;
    function update() {
      var want = inView && !document.hidden;
      if (want !== active) { active = want; on(want); }
    }
    if ("IntersectionObserver" in window) {
      new IntersectionObserver(function (e) { inView = e[0].isIntersecting; update(); }, { threshold: threshold }).observe(el);
    } else { inView = true; update(); }
    document.addEventListener("visibilitychange", update);
  }

  initDuel();
  initMiss();
  initPlay();
  initHub();
  initDownload();
  if (initial === "it") setLang("it");

  // Download: every download button names the visitor's system and, once GitHub answers,
  // points straight at that system's file in the latest release (the release page otherwise).
  function initDownload() {
    var get = document.getElementById("get");
    var buttons = document.querySelectorAll("[data-dl]");
    if (!buttons.length) return;
    var NAMES = { win: "Windows", mac: "macOS", linux: "Linux" };
    var ASSET = { win: /-windows-x64-setup\.exe$/, mac: /-macos-universal\.pkg$/, linux: /\.deb$/, sd: /\.streamDeckPlugin$/ };
    var RELEASES = buttons[0].getAttribute("href");
    var urls = {}, version = "";
    var pkg = get && get.querySelector("[data-dl-pkg]");
    var others = get && get.querySelector("[data-dl-others]");
    var sdRow = document.querySelector(".sd-row");
    var ua = (navigator.userAgent || "").toLowerCase();
    var plat = ((navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || "").toLowerCase();
    var os = /mac|iphone|ipad/.test(plat + ua) ? "mac" : (/linux/.test(plat + ua) && !/android/.test(ua)) ? "linux" : "win";

    function render() {
      buttons.forEach(function (b) {
        put(b.querySelector("[data-dl-label]"), t("dl.for") + " " + NAMES[os]);
        b.setAttribute("href", urls[os] || RELEASES);
      });
      if (pkg) put(pkg, t("dl." + os) + (version ? " · " + version : ""));
      if (sdRow && urls.sd) {
        sdRow.setAttribute("href", urls.sd);
        sdRow.querySelector("[data-sd-soon]").hidden = true;
        sdRow.querySelector("[data-sd-ready]").hidden = false;
      }
      if (!others) return;
      others.textContent = "";
      var pre = document.createElement("span");
      pre.textContent = t("dl.other"); pre.className = "get-also";
      others.appendChild(pre);
      Object.keys(NAMES).forEach(function (k) {
        if (k === os) return;
        var a = document.createElement("a");
        a.href = urls[k] || RELEASES; a.textContent = NAMES[k];
        a.addEventListener("click", function (e) { e.preventDefault(); os = k; render(); });
        others.appendChild(a);
      });
    }
    langListeners.push(render);
    render();

    if (!window.fetch) return;
    fetch("https://api.github.com/repos/angeloruggieridj/obs-scoreboard/releases/latest", { headers: { Accept: "application/vnd.github+json" } })
      .then(function (r) { return r.ok ? r.json() : null; })
      .then(function (rel) {
        if (!rel || !rel.assets) return;
        version = rel.tag_name || "";
        rel.assets.forEach(function (as) {
          Object.keys(ASSET).forEach(function (k) { if (!urls[k] && ASSET[k].test(as.name)) urls[k] = as.browser_download_url; });
        });
        render();
      })
      .catch(function () { /* offline or rate-limited: the release page still works */ });
  }

  // Hero duel: two clocks start together at 20:00 and count down.
  // "direct" changes exactly when the second changes (ScoreBoard writes the source text).
  // "file" only shows what it finds at each file check: about once a second, with the check
  // drifting against the clock, as OBS does when a Text source reads from a file. When two
  // changes fall between two checks, one second never airs.
  function initDuel() {
  var duel = document.getElementById("duel");
  if (!duel) return;
  var fileEl = duel.querySelector('[data-clock="file"]');
  var directEl = duel.querySelector('[data-clock="direct"]');
  var lostEl = duel.querySelector("[data-lost]");
  var ghostEl = duel.querySelector("[data-ghost]");
  var fileField = duel.querySelector(".screen-file .field");
  var reduce = window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches;

  var START = 20 * 60;    // 20:00
  var CHECK = 1.12;       // seconds between file checks (drifts against the 1 s clock)
  var FIRST = 0.35;       // first check, after the clock starts

  function fmt(s) { var m = Math.floor(s / 60), r = s % 60; return (m < 10 ? "0" : "") + m + ":" + (r < 10 ? "0" : "") + r; }
  function put(el, text) { if (el.textContent !== text) el.textContent = text; }

  function ghost(value) {
    ghostEl.textContent = fmt(value);
    ghostEl.classList.remove("is-on");
    void ghostEl.offsetWidth; // restart the animation
    ghostEl.classList.add("is-on");
    [fileEl, fileField].forEach(function (el) {
      el.classList.remove("is-skip");
      void el.offsetWidth;
      el.classList.add("is-skip");
    });
  }

  if (reduce) {
    put(fileEl, "19:27"); put(directEl, "19:27"); lostEl.textContent = "1";
    ghostEl.textContent = "19:28"; ghostEl.classList.add("is-static");
    return;
  }

  var t0, nextCheck, fileShown, lost, running = false;
  function reset() {
    t0 = performance.now(); nextCheck = FIRST; fileShown = START; lost = 0;
    put(fileEl, fmt(START)); put(directEl, fmt(START)); lostEl.textContent = "0";
  }
  function frame(now) {
    if (!running) return;
    var e = (now - t0) / 1000;
    var direct = START - Math.floor(e);
    if (direct < 0) { reset(); requestAnimationFrame(frame); return; }
    put(directEl, fmt(direct));
    while (e >= nextCheck) {
      var seen = START - Math.floor(nextCheck);
      if (seen < fileShown - 1) {
        lost += fileShown - seen - 1;
        lostEl.textContent = String(lost);
        ghost(fileShown - 1);
      }
      fileShown = seen;
      nextCheck += CHECK;
    }
    put(fileEl, fmt(fileShown));
    requestAnimationFrame(frame);
  }
  function start() { if (running) return; running = true; reset(); requestAnimationFrame(frame); }
  function stop() { running = false; }

  // Run only while the duel is on screen and the tab is visible; restart together each time.
  var visible = true;
  if ("IntersectionObserver" in window) {
    new IntersectionObserver(function (entries) {
      visible = entries[0].isIntersecting;
      if (visible && !document.hidden) start(); else stop();
    }, { threshold: 0.3 }).observe(duel);
  } else {
    start();
  }
  document.addEventListener("visibilitychange", function () {
    if (document.hidden) stop(); else if (visible) start();
  });
  }

  // The missing second: a pinned giant clock scrubbed by scroll, 19:30 → 19:29 → 19:27.
  function initMiss() {
    var miss = document.querySelector(".miss");
    if (!miss) return;
    var digits = miss.querySelector("[data-miss]");
    if (REDUCE) {
      miss.classList.add("is-static", "is-skipped");
      put(digits, "19:27");
      return;
    }
    var skipped = false, queued = false;
    function onScroll() {
      queued = false;
      var r = miss.getBoundingClientRect();
      var span = r.height - window.innerHeight;
      var p = span > 0 ? Math.min(1, Math.max(0, -r.top / span)) : 1;
      var now = p >= 0.45;
      put(digits, p < 0.22 ? "19:30" : now ? "19:27" : "19:29");
      if (now !== skipped) {
        skipped = now;
        miss.classList.toggle("is-skipped", now);
        if (now) restartClass(miss, "is-shake");
      }
    }
    window.addEventListener("scroll", function () {
      if (!queued) { queued = true; requestAnimationFrame(onScroll); }
    }, { passive: true });
    onScroll();
  }

  // Try it: a small scoreboard the visitor drives. Presets from the README sports table.
  function initPlay() {
    var play = document.getElementById("play");
    if (!play) return;
    // n = periods, ot = overtime text (or a key, or null), pen = demo penalty in seconds.
    var SPORTS = {
      hockey:     { n: 3, len: 20 * 60, down: true,  ot: "1 × 5:00",   pts: [1],          pen: 120, players: 5 },
      basketball: { n: 4, len: 10 * 60, down: true,  ot: "sp.ot.basketball", pts: [1, 2, 3] },
      soccer:     { n: 2, len: 45 * 60, down: false, ot: "2 × 15:00",  pts: [1] },
      futsal:     { n: 2, len: 20 * 60, down: true,  ot: "2 × 5:00",   pts: [1] },
      football:   { n: 4, len: 15 * 60, down: true,  ot: "1 × 10:00",  pts: [6, 1, 2, 3] },
      lacrosse:   { n: 4, len: 15 * 60, down: true,  ot: "sp.ot.lacrosse", pts: [1],     pen: 60,  players: 10 },
      rugby:      { n: 2, len: 40 * 60, down: false, ot: "2 × 10:00",  pts: [5, 2, 3],    pen: 600, players: 15 },
      sevens:     { n: 2, len: 7 * 60,  down: false, ot: "sp.ot.sevens", pts: [5, 2, 3],  pen: 120, players: 7 },
      field:      { n: 4, len: 15 * 60, down: true,  ot: null,         pts: [1],          pen: 120, players: 11 },
      waterpolo:  { n: 4, len: 8 * 60,  down: true,  ot: null,         pts: [1],          pen: 20,  players: 7 },
      handball:   { n: 2, len: 30 * 60, down: false, ot: "2 × 5:00",   pts: [1],          pen: 120, players: 7 },
      rink:       { n: 2, len: 25 * 60, down: true,  ot: "2 × 5:00",   pts: [1],          pen: 120, players: 5 },
      floorball:  { n: 3, len: 20 * 60, down: true,  ot: "1 × 10:00",  pts: [1],          pen: 120, players: 6 },
      generic:    { n: 0, len: 99 * 60 + 59, down: false, ot: null,    pts: [1] }
    };
    var specTime = play.querySelector("[data-spec-time]");
    var specDir = play.querySelector("[data-spec-dir]");
    var specDirLabel = play.querySelector("[data-spec-dirlabel]");
    var specOt = play.querySelector("[data-spec-ot]");
    var specX = play.querySelector("[data-spec-x]");
    var sportName;
    function renderSpec() {
      put(specTime, sport.n ? sport.n + " × " + fmt(sport.len).replace(/^0/, "") : t("sp.open"));
      put(specDir, sport.down ? "↓" : "↑");
      put(specDirLabel, t(sport.down ? "sp.down" : "sp.up"));
      put(specOt, sport.ot ? t("sp.ot") + " " + (sport.ot.indexOf("sp.") === 0 ? t(sport.ot) : sport.ot) : t("sp.noot"));
      put(specX, t("sp." + sportName + ".x"));
    }
    function penLabel(s) { return s < 60 ? s + "″" : (s / 60) + "′"; }
    var LABEL = { en: { start: "Start", stop: "Stop", period: "1st" }, it: { start: "Avvia", stop: "Ferma", period: "1°" } };
    var field = play.querySelector(".play-field");
    var clockEl = play.querySelector("[data-play-clock]");
    var periodEl = play.querySelector("[data-period]");
    var pensEl = play.querySelector("[data-pens]");
    var runBtn = play.querySelector("[data-run]");
    var scoreEl = { home: play.querySelector('[data-score="home"]'), away: play.querySelector('[data-score="away"]') };
    var btnsEl = { home: play.querySelector('[data-btns="home"]'), away: play.querySelector('[data-btns="away"]') };

    var sport, elapsed, running, last, score, pens, pensKey = "";

    function label(k) { return LABEL[currentLang][k]; }
    function runLabel() { put(runBtn.firstElementChild, label(running ? "stop" : "start")); runBtn.classList.toggle("is-running", running); }

    function setSport(name) {
      sport = SPORTS[name]; sportName = name; elapsed = 0; running = false; score = { home: 0, away: 0 }; pens = [];
      field.setAttribute("data-field", name);
      play.querySelectorAll("[data-sport]").forEach(function (b) { b.setAttribute("aria-pressed", String(b.getAttribute("data-sport") === name)); });
      ["home", "away"].forEach(function (team) {
        var box = btnsEl[team];
        box.textContent = "";
        sport.pts.forEach(function (n) { addBtn(box, "+" + n, function () { addScore(team, n); }); });
        if (sport.pen) addBtn(box, penLabel(sport.pen), function () { addPen(team); });
      });
      put(scoreEl.home, "0"); put(scoreEl.away, "0");
      runLabel(); render(); renderSpec();
    }
    function addBtn(box, text, fn) {
      var b = document.createElement("button");
      b.type = "button"; b.textContent = text; b.addEventListener("click", fn); box.appendChild(b);
    }
    function addScore(team, n) {
      score[team] += n;
      put(scoreEl[team], String(score[team]));
      restartClass(scoreEl[team], "is-pop");
    }
    function addPen(team) {
      if (pens.filter(function (p) { return p.team === team; }).length >= 2) return;
      pens.push({ team: team, left: sport.pen });
      render();
    }
    function clockText() {
      if (sport.down) return fmt(Math.ceil(sport.len - elapsed));
      return fmt(Math.floor(elapsed));
    }
    function render() {
      put(clockEl, clockText());
      put(periodEl, label("period"));
      var key = pens.map(function (p) { return p.team + Math.ceil(p.left); }).join(",");
      if (key === pensKey) return; // rebuild the pills only when their text changes
      pensKey = key;
      pensEl.textContent = "";
      pens.forEach(function (p) {
        var s = document.createElement("span");
        s.className = "pen " + p.team;
        s.textContent = (p.team === "home" ? "HOME " : "AWAY ") + fmt(Math.ceil(p.left));
        pensEl.appendChild(s);
      });
      if (pens.length && sport.players) {
        var h = sport.players - pens.filter(function (p) { return p.team === "home"; }).length;
        var a = sport.players - pens.filter(function (p) { return p.team === "away"; }).length;
        var st = document.createElement("span");
        st.className = "pen strength"; st.textContent = h + "-" + a;
        pensEl.appendChild(st);
      }
    }
    function frame(now) {
      if (!running) return;
      var dt = (now - last) / 1000; last = now;
      elapsed = Math.min(sport.len, elapsed + dt);
      pens.forEach(function (p) { p.left -= dt; });
      pens = pens.filter(function (p) { return p.left > 0; });
      if (elapsed >= sport.len) { running = false; runLabel(); }
      render();
      if (running) requestAnimationFrame(frame);
    }
    function setRunning(on) {
      if (on === running) return;
      if (on && elapsed >= sport.len) elapsed = 0;
      running = on; runLabel();
      if (on) { last = performance.now(); requestAnimationFrame(frame); }
    }

    runBtn.addEventListener("click", function () { setRunning(!running); });
    play.querySelectorAll("[data-sport]").forEach(function (b) {
      b.addEventListener("click", function () { setSport(b.getAttribute("data-sport")); });
    });
    langListeners.push(function () { runLabel(); render(); renderSpec(); });
    whileVisible(play, 0.1, function (on) { if (!on) setRunning(false); });
    setSport("hockey");
  }

  // One tap, every surface: the phone sends +1 and OBS, Stream Deck and Companion follow.
  function initHub() {
    var hub = document.getElementById("hub");
    if (!hub) return;
    var shown = hub.querySelectorAll("[data-s]");
    var phone = hub.querySelector(".phone-score");
    var score = 2, timer = null;
    function setAll(n) { shown.forEach(function (e) { put(e, String(n)); }); }
    if (REDUCE) return;
    function fire() {
      score = score >= 6 ? 2 : score + 1;
      if (score === 2) { setAll(2); return; } // quiet reset, no animation
      restartClass(hub, "fire");
      setTimeout(function () { put(phone, String(score)); }, 150);
      setTimeout(function () { shown.forEach(function (e) { if (e !== phone) put(e, String(score)); }); }, 640);
    }
    whileVisible(hub, 0.4, function (on) {
      clearInterval(timer);
      if (on) { setTimeout(fire, 600); timer = setInterval(fire, 3200); }
    });
  }
})();
