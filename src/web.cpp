#include "web.h"

#include <WebServer.h>

#include "display.h"
#include "modes.h"
#include "modes/ambient_mode.h"
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
  .sub { color: var(--muted); margin: 0 0 24px; font-size: 14px; }
  section { background: var(--card); border: 1px solid var(--line); border-radius: 14px; padding: 16px; margin-bottom: 16px; }
  h2 { font-size: 13px; text-transform: uppercase; letter-spacing: .08em; color: var(--muted); margin: 0 0 12px; }
  .modes { display: grid; gap: 8px; }
  .modes button { text-align: left; padding: 14px 16px; border-radius: 10px; border: 1px solid var(--line);
                  background: transparent; color: var(--fg); font: inherit; cursor: pointer; }
  .modes button.on { background: var(--accent); color: var(--bg); border-color: var(--accent); }
  label { display: block; font-size: 14px; color: var(--muted); margin: 10px 0 4px; }
  input[type=text] { width: 100%; padding: 10px 12px; border-radius: 8px; border: 1px solid var(--line);
                     background: var(--bg); color: var(--fg); font: inherit; }
  input[type=range] { width: 100%; accent-color: var(--accent); }
  .save { margin-top: 12px; padding: 10px 18px; border-radius: 8px; border: 0; background: var(--accent);
          color: var(--bg); font: inherit; cursor: pointer; }
  .hint { font-size: 13px; color: var(--muted); margin: 8px 0 0; }
  .action { margin-top: 12px; width: 100%; padding: 12px 16px; border-radius: 10px; border: 1px dashed var(--muted);
            background: transparent; color: var(--fg); font: inherit; cursor: pointer; }
  .row { display: flex; gap: 8px; }
  .row > div { flex: 1; }
  select { width: 100%; padding: 10px 12px; border-radius: 8px; border: 1px solid var(--line);
           background: var(--bg); color: var(--fg); font: inherit; }
  .info { font-size: 14px; color: var(--muted); margin: 0 0 16px; }
  #status { min-height: 1.4em; font-size: 14px; color: var(--muted); text-align: center; }
</style>
</head>
<body>
<main>
  <h1>OBEGRÄNSAD</h1>
  <p class="sub">Pannello di controllo della lampada</p>
  <p class="info" id="info"></p>

  <section>
    <h2>Modalità</h2>
    <div class="modes" id="modes"></div>
    <button class="action" id="action" hidden></button>
  </section>

  <section>
    <h2>Testo scorrevole</h2>
    <label for="top">Riga sopra</label>
    <input type="text" id="top" maxlength="100" autocomplete="off">
    <label for="bottom">Riga sotto (vuota = una riga sola al centro)</label>
    <input type="text" id="bottom" maxlength="100" autocomplete="off">
    <button class="save" id="saveText">Mostra</button>
    <p class="hint">Minuscole, cifre e . , ! ? ' - (le maiuscole diventano minuscole).</p>
  </section>

  <section>
    <h2>Orologio e meteo</h2>
    <div class="row">
      <div><label for="lat">Latitudine</label><input type="text" id="lat" inputmode="decimal"></div>
      <div><label for="lon">Longitudine</label><input type="text" id="lon" inputmode="decimal"></div>
    </div>
    <button class="save" id="saveLocation">Salva posizione</button>
    <p class="hint">Su Google Maps: tieni premuto su un punto e copia i due numeri (es. 45.4642, 9.1900).</p>
  </section>

  <section>
    <h2>Animazioni</h2>
    <label for="ambient">Animazione</label>
    <select id="ambient"></select>
    <p class="hint">Automatica: cambia ogni 5 minuti di giorno, solo stelle dalle 22 alle 7.</p>
  </section>

  <section>
    <h2>Regolazioni</h2>
    <label for="brightness">Luminosità</label>
    <input type="range" id="brightness" min="1" max="255">
    <label for="speed">Velocità del testo</label>
    <input type="range" id="speed" min="20" max="300">
  </section>

  <p id="status"></p>
