#include "web.h"

#include <WebServer.h>

#include "animation.h"
#include "display.h"
#include "modes.h"
#include "modes/ambient_mode.h"
#include "modes/quotes_mode.h"
#include "settings.h"
#include "timekeeping.h"
#include "weather.h"

static WebServer server(80);

static const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>OBEGRÄNSAD</title>
<style>
  :root { --bg: #f4f1ea; --card: #fff; --fg: #1d1d1b; --muted: #6b6a66; --accent: #1d1d1b; --line: #dedad0; }
  @media (prefers-color-scheme: dark) {
    :root { --bg: #151514; --card: #201f1d; --fg: #f1efe9; --muted: #a09d95; --accent: #f1efe9; --line: #353330; }
  }
  * { box-sizing: border-box; }
  body { margin: 0; font: 16px/1.4 system-ui, -apple-system, sans-serif; background: var(--bg); color: var(--fg); }
  main { max-width: 480px; margin: 0 auto; padding: 24px 16px 48px; }
  h1 { font-size: 22px; letter-spacing: .08em; margin: 0 0 4px; }
  .info { font-size: 14px; color: var(--muted); margin: 0 0 20px; }
  section, details { background: var(--card); border: 1px solid var(--line); border-radius: 14px; padding: 16px; margin-bottom: 12px; }
  h2, summary { font-size: 13px; text-transform: uppercase; letter-spacing: .08em; color: var(--muted); margin: 0; font-weight: 600; }
  h2 { margin-bottom: 12px; }
  summary { cursor: pointer; list-style: none; display: flex; justify-content: space-between; }
  summary::after { content: '+'; font-size: 16px; }
  details[open] summary::after { content: '−'; }
  details[open] summary { margin-bottom: 12px; }
  button { font: inherit; color: inherit; cursor: pointer; }
  .modes { display: grid; gap: 8px; }
  .modes button { text-align: left; padding: 14px 16px; border-radius: 10px; border: 1px solid var(--line); background: transparent; display: flex; justify-content: space-between; }
  .modes button.on { background: var(--accent); color: var(--bg); border-color: var(--accent); }
  .modes small { opacity: .7; }
  .action { margin-top: 12px; width: 100%; padding: 12px 16px; border-radius: 10px; border: 1px dashed var(--muted); background: transparent; }
  label { display: block; font-size: 14px; color: var(--muted); margin: 10px 0 4px; }
  label.check { display: flex; gap: 8px; align-items: center; color: var(--fg); margin: 0 0 8px; }
  input[type=text], input[type=number], input[type=time], select, textarea {
    width: 100%; padding: 10px 12px; border-radius: 8px; border: 1px solid var(--line); background: var(--bg); color: var(--fg); font: inherit; }
  textarea { min-height: 200px; resize: vertical; }
  input[type=range] { width: 100%; accent-color: var(--accent); }
  .save { margin-top: 12px; padding: 10px 18px; border-radius: 8px; border: 0; background: var(--accent); color: var(--bg); }
  .link { margin-top: 12px; padding: 10px 0; border: 0; background: transparent; color: var(--muted); text-decoration: underline; }
  .row { display: flex; gap: 8px; align-items: center; }
  .row > * { flex: 1; }
  .row > .narrow { flex: 0 0 90px; }
  .row > .x { flex: 0 0 40px; padding: 8px 0; border-radius: 8px; border: 1px solid var(--line); background: transparent; }
  .list { display: grid; gap: 8px; }
  .results { display: grid; gap: 6px; margin-top: 8px; }
  .results button { text-align: left; padding: 10px 12px; border-radius: 8px; border: 1px solid var(--line); background: transparent; }
  .segmented { display: grid; grid-template-columns: 1fr 1fr; border: 1px solid var(--line); border-radius: 10px; overflow: hidden; }
  .segmented button { padding: 10px; border: 0; background: transparent; }
  .segmented button + button { border-left: 1px solid var(--line); }
  .segmented button.on { background: var(--accent); color: var(--bg); }
  .hint { font-size: 13px; color: var(--muted); margin: 8px 0 0; }
  .pad { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; margin-top: 12px; touch-action: manipulation; user-select: none; -webkit-user-select: none; }
  .pad button { padding: 18px 0; font-size: 22px; border-radius: 12px; border: 1px solid var(--line); background: var(--bg); }
  .pad button:active { background: var(--accent); color: var(--bg); }
  .pad .wide { grid-column: 1 / -1; font-size: 18px; }
  [hidden] { display: none !important; }
  #status { min-height: 1.4em; font-size: 14px; color: var(--muted); text-align: center; }
</style>
</head>
<body>
<main>
  <h1>OBEGRÄNSAD</h1>
  <p class="info" id="info"></p>

  <section>
    <h2>Modalità</h2>
    <div class="modes" id="modes"></div>
    <button class="action" id="action" hidden></button>
    <div id="speedBox" hidden>
      <label for="speed">Velocità</label>
      <input type="range" id="speed" min="1" max="9">
    </div>
  </section>

  <section id="gameBox" hidden>
    <h2 id="gameTitle">Gioco</h2>
    <label class="check"><input type="checkbox" id="demo"> Modalità demo: gioca da solo</label>
    <p class="hint" id="demoHint"></p>
    <div class="pad" id="pad" hidden>
      <span></span><button data-key="U" id="keyU">↑</button><span></span>
      <button data-key="L">←</button><button data-key="D" id="keyD">↓</button><button data-key="R">→</button>
      <button data-key="A" class="wide" id="keyA">Salta</button>
    </div>
    <p class="hint" id="padHint"></p>
  </section>

  <!-- Settings of the mode being shown -->
  <section data-mode="text" hidden>
    <h2>Testo scorrevole</h2>
    <input type="text" id="text" maxlength="200" autocomplete="off">
    <button class="save" id="saveText">Mostra</button>
    <p class="hint">Maiuscole e minuscole, cifre, . , : ! ? ' - e à è é ì ò ù.</p>
  </section>

  <section data-mode="quotes" hidden>
    <h2>Frasi</h2>
    <textarea id="quotes" spellcheck="false"></textarea>
    <p class="hint">Una frase per riga: ogni ora ne scorre una diversa.</p>
    <div class="row">
      <button class="save" id="saveQuotes">Salva frasi</button>
      <button class="link" id="resetQuotes">Ripristina quelle predefinite</button>
    </div>
  </section>

  <section data-mode="clock" hidden>
    <h2>Orologio e meteo</h2>
    <p class="hint" id="clockInfo"></p>
    <button class="link" id="openPlace">Cambia città o fuso orario</button>
  </section>

  <section data-mode="ambient" hidden>
    <h2>Animazioni</h2>
    <select id="ambient"></select>
    <p class="hint" id="ambientInfo"></p>
  </section>

  <!-- General settings -->
  <details id="playlistBox">
    <summary>Playlist</summary>
    <label class="check"><input type="checkbox" id="playlistOn"> Alterna le modalità da sola</label>
    <div class="list" id="playlist"></div>
    <div class="row">
      <button class="link" id="addItem">+ Aggiungi</button>
      <button class="save" id="savePlaylist">Salva playlist</button>
    </div>
    <p class="hint">Scegliere una modalità a mano ferma la playlist.</p>
  </details>

  <details id="nightBox">
    <summary>Giorno e notte</summary>
    <label class="check"><input type="checkbox" id="nightOn"> Di notte cambia comportamento</label>
    <div class="row">
      <div><label for="nightStart">Dalle</label><input type="time" id="nightStart"></div>
      <div><label for="nightEnd">Alle</label><input type="time" id="nightEnd"></div>
    </div>
    <label for="nightMode">Di notte</label>
    <select id="nightMode">
      <option value="off">Lampada spenta</option>
      <option value="stars">Solo stelle</option>
      <option value="dim">Luminosità ridotta</option>
    </select>
    <div id="nightDimBox">
      <label for="nightBrightness">Luminosità di notte</label>
      <input type="range" id="nightBrightness" min="1" max="255">
    </div>
    <button class="save" id="saveNight">Salva</button>
  </details>

  <details id="placeBox">
    <summary>Luogo e ora</summary>
    <label for="city">Città (per il meteo)</label>
    <div class="row">
      <input type="text" id="city" autocomplete="off" placeholder="es. Milano">
      <button class="save narrow" id="searchCity" style="margin-top:0">Cerca</button>
    </div>
    <div class="results" id="cityResults"></div>
    <p class="hint" id="placeInfo"></p>
    <label for="tz">Fuso orario</label>
    <select id="tz"></select>
  </details>

  <details id="displayBox">
    <summary>Display</summary>
    <label>Orientamento della lampada</label>
    <div class="segmented" id="orientation">
      <button data-vertical="0">Orizzontale</button>
      <button data-vertical="1">Verticale</button>
    </div>
    <label for="brightness">Luminosità</label>
    <input type="range" id="brightness" min="1" max="255">
  </details>

  <p id="status"></p>
</main>
<script>
const $ = (id) => document.getElementById(id);
let state = null;
const dirty = {};  // forms edited but not saved yet: don't overwrite them

// IANA time zone -> POSIX TZ string (what the lamp's clock understands).
const ZONES = {
  'Europe/Rome': 'CET-1CEST,M3.5.0,M10.5.0/3', 'Europe/Paris': 'CET-1CEST,M3.5.0,M10.5.0/3',
  'Europe/Berlin': 'CET-1CEST,M3.5.0,M10.5.0/3', 'Europe/Madrid': 'CET-1CEST,M3.5.0,M10.5.0/3',
  'Europe/Zurich': 'CET-1CEST,M3.5.0,M10.5.0/3', 'Europe/Vienna': 'CET-1CEST,M3.5.0,M10.5.0/3',
  'Europe/Amsterdam': 'CET-1CEST,M3.5.0,M10.5.0/3', 'Europe/Brussels': 'CET-1CEST,M3.5.0,M10.5.0/3',
  'Europe/Stockholm': 'CET-1CEST,M3.5.0,M10.5.0/3', 'Europe/Oslo': 'CET-1CEST,M3.5.0,M10.5.0/3',
  'Europe/Copenhagen': 'CET-1CEST,M3.5.0,M10.5.0/3', 'Europe/Warsaw': 'CET-1CEST,M3.5.0,M10.5.0/3',
  'Europe/Prague': 'CET-1CEST,M3.5.0,M10.5.0/3', 'Europe/Budapest': 'CET-1CEST,M3.5.0,M10.5.0/3',
  'Europe/Belgrade': 'CET-1CEST,M3.5.0,M10.5.0/3', 'Europe/Malta': 'CET-1CEST,M3.5.0,M10.5.0/3',
  'Europe/San_Marino': 'CET-1CEST,M3.5.0,M10.5.0/3', 'Europe/Vatican': 'CET-1CEST,M3.5.0,M10.5.0/3',
  'Europe/London': 'GMT0BST,M3.5.0/1,M10.5.0', 'Europe/Dublin': 'IST-1GMT0,M10.5.0,M3.5.0/1',
  'Europe/Lisbon': 'WET0WEST,M3.5.0/1,M10.5.0', 'Atlantic/Canary': 'WET0WEST,M3.5.0/1,M10.5.0',
  'Europe/Athens': 'EET-2EEST,M3.5.0/3,M10.5.0/4', 'Europe/Helsinki': 'EET-2EEST,M3.5.0/3,M10.5.0/4',
  'Europe/Bucharest': 'EET-2EEST,M3.5.0/3,M10.5.0/4', 'Europe/Kyiv': 'EET-2EEST,M3.5.0/3,M10.5.0/4',
  'Europe/Istanbul': '<+03>-3', 'Europe/Moscow': 'MSK-3',
  'America/New_York': 'EST5EDT,M3.2.0,M11.1.0', 'America/Chicago': 'CST6CDT,M3.2.0,M11.1.0',
  'America/Denver': 'MST7MDT,M3.2.0,M11.1.0', 'America/Phoenix': 'MST7',
  'America/Los_Angeles': 'PST8PDT,M3.2.0,M11.1.0', 'America/Mexico_City': 'CST6',
  'America/Sao_Paulo': '<-03>3', 'America/Buenos_Aires': '<-03>3', 'America/Toronto': 'EST5EDT,M3.2.0,M11.1.0',
  'Africa/Cairo': 'EET-2EEST,M4.5.5/0,M10.5.4/24', 'Africa/Johannesburg': 'SAST-2', 'Africa/Casablanca': '<+01>-1',
  'Asia/Dubai': '<+04>-4', 'Asia/Kolkata': 'IST-5:30', 'Asia/Bangkok': '<+07>-7', 'Asia/Singapore': '<+08>-8',
  'Asia/Shanghai': 'CST-8', 'Asia/Hong_Kong': 'HKT-8', 'Asia/Tokyo': 'JST-9', 'Asia/Seoul': 'KST-9',
  'Australia/Perth': 'AWST-8', 'Australia/Sydney': 'AEST-10AEDT,M10.1.0,M4.1.0/3',
  'Pacific/Auckland': 'NZST-12NZDT,M9.5.0,M4.1.0/3', 'UTC': 'UTC0',
};

function status(msg) { $('status').textContent = msg; }
function fail(e) { status(e.message || 'Errore'); }

async function post(path, data) {
  const res = await fetch(path, { method: 'POST', body: new URLSearchParams(data) });
  if (!res.ok) throw new Error(await res.text());
  state = await res.json();
  render();
}

const WEATHER = [[0, 'sereno'], [2, 'poco nuvoloso'], [3, 'nuvoloso'], [48, 'nebbia'], [67, 'pioggia'],
                 [77, 'neve'], [82, 'rovesci'], [86, 'neve'], [99, 'temporale']];
function weatherName(code) { const hit = WEATHER.find(([max]) => code <= max); return hit ? hit[1] : ''; }
function editing(...ids) { return ids.includes(document.activeElement && document.activeElement.id); }
function hhmm(minutes) { return String(Math.floor(minutes / 60)).padStart(2, '0') + ':' + String(minutes % 60).padStart(2, '0'); }
function minutesOf(value) { const [h, m] = value.split(':').map(Number); return h * 60 + m; }
function modeName(id) { const m = state.modes.find((m) => m.id === id); return m ? m.name : id; }

function render() {
  const s = state;
  const info = [s.time ? 'Ora ' + s.time : 'Ora non ancora sincronizzata'];
  if (s.weather) info.push(Math.round(s.weather.temp) + '° ' + weatherName(s.weather.code) + ' a ' + s.city);
  if (s.night) info.push('notte');
  else if (s.playlistPos >= 0) info.push('playlist');
  $('info').textContent = info.join(' · ');

  // Modes: the one being shown is highlighted.
  const box = $('modes');
  box.innerHTML = '';
  for (const m of s.modes) {
    const b = document.createElement('button');
    b.className = m.id === s.active ? 'on' : '';
    b.textContent = m.name;
    if (m.id === s.active && (s.night || s.playlistPos >= 0)) {
      const tag = document.createElement('small');
      tag.textContent = s.night ? 'notte' : 'playlist';
      b.appendChild(tag);
    }
    b.onclick = () => post('/api/mode', { id: m.id }).then(() => status(m.name)).catch(fail);
    box.appendChild(b);
  }
  const active = s.modes.find((m) => m.id === s.active);
  $('action').hidden = !active.action;
  $('action').textContent = active.action || '';
  $('speedBox').hidden = !active.hasSpeed;
  if (!editing('speed')) $('speed').value = active.speed;

  renderGame();

  // Only the active mode's own settings.
  for (const sec of document.querySelectorAll('section[data-mode]')) sec.hidden = sec.dataset.mode !== s.active;

  if (!editing('text')) $('text').value = s.text;
  if (!dirty.quotes) $('quotes').value = s.quotes || s.defaultQuotes;
  $('clockInfo').textContent = 'Meteo per ' + s.city + ' (' + s.lat.toFixed(2) + ', ' + s.lon.toFixed(2) + '), fuso ' + s.tzName + '.';

  const sel = $('ambient');
  if (!sel.options.length) {
    sel.add(new Option('Automatica (cambia ogni 5 minuti)', 'auto'));
    let group = null;
    for (const a of s.animations) {
      if (!group || group.label !== a.group) { group = document.createElement('optgroup'); group.label = a.group; sel.appendChild(group); }
      group.appendChild(new Option(a.name, a.id));
    }
  }
  sel.value = s.ambient;
  const playing = s.animations.find((a) => a.id === s.animation);
  $('ambientInfo').textContent = playing ? 'In riproduzione: ' + playing.name + (s.night ? ' (modalità notte)' : '') : '';

  if (!dirty.playlist) {
    $('playlistOn').checked = s.playlistOn;
    renderPlaylist(s.playlist.split(',').filter(Boolean).map((i) => i.split(':')));
  }
  if (!dirty.night) {
    $('nightOn').checked = s.nightOn;
    $('nightStart').value = hhmm(s.nightStart);
    $('nightEnd').value = hhmm(s.nightEnd);
    $('nightMode').value = s.nightMode;
    $('nightBrightness').value = s.nightBrightness;
  }
  $('nightDimBox').hidden = $('nightMode').value !== 'dim';

  $('placeInfo').textContent = 'Attuale: ' + s.city + ' (' + s.lat.toFixed(4) + ', ' + s.lon.toFixed(4) + ')';
  const tz = $('tz');
  if (!tz.options.length) {
    for (const name of Object.keys(ZONES).sort()) tz.add(new Option(name.replace(/_/g, ' '), name));
  }
  if (!ZONES[s.tzName] && ![...tz.options].some((o) => o.value === s.tzName)) tz.add(new Option(s.tzName, s.tzName));
  tz.value = s.tzName;

  for (const b of $('orientation').children) b.classList.toggle('on', b.dataset.vertical === (s.vertical ? '1' : '0'));
  if (!editing('brightness')) $('brightness').value = s.brightness;
}

// Game box: demo checkbox and, when the player is in control, the pad.
const PADS = {
  mario: { keys: ['A'], labels: { A: 'Salta' }, hint: 'Tastiera: barra spaziatrice o freccia su per saltare.' },
  tetris: { keys: ['L', 'R', 'U', 'D'], labels: { U: '↻', D: '⤓' }, hint: 'Tastiera: ← → per spostare, ↑ per ruotare, ↓ o spazio per far cadere.' },
  snake: { keys: ['L', 'R', 'U', 'D'], labels: { U: '↑', D: '↓' }, hint: 'Tastiera: le frecce.' },
};
function playable() { return state && state.game && !state.game.demo; }
function renderGame() {
  const g = state.game;
  $('gameBox').hidden = !g;
  if (!g) return;
  $('gameTitle').textContent = g.name;
  $('demo').checked = g.demo;
  $('demo').disabled = g.forced;
  $('demoHint').textContent = g.forced
    ? 'In «Automatica» e di notte i giochi vanno sempre in demo: sceglilo nel menu delle animazioni per giocare.'
    : g.demo ? 'Togli la spunta per giocare tu.' : '';
  const pad = PADS[g.id];
  $('pad').hidden = g.demo || !pad;
  $('padHint').textContent = g.demo || !pad ? '' : pad.hint;
  if (!pad) return;
  for (const b of $('pad').querySelectorAll('button')) {
    b.hidden = !pad.keys.includes(b.dataset.key);
    const label = pad.labels[b.dataset.key];
    if (label) b.textContent = label;
    else b.textContent = { L: '←', R: '→', U: '↑', D: '↓', A: 'Salta' }[b.dataset.key];
  }
}
// Controls go out on touch/press, not on release, and don't wait for an
// answer: every millisecond counts over WiFi.
function sendKey(key) { fetch('/api/input', { method: 'POST', body: new URLSearchParams({ key }), keepalive: true }).catch(() => {}); }
for (const b of $('pad').querySelectorAll('button')) {
  b.addEventListener('pointerdown', (e) => { e.preventDefault(); sendKey(b.dataset.key); });
}
$('demo').onchange = (e) => (e.target.blur(), post('/api/demo', { id: state.game.id, on: e.target.checked ? 1 : 0 }))
  .then(() => {
    status(e.target.checked ? 'Modalità demo' : 'Tocca a te!');
    if (!e.target.checked) $('gameBox').scrollIntoView({ behavior: 'smooth', block: 'start' });  // pad in view
  }).catch(fail);

// Playlist editor rows: [mode] [minutes] [x]
function renderPlaylist(items) {
  const list = $('playlist');
  list.innerHTML = '';
  for (const [id, minutes] of items) addPlaylistRow(id, minutes);
}
function addPlaylistRow(id, minutes) {
  const row = document.createElement('div');
  row.className = 'row';
  const sel = document.createElement('select');
  for (const m of state.modes) sel.add(new Option(m.name, m.id));
  sel.value = id;
  const min = document.createElement('input');
  min.type = 'number'; min.min = 1; min.max = 240; min.value = minutes; min.className = 'narrow';
  min.title = 'minuti';
  const x = document.createElement('button');
  x.className = 'x'; x.textContent = '✕';
  x.onclick = () => { row.remove(); dirty.playlist = true; };
  sel.onchange = min.oninput = () => { dirty.playlist = true; };
  row.append(sel, min, x);
  $('playlist').appendChild(row);
}
function playlistValue() {
  return [...$('playlist').children].map((row) => {
    const [sel, min] = row.querySelectorAll('select, input');
    return sel.value + ':' + Math.max(1, Math.min(240, parseInt(min.value) || 1));
  }).join(',');
}

$('action').onclick = () => post('/api/action', {}).catch(fail);
const ARROWS = { ArrowLeft: 'L', ArrowRight: 'R', ArrowUp: 'U', ArrowDown: 'D' };
// True while the focus is in a field you type into (not a checkbox or slider).
function typing() {
  const el = document.activeElement;
  if (!el) return false;
  if (el.tagName === 'TEXTAREA' || el.tagName === 'SELECT') return true;
  return el.tagName === 'INPUT' && !['checkbox', 'range', 'button'].includes(el.type);
}
document.addEventListener('keydown', (e) => {
  if (!state || typing()) return;
  if (playable() && (ARROWS[e.code] || e.code === 'Space')) {
    // Arrows and space drive the game (space: jump / drop).
    e.preventDefault();
    if (!e.repeat || e.code !== 'Space') sendKey(ARROWS[e.code] || 'A');
  } else if (e.code === 'Space' && !e.repeat && !$('action').hidden && document.activeElement.tagName !== 'BUTTON') {
    e.preventDefault();  // otherwise space = the mode's button (e.g. next quote)
    $('action').click();
  }
});
$('speed').onchange = (e) => post('/api/speed', { id: state.active, level: e.target.value }).catch(fail);

$('saveText').onclick = () => post('/api/text', { text: $('text').value }).then(() => status('Testo aggiornato')).catch(fail);
$('quotes').oninput = () => { dirty.quotes = true; };
$('saveQuotes').onclick = () => post('/api/quotes', { quotes: $('quotes').value })
  .then(() => { dirty.quotes = false; render(); status('Frasi salvate'); }).catch(fail);
$('resetQuotes').onclick = () => post('/api/quotes', { quotes: '' })
  .then(() => { dirty.quotes = false; render(); status('Frasi predefinite ripristinate'); }).catch(fail);
$('openPlace').onclick = () => { $('placeBox').open = true; $('placeBox').scrollIntoView({ behavior: 'smooth' }); };
$('ambient').onchange = (e) => post('/api/settings', { ambient: e.target.value }).then(() => status('Animazione cambiata')).catch(fail);

$('playlistOn').onchange = () => { dirty.playlist = true; };
$('addItem').onclick = () => { addPlaylistRow(state.modes[0].id, 5); dirty.playlist = true; };
$('savePlaylist').onclick = () => post('/api/playlist', { on: $('playlistOn').checked ? 1 : 0, items: playlistValue() })
  .then(() => { dirty.playlist = false; render(); status('Playlist salvata'); }).catch(fail);

for (const id of ['nightOn', 'nightStart', 'nightEnd', 'nightMode', 'nightBrightness']) {
  $(id).addEventListener('input', () => { dirty.night = true; $('nightDimBox').hidden = $('nightMode').value !== 'dim'; });
}
$('saveNight').onclick = () => post('/api/night', {
  on: $('nightOn').checked ? 1 : 0, start: minutesOf($('nightStart').value), end: minutesOf($('nightEnd').value),
  mode: $('nightMode').value, brightness: $('nightBrightness').value,
}).then(() => { dirty.night = false; render(); status('Impostazioni della notte salvate'); }).catch(fail);

// City search runs in the browser (Open-Meteo geocoding); the lamp only
// gets the coordinates and time zone of the chosen place.
async function searchCity() {
  const q = $('city').value.trim();
  if (!q) return;
  const box = $('cityResults');
  box.innerHTML = '';
  status('Cerco ' + q + '...');
  try {
    const url = 'https://geocoding-api.open-meteo.com/v1/search?count=6&language=it&format=json&name=' + encodeURIComponent(q);
    const data = await (await fetch(url)).json();
    const results = data.results || [];
    status(results.length ? '' : 'Nessuna città trovata');
    for (const r of results) {
      const b = document.createElement('button');
      b.textContent = [r.name, r.admin1, r.country].filter(Boolean).join(', ');
      b.onclick = () => pickCity(r);
      box.appendChild(b);
    }
  } catch (e) {
    status('Ricerca non riuscita: serve una connessione a Internet');
  }
}
async function pickCity(r) {
  $('cityResults').innerHTML = '';
  $('city').value = '';
  status('Aggiorno il meteo...');
  try {
    await post('/api/location', { lat: r.latitude, lon: r.longitude, city: r.name });
    if (r.timezone && ZONES[r.timezone] && r.timezone !== state.tzName) {
      await post('/api/timezone', { tz: ZONES[r.timezone], tzName: r.timezone });
    }
    status('Città: ' + r.name + (r.timezone && !ZONES[r.timezone] ? ' (scegli il fuso orario a mano)' : ''));
  } catch (e) { fail(e); }
}
$('searchCity').onclick = searchCity;
$('city').onkeydown = (e) => { if (e.key === 'Enter') searchCity(); };
$('tz').onchange = (e) => {
  const tz = ZONES[e.target.value];
  if (tz) post('/api/timezone', { tz, tzName: e.target.value }).then(() => status('Fuso orario aggiornato')).catch(fail);
};

for (const b of $('orientation').children) {
  b.onclick = () => post('/api/settings', { vertical: b.dataset.vertical })
    .then(() => status('Orientamento: ' + b.textContent.toLowerCase())).catch(fail);
}
$('brightness').onchange = (e) => post('/api/settings', { brightness: e.target.value }).catch(fail);

function refresh() { return fetch('/api/state').then((r) => r.json()).then((s) => { state = s; render(); }); }
refresh().catch(() => status('Lampada non raggiungibile'));
setInterval(() => refresh().catch(() => {}), 15000);
</script>
</body>
</html>
)HTML";

// ---------------------------------------------------------------------------
// JSON state

static String jsonString(const String &s) {
  String out = "\"";
  for (unsigned i = 0; i < s.length(); i++) {
    const char c = s[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else if ((uint8_t)c < 0x20) {
      out += ' ';
    } else {
      out += c;  // UTF-8 bytes pass through unchanged
    }
  }
  return out + "\"";
}

static String jsonBool(bool b) { return b ? "true" : "false"; }

static void sendState() {
  String json;
  json.reserve(4096);
  json = "{\"mode\":" + jsonString(settings.mode) + ",\"active\":" + jsonString(currentMode()->id());
  json += ",\"night\":" + jsonBool(isNight()) + ",\"playlistPos\":" + String(playlistPosition());

  json += ",\"modes\":[";
  for (uint8_t i = 0; i < MODE_COUNT; i++) {
    const Mode *m = MODES[i];
    if (i) json += ',';
    json += "{\"id\":" + jsonString(m->id()) + ",\"name\":" + jsonString(m->name());
    json += ",\"action\":" + (m->actionName() ? jsonString(m->actionName()) : String("null"));
    json += ",\"hasSpeed\":" + jsonBool(m->hasSpeed()) + ",\"speed\":" + String(speedLevel(m->id())) + "}";
  }
  json += "]";

  json += ",\"text\":" + jsonString(settings.text);
  json += ",\"quotes\":" + jsonString(settings.quotes);
  json += ",\"defaultQuotes\":" + jsonString(QuotesMode::defaultQuotes());
  json += ",\"brightness\":" + String(settings.brightness) + ",\"vertical\":" + jsonBool(settings.vertical);
  json += ",\"lat\":" + String(settings.latitude, 4) + ",\"lon\":" + String(settings.longitude, 4);
  json += ",\"city\":" + jsonString(settings.city) + ",\"tzName\":" + jsonString(settings.timezoneName);

  json += ",\"ambient\":" + jsonString(settings.ambient) + ",\"animations\":[";
  for (uint8_t i = 0; i < ANIMATION_COUNT; i++) {
    if (i) json += ',';
    json += "{\"id\":" + jsonString(ANIMATIONS[i]->id()) + ",\"name\":" + jsonString(ANIMATIONS[i]->name()) +
            ",\"group\":" + jsonString(ANIMATIONS[i]->group()) + "}";
  }
  json += "]";
  const AmbientMode *ambient = nullptr;
  for (uint8_t i = 0; i < MODE_COUNT; i++) {
    if (strcmp(MODES[i]->id(), "ambient") == 0) ambient = static_cast<const AmbientMode *>(MODES[i]);
  }
  const Animation *playing = ambient && currentMode() == ambient ? ambient->playing() : nullptr;
  json += ",\"animation\":" + (playing ? jsonString(playing->id()) : String("null"));

  // The game on the panel (Super Mario, or a game animation) and its demo mode.
  const char *game = currentMode()->gameId();
  if (game) {
    const bool forced = currentMode() == ambient && ambient->demoForced();
    const char *gameName = currentMode() == ambient && playing ? playing->name() : currentMode()->name();
    json += ",\"game\":{\"id\":" + jsonString(game) + ",\"name\":" + jsonString(gameName) +
            ",\"demo\":" + jsonBool(forced || demoMode(game)) + ",\"forced\":" + jsonBool(forced) + "}";
  } else {
    json += ",\"game\":null";
  }

  json += ",\"playlistOn\":" + jsonBool(settings.playlistOn) + ",\"playlist\":" + jsonString(settings.playlist);
  json += ",\"nightOn\":" + jsonBool(settings.nightOn) + ",\"nightStart\":" + String(settings.nightStart);
  json += ",\"nightEnd\":" + String(settings.nightEnd) + ",\"nightMode\":" + jsonString(settings.nightMode);
  json += ",\"nightBrightness\":" + String(settings.nightBrightness);

  struct tm t;
  if (localTime(t)) {
    char hhmm[6];
    strftime(hhmm, sizeof(hhmm), "%H:%M", &t);
    json += ",\"time\":" + jsonString(hhmm);
  } else {
    json += ",\"time\":null";
  }
  if (weather.valid) {
    json += ",\"weather\":{\"temp\":" + String(weather.temperature, 1) + ",\"code\":" + String(weather.code) + "}";
  } else {
    json += ",\"weather\":null";
  }
  json += "}";
  server.send(200, "application/json", json);
}

// ---------------------------------------------------------------------------
// Handlers: each changes settings, saves them and answers with the state.

static void badRequest(const char *message) { server.send(400, "text/plain", message); }

static void handleMode() {
  if (!setMode(server.arg("id"))) return badRequest("Modalità sconosciuta");
  saveSettings();
  sendState();
}

static void handleAction() {
  currentMode()->action();
  sendState();
}

static void handleInput() {
  const String key = server.arg("key");
  if (key.length() != 1 || !strchr("LRUDA", key[0])) return badRequest("Tasto sconosciuto");
  currentMode()->input(key[0]);
  server.send(204);
}

static void handleDemo() {
  const String id = server.arg("id");
  if (id != "mario" && id != "tetris" && id != "snake") return badRequest("Gioco sconosciuto");
  setDemoMode(id.c_str(), server.arg("on") == "1");
  saveSettings();
  sendState();
}

static void handleSpeed() {
  const String id = server.arg("id");
  if (!validModeId(id)) return badRequest("Modalità sconosciuta");
  setSpeedLevel(id.c_str(), server.arg("level").toInt());
  saveSettings();
  sendState();
}

static void handleText() {
  // One line only: '|' would split the text in two.
  String text = server.arg("text");
  text.replace('|', ' ');
  text.trim();
  if (text.length() > 200) text = text.substring(0, 200);
  settings.text = text;
  setMode("text");  // show the new text straight away
  saveSettings();
  sendState();
}

static void handleQuotes() {
  String quotes = server.arg("quotes");
  quotes.replace("\r", "");
  quotes.trim();
  if (quotes.length() > 3000) return badRequest("Troppo testo: al massimo 3000 caratteri");
  settings.quotes = quotes;
  saveSettings();
  if (strcmp(currentMode()->id(), "quotes") == 0) restartMode();
  sendState();
}

static void handleSettings() {
  if (server.hasArg("brightness")) settings.brightness = constrain(server.arg("brightness").toInt(), 1, 255);
  if (server.hasArg("vertical")) {
    settings.vertical = server.arg("vertical") == "1";
    display.setRotation(rotationForSettings());
    restartMode();  // redraw straight away in the new orientation
  }
  if (server.hasArg("ambient")) {
    const String id = server.arg("ambient");
    if (id != "auto" && !findAnimation(id)) return badRequest("Animazione sconosciuta");
    settings.ambient = id;
    setMode("ambient");
  }
  saveSettings();
  refreshModes();
  sendState();
}

static void handleLocation() {
  const float lat = server.arg("lat").toFloat(), lon = server.arg("lon").toFloat();
  if (!server.hasArg("lat") || !server.hasArg("lon") || fabsf(lat) > 90 || fabsf(lon) > 180) {
    return badRequest("Coordinate non valide");
  }
  settings.latitude = lat;
  settings.longitude = lon;
  String city = server.arg("city");
  city.trim();
  settings.city = city.length() ? city.substring(0, 60) : String("?");
  saveSettings();
  updateWeather(true);
  sendState();
}

static void handleTimezone() {
  const String tz = server.arg("tz"), name = server.arg("tzName");
  if (tz.length() == 0 || tz.length() > 60 || name.length() > 60) return badRequest("Fuso orario non valido");
  settings.timezone = tz;
  settings.timezoneName = name;
  applyTimezone();
  saveSettings();
  refreshModes();
  sendState();
}

static void handlePlaylist() {
  // Keep only well-formed "mode:minutes" items.
  String clean;
  const String items = server.arg("items");
  int start = 0, count = 0;
  while (start < (int)items.length() && count < 12) {
    int end = items.indexOf(',', start);
    if (end < 0) end = items.length();
    const String item = items.substring(start, end);
    const int colon = item.indexOf(':');
    const long minutes = colon > 0 ? item.substring(colon + 1).toInt() : 0;
    if (colon > 0 && validModeId(item.substring(0, colon)) && minutes >= 1 && minutes <= 240) {
      if (clean.length()) clean += ',';
      clean += item.substring(0, colon) + ':' + String(minutes);
      count++;
    }
    start = end + 1;
  }
  settings.playlist = clean;
  settings.playlistOn = server.arg("on") == "1" && count > 0;
  saveSettings();
  restartPlaylist();
  sendState();
}

static void handleNight() {
  const String mode = server.arg("mode");
  if (mode != "off" && mode != "stars" && mode != "dim") return badRequest("Modalità notte sconosciuta");
  settings.nightOn = server.arg("on") == "1";
  settings.nightStart = constrain(server.arg("start").toInt(), 0, 1439);
  settings.nightEnd = constrain(server.arg("end").toInt(), 0, 1439);
  settings.nightMode = mode;
  settings.nightBrightness = constrain(server.arg("brightness").toInt(), 1, 255);
  saveSettings();
  refreshModes();
  sendState();
}

void webBegin() {
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", PAGE); });
  server.on("/api/state", HTTP_GET, sendState);
  server.on("/api/mode", HTTP_POST, handleMode);
  server.on("/api/action", HTTP_POST, handleAction);
  server.on("/api/speed", HTTP_POST, handleSpeed);
  server.on("/api/input", HTTP_POST, handleInput);
  server.on("/api/demo", HTTP_POST, handleDemo);
  server.on("/api/text", HTTP_POST, handleText);
  server.on("/api/quotes", HTTP_POST, handleQuotes);
  server.on("/api/settings", HTTP_POST, handleSettings);
  server.on("/api/location", HTTP_POST, handleLocation);
  server.on("/api/timezone", HTTP_POST, handleTimezone);
  server.on("/api/playlist", HTTP_POST, handlePlaylist);
  server.on("/api/night", HTTP_POST, handleNight);
  server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
  server.begin();
}

void webLoop() { server.handleClient(); }
