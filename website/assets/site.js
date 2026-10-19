// HP Tracker website: language switch (EN/ES), the download button for the visitor's
// system, the latest release's version and sizes from GitHub, and the small demo.
(function () {
  "use strict";

  const REPO = "imboro/hp-tracker";
  const DL = {
    win: "https://github.com/" + REPO + "/releases/latest/download/HP-Tracker-Windows.zip",
    mac: "https://github.com/" + REPO + "/releases/latest/download/HP-Tracker-Mac.zip",
  };

  // ---------------------------------------------------------------- texts
  const ES = {
    "nav.features": "Funciones",
    "nav.phones": "Pantallas de jugadores",
    "nav.download": "Descargar",
    "hero.eyebrow": "Tracker de combate gratuito para 5e",
    "hero.title": "Que el combate no se pare.",
    "hero.lead": "HP Tracker lleva la cuenta de los puntos de golpe, los turnos, las condiciones y las salvaciones de muerte de tu mesa, para que tú puedas mirar a los jugadores.",
    "dl.mac": "Descargar para Mac",
    "dl.win": "Descargar para Windows",
    "hero.free": "Gratis",
    "hero.version": "Versión",
    "hero.macreq": "Mac con Apple Silicon",
    "st.title": "No sustituye a la mesa. Lleva la cuenta.",
    "st.p1": "Los dados, las miniaturas, las voces y las discusiones de reglas se quedan en la mesa. HP Tracker solo lleva las cuentas: de quién es el turno, cuánto daño pasa la resistencia, quién arriesga su concentración.",
    "st.p2": "Pruébalo: quítale vida al goblin, cura a Aria y pasa el turno.",
    "demo.aria": "Ejemplo interactivo del orden de turno",
    "demo.turnorder": "Orden de turno",
    "demo.round": "Ronda",
    "demo.next": "Siguiente turno",
    "demo.reset": "Reiniciar",
    "demo.turn": "TURNO",
    "demo.damage": "Quitar 3",
    "demo.heal": "Curar 3",
    "ft.eyebrow": "Qué hace",
    "ft.title": "Todo lo que hay que vigilar en un combate",
    "f1.t": "Orden de turno a mano",
    "f1.p": "Arrastra las tarjetas al orden que acordó tu mesa. Un clic pasa el turno y las rondas se cuentan solas.",
    "f2.t": "Daño bien aplicado",
    "f2.p": "Resistencias, vulnerabilidades, inmunidades y vida temporal se aplican solas. Daña a un grupo entero de una vez, o la mitad a quien superó la salvación.",
    "f3.t": "Muerte y concentración",
    "f3.p": "Los jugadores a 0 PG tienen sus casillas de salvación. Si un lanzador concentrado recibe daño, aparece la CD de la tirada.",
    "f4.t": "Condiciones que se acaban",
    "f4.p": "¿Envenenado 3 rondas? Se descuenta solo y te avisa cuando termina.",
    "f5.t": "Grupo y bestiario",
    "f5.p": "Guarda a tus jugadores una vez y recupéralos cada sesión. Añade tres goblins de un clic.",
    "f6.t": "Dificultad del encuentro",
    "f6.p": "De Fácil a Mortal, según los niveles del grupo y el VD de los monstruos, con la experiencia que dan.",
    "f7.t": "Deshacer cualquier cosa",
    "f7.p": "¿Objetivo equivocado? Cmd+Z o Ctrl+Z lo devuelve, sea lo que sea.",
    "f8.t": "Se guarda solo",
    "f8.p": "Ciérralo a mitad de combate y sigue la semana que viene donde lo dejaste.",
    "cap.setup": "Prepara el encuentro y pulsa Empezar combate.",
    "cap.alerts": "Los enemigos son tarjetas oscuras. Los avisos te dicen lo que piden las reglas: tiradas de concentración, fallos de salvación.",
    "tour.aria": "Pantallas",
    "tab.setup": "Preparación",
    "tab.alerts": "Enemigos y avisos",
    "tab.looks": "Aspecto",
    "cap.looks": "Un color y un icono para cada personaje.",
    "alt.combat": "HP Tracker en combate: la barra lateral, el orden de turno con jugadores y enemigos en oscuro, el panel con el jugador que tiene el turno y la columna de derrotados.",
    "alt.setup": "La pantalla de preparación: el grupo en el orden de turno y el botón para empezar el combate.",
    "alt.alerts": "Un enemigo en el panel grande, con avisos de una tirada de concentración y un fallo de salvación de muerte.",
    "alt.looks": "Eligiendo color e icono para un personaje.",
    "alt.phone": "El móvil de una jugadora: la vida de Aria, el orden de turno y el aviso de que es su turno.",
    "ph.eyebrow": "Pantallas de jugadores",
    "ph.title": "Tus jugadores siguen el combate en el móvil",
    "ph.p": "Enseña un código QR, los jugadores lo escanean y eligen su personaje. Cada uno ve su vida, el orden de turno y un aviso de «¡Es tu turno!» cuando le toca.",
    "ph.l1": "No hay que instalar nada en los móviles.",
    "ph.l2": "Solo lectura: los móviles miran, no cambian nada.",
    "ph.l3": "Los enemigos salen solo con su nombre. Nada de chivatazos sobre su vida.",
    "ph.l4": "Se queda en tu wifi: nada sale a internet.",
    "dl.eyebrow": "Descargar",
    "dl.title": "Gratis, para Windows y Mac",
    "dl.noinstall": "No hay que instalar nada: descomprime y abre.",
    "dl.winreq": "Windows 10 u 11, 64 bits",
    "dl.macreq": "Apple Silicon (M1 o posterior)",
    "dl.get": "Descargar",
    "dl.w1": "Clic derecho en el zip y elige Extraer todo.",
    "dl.w2": "Abre la carpeta y haz doble clic en HP Tracker.exe (deja SDL2.dll a su lado).",
    "dl.w3": "Si sale «Windows protegió su PC»: Más información y luego Ejecutar de todas formas. Solo lo pregunta una vez.",
    "dl.m1": "Haz doble clic en el zip y arrastra HP Tracker a Aplicaciones.",
    "dl.m2": "La primera vez, clic derecho en la app, Abrir y otra vez Abrir.",
    "dl.m3": "¿No sale el botón Abrir? Ve a Ajustes del Sistema, Privacidad y seguridad, y pulsa Abrir igualmente.",
    "dl.why": "¿Por qué esos avisos? La app no está firmada con un certificado de desarrollador de pago, así que Windows y macOS piden confirmación la primera vez. No necesita internet y no envía nada a ningún sitio.",
    "faq.eyebrow": "Preguntas",
    "faq.title": "Para que lo sepas",
    "q1.q": "¿Es gratis?",
    "q1.a": "Sí. Sin cuenta, sin anuncios y sin suscripción.",
    "q2.q": "¿Necesita internet?",
    "q2.a": "No. Las pantallas de jugadores solo necesitan que los móviles estén en la misma wifi que el ordenador.",
    "q3.q": "¿Mac con Intel, Linux, iPad?",
    "q3.a": "Todavía no. Hoy funciona en Windows 10/11 y en Mac con Apple Silicon. Móviles y tablets pueden seguir el combate con las pantallas de jugadores.",
    "q4.q": "¿Dónde se guardan mis combates?",
    "q4.a": "Solo en tu ordenador. Windows: %APPDATA%\\HotelWorkshop\\HPTracker. Mac: ~/Library/Application Support/HotelWorkshop/HPTracker. Al actualizar la app se conservan.",
    "q5.q": "¿Es un producto oficial?",
    "q5.a": "No. HP Tracker es una herramienta independiente hecha por un fan para partidas con las reglas de quinta edición. No está afiliada a Wizards of the Coast ni respaldada por ella.",
    "q6.q": "¿En qué idiomas está?",
    "q6.a": "Inglés, castellano y catalán. Cámbialo con EN · ES · CA arriba en la barra lateral de la app; los móviles de los jugadores siguen el mismo idioma.",
    "ft.made": "Hecho por",
    "ft.credits": "Créditos y licencias",
  };
  const CA = {
    "nav.features": "Funcions",
    "nav.phones": "Pantalles de jugadors",
    "nav.download": "Descarregar",
    "hero.eyebrow": "Tracker de combat gratuït per a 5e",
    "hero.title": "Que el combat no s'aturi.",
    "hero.lead": "HP Tracker porta el compte dels punts de vida, els torns, els estats i les salvacions de mort de la teva taula, perquè tu puguis mirar els jugadors.",
    "dl.mac": "Descarregar per a Mac",
    "dl.win": "Descarregar per a Windows",
    "hero.free": "Gratuït",
    "hero.version": "Versió",
    "hero.macreq": "Mac amb Apple Silicon",
    "st.title": "No substitueix la taula. Porta el compte.",
    "st.p1": "Els daus, les miniatures, les veus i les discussions de regles es queden a la taula. HP Tracker només porta els comptes: de qui és el torn, quant dany passa la resistència, qui arrisca la concentració.",
    "st.p2": "Prova-ho: treu vida al goblin, cura l'Aria i passa el torn.",
    "demo.aria": "Exemple interactiu de l'ordre de torn",
    "demo.turnorder": "Ordre de torn",
    "demo.round": "Ronda",
    "demo.next": "Torn següent",
    "demo.reset": "Reiniciar",
    "demo.turn": "TORN",
    "demo.damage": "Treure 3",
    "demo.heal": "Curar 3",
    "ft.eyebrow": "Què fa",
    "ft.title": "Tot el que cal vigilar en un combat",
    "f1.t": "Ordre de torn a mà",
    "f1.p": "Arrossega les targetes a l'ordre que ha acordat la teva taula. Un clic passa el torn i les rondes es compten soles.",
    "f2.t": "Dany ben aplicat",
    "f2.p": "Resistències, vulnerabilitats, immunitats i vida temporal s'apliquen soles. Fes mal a tot un grup de cop, o la meitat a qui ha superat la salvació.",
    "f3.t": "Mort i concentració",
    "f3.p": "Els jugadors a 0 PV tenen les caselles de salvació. Si un llançador concentrat rep dany, apareix la CD de la tirada.",
    "f4.t": "Estats que s'acaben",
    "f4.p": "Enverinat 3 rondes? Es descompta sol i t'avisa quan s'acaba.",
    "f5.t": "Grup i bestiari",
    "f5.p": "Desa els teus jugadors un cop i recupera'ls cada sessió. Afegeix tres goblins d'un clic.",
    "f6.t": "Dificultat de l'encontre",
    "f6.p": "De Fàcil a Mortal, segons els nivells del grup i el VD dels monstres, amb l'experiència que donen.",
    "f7.t": "Desfer qualsevol cosa",
    "f7.p": "Objectiu equivocat? Cmd+Z o Ctrl+Z ho torna enrere, sigui el que sigui.",
    "f8.t": "Es desa sol",
    "f8.p": "Tanca'l a mig combat i continua la setmana vinent on ho havies deixat.",
    "cap.setup": "Prepara l'encontre i prem Començar combat.",
    "cap.alerts": "Els enemics són targetes fosques. Els avisos et diuen el que demanen les regles: tirades de concentració, fallades de salvació.",
    "tour.aria": "Pantalles",
    "tab.setup": "Preparació",
    "tab.alerts": "Enemics i avisos",
    "tab.looks": "Aspecte",
    "cap.looks": "Un color i una icona per a cada personatge.",
    "alt.combat": "HP Tracker en combat: la barra lateral, l'ordre de torn amb jugadors i enemics en fosc, el panell amb el jugador que té el torn i la columna de derrotats.",
    "alt.setup": "La pantalla de preparació: el grup a l'ordre de torn i el botó per començar el combat.",
    "alt.alerts": "Un enemic al panell gran, amb avisos d'una tirada de concentració i una fallada de salvació de mort.",
    "alt.looks": "Triant el color i la icona d'un personatge.",
    "alt.phone": "El mòbil d'una jugadora: la vida de l'Aria, l'ordre de torn i l'avís que és el seu torn.",
    "ph.eyebrow": "Pantalles de jugadors",
    "ph.title": "Els teus jugadors segueixen el combat al mòbil",
    "ph.p": "Mostra un codi QR, els jugadors l'escanegen i trien el seu personatge. Cadascú veu la seva vida, l'ordre de torn i un avís de «És el teu torn!» quan li toca.",
    "ph.l1": "No cal instal·lar res als mòbils.",
    "ph.l2": "Només lectura: els mòbils miren, no canvien res.",
    "ph.l3": "Els enemics surten només amb el nom. Res de xivatades sobre la seva vida.",
    "ph.l4": "Es queda a la teva wifi: res no surt a internet.",
    "dl.eyebrow": "Descarregar",
    "dl.title": "Gratuït, per a Windows i Mac",
    "dl.noinstall": "No cal instal·lar res: descomprimeix i obre.",
    "dl.winreq": "Windows 10 o 11, 64 bits",
    "dl.macreq": "Apple Silicon (M1 o posterior)",
    "dl.get": "Descarregar",
    "dl.w1": "Clic dret al zip i tria Extreu-ho tot.",
    "dl.w2": "Obre la carpeta i fes doble clic a HP Tracker.exe (deixa SDL2.dll al costat).",
    "dl.w3": "Si surt «Windows ha protegit l'ordinador»: Més informació i després Executa igualment. Només ho pregunta un cop.",
    "dl.m1": "Fes doble clic al zip i arrossega HP Tracker a Aplicacions.",
    "dl.m2": "El primer cop, clic dret a l'app, Obre i un altre cop Obre.",
    "dl.m3": "No surt el botó Obre? Ves a Configuració del Sistema, Privadesa i seguretat, i prem Obre igualment.",
    "dl.why": "Per què aquests avisos? L'app no està signada amb un certificat de desenvolupador de pagament, així que Windows i macOS demanen confirmació el primer cop. No necessita internet i no envia res enlloc.",
    "faq.eyebrow": "Preguntes",
    "faq.title": "Perquè ho sàpigues",
    "q1.q": "És gratuït?",
    "q1.a": "Sí. Sense compte, sense anuncis i sense subscripció.",
    "q2.q": "Necessita internet?",
    "q2.a": "No. Les pantalles de jugadors només necessiten que els mòbils estiguin a la mateixa wifi que l'ordinador.",
    "q3.q": "Mac amb Intel, Linux, iPad?",
    "q3.a": "Encara no. Avui funciona a Windows 10/11 i a Mac amb Apple Silicon. Mòbils i tauletes poden seguir el combat amb les pantalles de jugadors.",
    "q4.q": "On es desen els meus combats?",
    "q4.a": "Només al teu ordinador. Windows: %APPDATA%\\HotelWorkshop\\HPTracker. Mac: ~/Library/Application Support/HotelWorkshop/HPTracker. En actualitzar l'app es conserven.",
    "q5.q": "És un producte oficial?",
    "q5.a": "No. HP Tracker és una eina independent feta per un fan per a partides amb les regles de cinquena edició. No està afiliada a Wizards of the Coast ni avalada per ella.",
    "q6.q": "En quins idiomes està?",
    "q6.a": "Anglès, castellà i català. Canvia-ho amb EN · ES · CA a dalt de la barra lateral de l'app; els mòbils dels jugadors segueixen el mateix idioma.",
    "ft.made": "Fet per",
    "ft.credits": "Crèdits i llicències",
  };
  const EN = {};  // English lives in the HTML; filled from it on load.
  const EN_EXTRA = { "demo.turn": "TURN", "demo.damage": "Damage 3", "demo.heal": "Heal 3" };

  let lang = "en";
  const DICT = { es: ES, ca: CA };
  const t = (key) => (DICT[lang] && DICT[lang][key]) || EN[key] || EN_EXTRA[key] || key;

  function applyLang(next) {
    lang = next;
    document.documentElement.lang = lang;
    document.querySelectorAll("[data-i18n]").forEach((el) => { el.textContent = t(el.dataset.i18n); });
    document.querySelectorAll("[data-i18n-alt]").forEach((el) => { el.alt = t(el.dataset.i18nAlt); });
    document.querySelectorAll("[data-i18n-aria]").forEach((el) => { el.setAttribute("aria-label", t(el.dataset.i18nAria)); });
    const seg = document.getElementById("lang");
    seg.dataset.active = String(["en", "es", "ca"].indexOf(lang));
    seg.querySelectorAll("button").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.lang === lang)));
    showRelease();
    renderDemo();
    try { localStorage.setItem("hpTrackerLang", lang); } catch (e) { /* private mode */ }
  }

  document.querySelectorAll("[data-i18n]").forEach((el) => { EN[el.dataset.i18n] = el.textContent; });
  document.querySelectorAll("[data-i18n-alt]").forEach((el) => { EN[el.dataset.i18nAlt] = el.alt; });
  document.querySelectorAll("[data-i18n-aria]").forEach((el) => { EN[el.dataset.i18nAria] = el.getAttribute("aria-label"); });

  // ---------------------------------------------------------------- downloads
  const ua = navigator.userAgent;
  const mobile = /Android|iPhone|iPad|iPod/i.test(ua) || (/Macintosh/.test(ua) && navigator.maxTouchPoints > 1);
  const os = mobile ? null : /Windows/i.test(ua) ? "win" : /Macintosh|Mac OS X/i.test(ua) ? "mac" : null;
  {
    const main = document.getElementById("ctaMain"), alt = document.getElementById("ctaAlt");
    if (os === "win") {  // Windows first
      main.dataset.dl = "win"; main.querySelector(".os-label").dataset.i18n = "dl.win";
      alt.dataset.dl = "mac"; alt.querySelector(".os-label").dataset.i18n = "dl.mac";
    }
    // On a computer the hero buttons download directly; on a phone they scroll to the
    // download section (the app runs on computers).
    if (os) {
      main.href = DL[main.dataset.dl];
      alt.href = DL[alt.dataset.dl];
      document.getElementById(os === "win" ? "dlWin" : "dlMac").classList.add("suggested");
    }
  }

  let release = null;
  function showRelease() {
    if (!release) return;
    const version = String(release.tag_name || "").replace(/^v/, "");
    if (version) document.querySelectorAll(".js-version").forEach((el) => { el.textContent = version; });
    if (release.published_at) {
      const d = new Date(release.published_at);
      const text = d.toLocaleDateString({ es: "es-ES", ca: "ca-ES" }[lang] || "en-GB", { day: "numeric", month: "long", year: "numeric" });
      document.querySelectorAll(".js-date").forEach((el) => { el.textContent = " · " + text; });
    }
    for (const a of release.assets || []) {
      const mb = (a.size / 1048576).toFixed(1).replace(".", lang === "en" ? "." : ",") + " MB";
      if (/Windows/i.test(a.name)) document.querySelectorAll(".js-size-win").forEach((el) => { el.textContent = "· " + mb; });
      if (/Mac/i.test(a.name)) document.querySelectorAll(".js-size-mac").forEach((el) => { el.textContent = "· " + mb; });
    }
  }
  fetch("https://api.github.com/repos/" + REPO + "/releases/latest", { headers: { Accept: "application/vnd.github+json" } })
    .then((r) => (r.ok ? r.json() : null))
    .then((data) => { if (data) { release = data; showRelease(); } })
    .catch(() => { /* offline or rate-limited: the version in the page stays */ });

  // ---------------------------------------------------------------- demo
  const ICONS = {
    heart: '<path d="M12 20.5C4 15 2.8 9 6.2 6.4 9 4.3 11.2 6 12 8c.8-2 3-3.7 5.8-1.6C21.2 9 20 15 12 20.5Z"/>',
    bolt: '<path d="M14 2.5 6 13h6l-2 8.5L18 11h-6z"/>',
    skull: '<circle cx="12" cy="10" r="7"/><path d="M9 17v4h6v-4"/><circle cx="9.4" cy="10" r="1.2" fill="currentColor"/><circle cx="14.6" cy="10" r="1.2" fill="currentColor"/>',
  };
  const START = [
    { name: "Aria", enemy: false, icon: "heart", hp: 16, max: 22 },
    { name: "Goblin", enemy: true, icon: "bolt", hp: 7, max: 7 },
    { name: "Orc", enemy: true, icon: "skull", hp: 15, max: 15 },
  ];
  let cards = [], turn = 0, round = 1;
  const list = document.getElementById("demoList");
  const barColor = (f) => (f > 0.5 ? "var(--green)" : f > 0.25 ? "var(--amber)" : "var(--red)");
  const minus = '<svg viewBox="0 0 12 12"><path d="M2 6h8"/></svg>';
  const plus = '<svg viewBox="0 0 12 12"><path d="M2 6h8M6 2v8"/></svg>';

  function reset() {
    cards = START.map((c) => Object.assign({}, c));
    turn = 0; round = 1;
    renderDemo();
  }
  function renderDemo() {
    if (!list) return;
    document.getElementById("demoRound").textContent = round;
    list.innerHTML = "";
    cards.forEach((c, i) => {
      const dead = c.hp === 0;
      const el = document.createElement("div");
      el.className = "card" + (c.enemy ? " enemy" : "") + (dead ? " dead" : "") + (i === turn ? " turn" : "");
      const pct = (100 * c.hp) / c.max;
      el.innerHTML =
        '<div class="edge"></div><div class="face">' +
        '<div class="ico"><svg viewBox="0 0 24 24">' + ICONS[c.icon] + "</svg></div>" +
        '<div class="mid"><div class="row1"><span class="name"></span><span class="hp">' + c.hp + " <small>/ " + c.max + "</small></span></div>" +
        '<div class="bar"><i class="pre" style="width:' + pct + '%"></i><i class="fill" style="width:' + pct + "%;background:" + barColor(c.hp / c.max) + '"></i></div></div>' +
        '<div class="btns"><button type="button" class="mini dmg">' + minus + '</button><button type="button" class="mini heal">' + plus + "</button></div>" +
        '</div><span class="ribbon">&#9654; ' + t("demo.turn") + "</span>";
      el.querySelector(".name").textContent = c.name;
      const dmg = el.querySelector(".dmg"), heal = el.querySelector(".heal");
      dmg.setAttribute("aria-label", t("demo.damage") + " · " + c.name);
      heal.setAttribute("aria-label", t("demo.heal") + " · " + c.name);
      dmg.title = t("demo.damage");
      heal.title = t("demo.heal");
      dmg.addEventListener("click", () => change(i, -3, el));
      heal.addEventListener("click", () => change(i, +3, el));
      list.appendChild(el);
    });
  }
  // The bar animates like the app's: on a hit the fill drops at once and a yellow
  // pre-bar shows what was lost, then drains; on a heal a light green pre-bar shows
  // the gain first and the fill rises into it.
  function change(i, delta, el) {
    const c = cards[i];
    const before = c.hp;
    c.hp = Math.max(0, Math.min(c.max, c.hp + delta));
    if (c.hp === before) return;
    const pre = el.querySelector(".pre"), fill = el.querySelector(".fill");
    const pct = (100 * c.hp) / c.max;
    el.querySelector(".hp").innerHTML = c.hp + " <small>/ " + c.max + "</small>";
    if (delta < 0) {
      pre.classList.remove("heal"); fill.classList.remove("drop");
      fill.style.width = pct + "%";
      fill.style.background = barColor(c.hp / c.max);
      pre.style.width = pct + "%";
      el.classList.remove("hit"); void el.offsetWidth; el.classList.add("hit");
    } else {
      pre.classList.add("heal"); fill.classList.add("drop");
      pre.style.width = pct + "%";
      fill.style.width = pct + "%";
      fill.style.background = barColor(c.hp / c.max);
    }
    const f = document.createElement("span");
    f.className = "float" + (delta > 0 ? " heal" : "");
    f.textContent = (delta > 0 ? "+" : "−") + Math.abs(c.hp - before);
    el.appendChild(f);
    setTimeout(() => f.remove(), 1000);
    if ((c.hp === 0) !== (before === 0)) {
      setTimeout(() => {
        if (c.hp === 0 && turn === i) nextTurn(false);
        else renderDemo();
      }, 950);
    }
  }
  function nextTurn(countRound) {
    const n = cards.length;
    for (let step = 1; step <= n; step++) {
      const idx = (turn + step) % n;
      if (cards[idx].hp === 0) continue;
      if (idx <= turn && countRound !== false) round++;
      turn = idx;
      break;
    }
    renderDemo();
  }
  document.getElementById("demoNext").addEventListener("click", () => nextTurn(true));
  document.getElementById("demoReset").addEventListener("click", reset);

  // ---------------------------------------------------------------- screens tour
  const tabs = document.getElementById("tourTabs");
  function showShot(i) {
    tabs.dataset.active = String(i);
    tabs.querySelectorAll("button").forEach((b) => b.setAttribute("aria-selected", String(+b.dataset.shot === i)));
    document.querySelectorAll("[data-shot-img]").forEach((el) => { el.hidden = +el.dataset.shotImg !== i; });
    document.querySelectorAll("[data-shot-cap]").forEach((el) => { el.hidden = +el.dataset.shotCap !== i; });
  }
  tabs.querySelectorAll("button").forEach((b) => b.addEventListener("click", () => showShot(+b.dataset.shot)));
  tabs.addEventListener("keydown", (e) => {
    if (e.key !== "ArrowRight" && e.key !== "ArrowLeft") return;
    const i = (+tabs.dataset.active + (e.key === "ArrowRight" ? 1 : 2)) % 3;
    showShot(i);
    tabs.querySelectorAll("button")[i].focus();
  });
  showShot(0);

  // ---------------------------------------------------------------- start
  document.querySelectorAll("#lang button").forEach((b) => b.addEventListener("click", () => applyLang(b.dataset.lang)));
  let saved = null;
  try { saved = localStorage.getItem("hpTrackerLang"); } catch (e) { /* private mode */ }
  reset();
  const browser = (navigator.language || "").toLowerCase();
  const guess = browser.startsWith("ca") ? "ca" : browser.startsWith("es") ? "es" : "en";
  applyLang(["en", "es", "ca"].includes(saved) ? saved : guess);
})();