</main>
<script>
const $ = (id) => document.getElementById(id);
let state = null;

function status(msg) { $('status').textContent = msg; }

async function post(path, data) {
  const res = await fetch(path, { method: 'POST', body: new URLSearchParams(data) });
  if (!res.ok) throw new Error(await res.text());
  state = await res.json();
  render();
}

const WEATHER = [[0, 'sereno'], [2, 'poco nuvoloso'], [3, 'nuvoloso'], [48, 'nebbia'], [67, 'pioggia'],
                 [77, 'neve'], [82, 'rovesci'], [86, 'neve'], [99, 'temporale']];
function weatherName(code) {
  const hit = WEATHER.find(([max]) => code <= max);
  return hit ? hit[1] : '';
}

function editing(...ids) { return ids.includes(document.activeElement && document.activeElement.id); }

function render() {
  const info = [];
  info.push(state.time ? 'Ora ' + state.time : 'Ora non ancora sincronizzata');
  if (state.weather) info.push(Math.round(state.weather.temp) + '° ' + weatherName(state.weather.code));
  $('info').textContent = info.join(' · ');

  const action = $('action');
  action.hidden = !state.action;
  action.textContent = state.action || '';

  const box = $('modes');
  box.innerHTML = '';
  for (const m of state.modes) {
    const b = document.createElement('button');
    b.textContent = m.name;
    b.className = m.id === state.mode ? 'on' : '';
    b.onclick = () => post('/api/mode', { id: m.id }).then(() => status(m.name + ' attivato')).catch((e) => status(e.message));
    box.appendChild(b);
  }
  if (!editing('top', 'bottom')) {
    const [top, ...rest] = state.text.split('|');
    $('top').value = top;
    $('bottom').value = rest.join(' ');
  }
  if (!editing('lat', 'lon')) {
    $('lat').value = state.lat;
    $('lon').value = state.lon;
  }
  const sel = $('ambient');
  if (!sel.options.length) {
    for (const a of [{ id: 'auto', name: 'Automatica' }, ...state.ambients]) sel.add(new Option(a.name, a.id));
  }
  sel.value = state.ambient;
  $('brightness').value = state.brightness;
  // The slider goes slow -> fast, the setting is a delay (fast = small).
  $('speed').value = 320 - state.speed;
}

$('saveText').onclick = () => {
  const top = $('top').value.replace(/\|/g, ' ');
  const bottom = $('bottom').value.replace(/\|/g, ' ');
  post('/api/text', { text: bottom.trim() ? top + '|' + bottom : top })
    .then(() => status('Testo aggiornato')).catch((e) => status(e.message));
};
$('action').onclick = () => post('/api/action', {}).catch((e) => status(e.message));
$('saveLocation').onclick = () => {
  const lat = parseFloat($('lat').value.replace(',', '.'));
  const lon = parseFloat($('lon').value.replace(',', '.'));
  if (!(Math.abs(lat) <= 90 && Math.abs(lon) <= 180)) { status('Coordinate non valide'); return; }
  status('Aggiorno il meteo...');
  post('/api/settings', { lat, lon }).then(() => status('Posizione salvata')).catch((e) => status(e.message));
};
$('ambient').onchange = (e) => post('/api/settings', { ambient: e.target.value })
  .then(() => status('Animazione cambiata')).catch((e) => status(e.message));
$('brightness').onchange = (e) => post('/api/settings', { brightness: e.target.value }).catch((e) => status(e.message));
$('speed').onchange = (e) => post('/api/settings', { speed: 320 - e.target.value }).catch((e) => status(e.message));

function refresh() {
  return fetch('/api/state').then((r) => r.json()).then((s) => { state = s; render(); });
}
refresh().catch(() => status('Lampada non raggiungibile'));
setInterval(() => refresh().catch(() => {}), 30000);
</script>
</body>
</html>
)HTML";

static String jsonString(const String &s) {
  String out = "\"";
  for (unsigned i = 0; i < s.length(); i++) {
    const char c = s[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if ((uint8_t)c < 0x20) {
      out += ' ';
    } else {
      out += c;
    }
  }
  return out + "\"";
}

static void sendState() {
  String json = "{\"mode\":" + jsonString(settings.mode) + ",\"modes\":[";
  for (uint8_t i = 0; i < MODE_COUNT; i++) {
    if (i) json += ',';
    json += "{\"id\":" + jsonString(MODES[i]->id()) + ",\"name\":" + jsonString(MODES[i]->name()) + "}";
  }
  json += "],\"text\":" + jsonString(settings.text);
  json += ",\"brightness\":" + String(settings.brightness);
  json += ",\"speed\":" + String(settings.speedMs);

  const char *action = currentMode()->actionName();
  json += ",\"action\":" + (action ? jsonString(action) : String("null"));

  json += ",\"lat\":" + String(settings.latitude, 4) + ",\"lon\":" + String(settings.longitude, 4);
  json += ",\"ambient\":" + jsonString(settings.ambient) + ",\"ambients\":[";
  for (uint8_t i = 0; i < AmbientMode::ANIMATION_COUNT; i++) {
    if (i) json += ',';
    json += "{\"id\":" + jsonString(AmbientMode::ANIMATIONS[i].id) +
            ",\"name\":" + jsonString(AmbientMode::ANIMATIONS[i].name) + "}";
  }
  json += "]";

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

static void handleMode() {
  if (!setMode(server.arg("id"))) {
    server.send(400, "text/plain", "Modalità sconosciuta");
    return;
  }
  saveSettings();
  sendState();
}

static void handleText() {
  String text = server.arg("text");
  text.trim();
  if (text.length() > 201) text = text.substring(0, 201);
  settings.text = text;
  saveSettings();
  // Show the new text straight away, even if another mode was active.
  if (settings.mode != "text") {
    setMode("text");
    saveSettings();
  } else {
    restartMode();
  }
  sendState();
}

static void handleAction() {
  currentMode()->action();
  sendState();
}

static bool validAmbient(const String &id) {
  if (id == "auto") return true;
  for (uint8_t i = 0; i < AmbientMode::ANIMATION_COUNT; i++) {
    if (id == AmbientMode::ANIMATIONS[i].id) return true;
  }
  return false;
}

static void handleSettings() {
  if (server.hasArg("brightness")) {
    settings.brightness = constrain(server.arg("brightness").toInt(), 1, 255);
    display.setBrightness(settings.brightness);
  }
  if (server.hasArg("speed")) {
    settings.speedMs = constrain(server.arg("speed").toInt(), 20, 300);
  }
  if (server.hasArg("lat") && server.hasArg("lon")) {
    const float lat = server.arg("lat").toFloat();
    const float lon = server.arg("lon").toFloat();
    if (fabsf(lat) <= 90 && fabsf(lon) <= 180) {
      settings.latitude = lat;
      settings.longitude = lon;
      saveSettings();
      updateWeather(true);
    }
  }
  if (server.hasArg("ambient") && validAmbient(server.arg("ambient"))) {
    settings.ambient = server.arg("ambient");
    if (settings.mode == "ambient") {
      restartMode();
    } else {
      setMode("ambient");
    }
  }
  saveSettings();
  sendState();
}

void webBegin() {
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", PAGE); });
  server.on("/api/state", HTTP_GET, sendState);
  server.on("/api/mode", HTTP_POST, handleMode);
  server.on("/api/text", HTTP_POST, handleText);
  server.on("/api/settings", HTTP_POST, handleSettings);
  server.on("/api/action", HTTP_POST, handleAction);
  server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
  server.begin();
}

void webLoop() { server.handleClient(); }
