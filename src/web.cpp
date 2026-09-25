#include "web.h"

#include <Update.h>
#include <WebServer.h>
#include <mbedtls/base64.h>

#include "animation.h"
#include "build_info.h"
#include "display.h"
#include "modes.h"
#include "modes/ambient_mode.h"
#include "gallery.h"
#include "modes/countdown_mode.h"
#include "modes/forecast_mode.h"
#include "modes/gallery_mode.h"
#include "modes/quotes_mode.h"
#include "modes/sunrise_mode.h"
#include "moon.h"
#include "webinfo.h"
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
  .fc { display: grid; grid-template-columns: repeat(12, 1fr); gap: 2px; align-items: end; height: 110px; margin-bottom: 8px; }
  .fc div { display: flex; flex-direction: column; align-items: center; justify-content: flex-end; height: 100%; font-size: 10px; color: var(--muted); }
  .fc .bar { width: 100%; background: var(--line); border-radius: 3px 3px 0 0; }
  .fc .t { color: var(--fg); font-size: 11px; }
  .fcdays { display: grid; grid-template-columns: repeat(4, 1fr); gap: 6px; }
  .fcdays div { border: 1px solid var(--line); border-radius: 10px; padding: 8px 4px; text-align: center; font-size: 13px; }
  .fcdays .ic { font-size: 22px; display: block; margin: 2px 0; }
  .fcdays small { color: var(--muted); display: block; }
  .days { display: flex; gap: 6px; flex-wrap: wrap; }
  .days label { display: flex; align-items: center; gap: 4px; margin: 0; color: var(--fg); }
  #canvas { width: 100%; max-width: 320px; aspect-ratio: 1; display: block; margin: 0 auto; border-radius: 8px; touch-action: none; background: #000; }
  .swatches { display: flex; gap: 6px; margin: 10px 0; }
  .swatches button { flex: 1; height: 36px; border-radius: 8px; border: 2px solid var(--line); }
  .swatches button.on { border-color: var(--accent); outline: 2px solid var(--accent); }
  .tools { display: flex; gap: 6px; flex-wrap: wrap; margin: 6px 0; }
  .tools button { padding: 8px 10px; border-radius: 8px; border: 1px solid var(--line); background: transparent; font-size: 14px; }
  .gallery { display: grid; gap: 8px; margin-top: 8px; }
  .item { display: flex; flex-wrap: wrap; gap: 6px 8px; align-items: center; padding: 8px; border: 1px solid var(--line); border-radius: 10px; }
  .item.on { border-color: var(--accent); }
  .item canvas { width: 40px; height: 40px; border-radius: 4px; background: #000; image-rendering: pixelated; }
  .item .name { flex: 1 1 120px; min-width: 0; font-size: 14px; }
  .item button { padding: 6px 8px; border-radius: 8px; border: 1px solid var(--line); background: transparent; font-size: 13px; }
  #preview { width: 100%; max-width: 220px; aspect-ratio: 1; display: block; margin: 0 auto; border-radius: 8px; background: #000; }
  #gameBox #preview { margin-top: 12px; }
  [hidden] { display: none !important; }
  #status { min-height: 1.4em; font-size: 14px; color: var(--muted); text-align: center; }
</style>
</head>
<body>
<main>
  <h1>OBEGRÄNSAD</h1>
  <p class="info" id="info"></p>

  <section id="previewBox">
    <h2>Sulla lampada ora</h2>
    <canvas id="preview" width="320" height="320"></canvas>
  </section>

  <section>
    <h2>Modalità</h2>
    <div class="modes" id="modes"></div>
    <p class="hint" id="override" hidden></p>
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
    <p class="hint">Maiuscole e minuscole, cifre, . , : ; ! ? ' - %. Le lettere accentate compaiono con l'apostrofo: «perché» diventa «perche'».</p>
    <label for="textPos">Altezza</label>
    <select id="textPos">
      <option value="random">Variabile (cambia a ogni passaggio)</option>
      <option value="top">In alto</option>
      <option value="middle">Al centro</option>
      <option value="bottom">In basso</option>
    </select>
    <label for="textFontText">Font</label>
    <select id="textFontText">
      <option value="small">Attuale (8 pixel)</option>
      <option value="big">Grande (tutto il pannello)</option>
      <option value="mini">Mini 3×5 (solo maiuscole)</option>
    </select>
    <p class="hint">È lo stesso font di Display: vale per tutto il testo che scorre. Con «Attuale» le lettere sono strette di un pixel. Con il Grande l'altezza non conta.</p>
  </section>

  <section data-mode="quotes" hidden>
    <h2>Frasi</h2>
    <textarea id="quotes" spellcheck="false"></textarea>
    <p class="hint">Una frase per riga: ogni ora ne compare una diversa, a pagine di 3 righe ferme (font 4 pixel, solo maiuscole). La velocità regola quanto resta ogni pagina. <span id="quotesInfo"></span></p>
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

  <section data-mode="forecast" hidden>
    <h2>Previsioni</h2>
    <div class="fcdays" id="fcDays"></div>
    <label>Prossime 12 ore</label>
    <div class="fc" id="fcChart"></div>
    <p class="hint" id="fcInfo"></p>
  </section>

  <section data-mode="web" hidden>
    <h2>Dal web</h2>
    <label class="check"><input type="checkbox" id="infoWord"> Parola del giorno</label>
    <label class="check"><input type="checkbox" id="infoHistory"> Accadde oggi (Wikipedia)</label>
    <p class="hint" id="historyStatus"></p>
    <label class="check"><input type="checkbox" id="infoCalendar"> Prossimo evento del calendario</label>
    <input type="text" id="icalUrl" placeholder="Link iCal del calendario (.ics)" autocomplete="off">
    <p class="hint">Google Calendar: Impostazioni → il tuo calendario → «Indirizzo segreto in formato iCal». Gli eventi ricorrenti non sono supportati. <span id="calendarStatus"></span></p>
    <label for="webPos">Altezza</label>
    <select id="webPos">
      <option value="random">Variabile (cambia a ogni passaggio)</option>
      <option value="top">In alto</option>
      <option value="middle">Al centro</option>
      <option value="bottom">In basso</option>
    </select>
    <button class="save" id="saveWeb">Salva</button>
    <p class="hint" id="webPreview"></p>
  </section>

  <section data-mode="gallery" hidden>
    <h2>Disegni</h2>
    <canvas id="canvas" width="320" height="320"></canvas>
    <div class="swatches" id="swatches"></div>
    <div class="tools">
      <button id="toolFill">Riempi</button><button id="toolClear">Pulisci</button><button id="toolInvert">Inverti</button>
    </div>
    <div class="row">
      <button class="x" id="framePrev">◀</button>
      <span id="frameInfo" style="text-align:center"></span>
      <button class="x" id="frameNext">▶</button>
    </div>
    <div class="tools">
      <button id="frameAdd">+ Fotogramma</button><button id="frameDup">Duplica</button><button id="frameDel">Elimina fotogramma</button><button id="framePlay">▶ Anteprima</button>
    </div>
    <label for="fps">Velocità dell'animazione</label>
    <select id="fps">
      <option value="500">2 fotogrammi al secondo</option><option value="250">4 al secondo</option>
      <option value="166">6 al secondo</option><option value="125">8 al secondo</option>
      <option value="100">10 al secondo</option><option value="66">15 al secondo</option>
    </select>
    <label for="drawName">Nome</label>
    <div class="row">
      <input type="text" id="drawName" maxlength="40" placeholder="Il mio disegno">
      <button class="save narrow" id="saveDrawing" style="margin-top:0">Salva</button>
    </div>
    <div class="tools"><button id="newDrawing">Nuovo disegno</button></div>
    <label for="importFile">Importa un'immagine o una GIF</label>
    <input type="file" id="importFile" accept="image/*">
    <label class="check" style="margin-top:6px"><input type="checkbox" id="importInvert"> Inverti chiari e scuri</label>
    <label class="check"><input type="checkbox" id="importContrast" checked> Contrasto automatico</label>
    <h2 style="margin-top:16px">Galleria</h2>
    <div class="tools"><button id="showAll">Mostra tutti a rotazione</button></div>
    <div class="gallery" id="gallery"></div>
  </section>

  <section data-mode="demo" hidden>
    <h2>Demo</h2>
    <label for="demoStyle">Come mostrare le frasi</label>
    <select id="demoStyle">
      <option value="auto">A turno: uno stile per frase</option>
      <option value="rows3">3 righe che scorrono insieme (font 4 pixel)</option>
      <option value="pages">A pagine: 3 righe ferme alla volta (font 4 pixel)</option>
      <option value="rows2">2 righe che scorrono insieme (Mini 5 pixel)</option>
    </select>
    <p class="hint">Le frasi dell'ora, per confrontare come si legge un testo lungo su 16×16 LED. La velocità regola lo scorrimento e il tempo di ogni pagina.</p>
  </section>

  <section data-mode="countdown" hidden>
    <h2>Conto alla rovescia</h2>
    <label for="cdLabel">Evento</label>
    <input type="text" id="cdLabel" maxlength="40">
    <div class="row">
      <div><label for="cdDate">Data</label><input type="date" id="cdDate"></div>
      <div><label for="cdTime">Ora</label><input type="time" id="cdTime"></div>
    </div>
    <button class="save" id="saveCd">Salva</button>
    <p class="hint" id="cdInfo"></p>
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

  <details id="alarmBox">
    <summary>Sveglia con l'alba</summary>
    <label class="check"><input type="checkbox" id="alarmOn"> Attiva</label>
    <label for="alarmTime">Ora della sveglia</label>
    <input type="time" id="alarmTime">
    <label>Giorni</label>
    <div class="days" id="alarmDays"></div>
    <div class="row">
      <div><label for="alarmRamp">Alba (minuti prima)</label><input type="number" id="alarmRamp" min="5" max="60"></div>
      <div><label for="alarmHold">Accesa dopo (minuti)</label><input type="number" id="alarmHold" min="1" max="120"></div>
    </div>
    <div class="row">
      <button class="save" id="saveAlarm">Salva</button>
      <button class="link" id="testAlarm">Prova l'alba (1 minuto)</button>
    </div>
    <p class="hint">Il sole sorge sul pannello e la luminosità sale piano fino all'ora della sveglia. Ha la precedenza su tutto, anche sulla notte.</p>
  </details>

  <details id="nightBox">
    <summary>Giorno e notte</summary>
    <label class="check"><input type="checkbox" id="nightOn"> Di notte cambia comportamento</label>
    <label class="check"><input type="checkbox" id="nightSun"> Dal tramonto all'alba <small id="sunTimes"></small></label>
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
    <p class="hint" id="skyInfo"></p>
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
    <label for="textFont">Font del testo che scorre</label>
    <select id="textFont">
      <option value="small">Attuale (8 pixel)</option>
      <option value="big">Grande (tutto il pannello)</option>
      <option value="mini">Mini 3×5 (solo maiuscole)</option>
    </select>
    <p class="hint">Vale per tutto il testo che scorre: testo, dati dal web, orologio a parole e punteggi dei giochi (le frasi dell'ora hanno le loro pagine a 3 righe). Con «Attuale» le lettere sono strette di un pixel. Con il Grande l'altezza del testo non conta: occupa tutto il pannello.</p>
    <label for="transition">Passaggio tra modalità e animazioni</label>
    <select id="transition">
      <option value="fade">Dissolvenza</option>
      <option value="wipe">Tendina da sinistra</option>
      <option value="none">Stacco netto</option>
    </select>
  </details>

  <details id="updateBox">
    <summary>Aggiornamento firmware</summary>
    <p class="hint">Versione installata: <b id="fwVersion"></b></p>
    <label for="fwFile">File del firmware</label>
    <input type="file" id="fwFile" accept=".bin">
    <p class="hint">Dopo <code>pio run</code>, carica <code>.pio/build/xhs3e/firmware.bin</code> (non <i>firmware.factory.bin</i>). La lampada lo verifica, si riavvia con il nuovo firmware e tiene tutte le impostazioni; se qualcosa va storto resta quello di prima.</p>
    <button class="save" id="fwUpload" disabled>Aggiorna</button>
    <progress id="fwProgress" max="100" value="0" hidden style="width:100%;margin-top:12px"></progress>
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

  // Modes: the one picked is highlighted (with the playlist on, the one
  // it is showing). The night schedule or the alarm may show something
  // else for a while: that is said below the list.
  const selected = s.playlistPos >= 0 ? s.active : s.mode;
  const box = $('modes');
  box.innerHTML = '';
  for (const m of s.modes) {
    const b = document.createElement('button');
    b.className = m.id === selected ? 'on' : '';
    b.textContent = m.name;
    if (m.id === selected && s.playlistPos >= 0) {
      const tag = document.createElement('small');
      tag.textContent = 'playlist';
      b.appendChild(tag);
    }
    b.onclick = () => post('/api/mode', { id: m.id }).then(() => status(m.name)).catch(fail);
    box.appendChild(b);
  }
  const override = $('override');
  override.hidden = s.active === selected;
  if (s.active === 'sunrise') {
    override.textContent = 'Sveglia in corso: la lampada mostra l\'alba. «' + modeName(selected) + '» torna dopo.';
  } else if (s.night) {
    const until = s.nightSun && s.weather && s.weather.sunrise >= 0 ? clock(s.weather.sunrise) : clock(s.nightEnd);
    const what = s.active === 'off' ? 'è spenta' : s.active === 'ambient' ? 'mostra le stelle' : 'mostra «' + s.activeMode.name + '»';
    override.textContent = 'Modalità notte fino alle ' + until + ': la lampada ' + what + '. «' + modeName(selected) + '» torna dopo (orari in «Giorno e notte»).';
  } else {
    override.textContent = 'Sulla lampada ora: «' + s.activeMode.name + '».';
  }

  const active = s.activeMode;  // may be a hidden mode (the alarm's sunrise)
  $('action').hidden = !active.action;
  $('action').textContent = active.action || '';
  $('speedBox').hidden = !active.hasSpeed;
  if (!editing('speed')) $('speed').value = active.speed;

  renderGame();
  renderExtras();

  // Only the picked mode's own settings.
  for (const sec of document.querySelectorAll('section[data-mode]')) sec.hidden = sec.dataset.mode !== selected;

  if (!editing('text')) $('text').value = s.text;
  $('textPos').value = s.textPos;
  $('demoStyle').value = s.demoStyle;
  if (selected === 'quotes' && !quotesLoaded) loadQuotes().catch(() => {});
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
  $('textFont').value = s.textFont;
  $('textFontText').value = s.textFont;
  $('transition').value = s.transition;
  $('fwVersion').textContent = s.version;
}

// Game box: demo checkbox and, when the player is in control, the pad.
const PADS = {
  mario: { keys: ['A'], labels: { A: 'Salta' }, hint: 'Tastiera: barra spaziatrice o freccia su per saltare.' },
  tetris: { keys: ['L', 'R', 'U', 'D'], labels: { U: '↻', D: '⤓' }, hint: 'Tastiera: ← → per spostare, ↑ per ruotare, ↓ o spazio per far cadere.' },
  snake: { keys: ['L', 'R', 'U', 'D'], labels: { U: '↑', D: '↓' }, hint: 'Tastiera: le frecce.' },
  pong: { keys: ['U', 'D'], labels: {}, repeat: true, hint: 'Racchetta di sinistra. Tastiera: ↑ ↓ (tieni premuto).' },
  breakout: { keys: ['L', 'R'], labels: {}, repeat: true, hint: 'Tastiera: ← → (tieni premuto).' },
  flappy: { keys: ['A'], labels: { A: 'Vola' }, hint: 'Tastiera: spazio o ↑.' },
  invaders: { keys: ['L', 'R', 'A'], labels: { A: 'Spara' }, repeat: true, hint: 'Tastiera: ← → per muoverti, spazio o ↑ per sparare.' },
  maze: { keys: ['L', 'R', 'U', 'D', 'A'], labels: { L: '↶', R: '↷', U: '↑', D: '↓', A: 'Mappa' }, hint: '↑ ↓ per camminare, ← → per girarti, Mappa per vedere dove sei. Trova il blocco che pulsa. Tastiera: le frecce e la barra spaziatrice.' },
};
function playable() { return state && state.game && !state.game.demo; }
function renderGame() {
  const g = state.game;
  $('gameBox').hidden = !g;
  // While you play, the preview sits right above the pad.
  const home = g && !g.demo ? $('gameBox') : $('previewBox');
  if ($('preview').parentNode !== home) home.insertBefore($('preview'), g && !g.demo ? $('pad') : null);
  $('previewBox').hidden = home !== $('previewBox');
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
// Paddle games repeat the arrow while it is held down.
let repeatTimer = null;
function stopRepeat() { clearInterval(repeatTimer); repeatTimer = null; }
for (const b of $('pad').querySelectorAll('button')) {
  b.addEventListener('pointerdown', (e) => {
    e.preventDefault();
    sendKey(b.dataset.key);
    const pad = state.game && PADS[state.game.id];
    stopRepeat();
    if (pad && pad.repeat && b.dataset.key !== 'A') repeatTimer = setInterval(() => sendKey(b.dataset.key), 110);
  });
  for (const ev of ['pointerup', 'pointerleave', 'pointercancel']) b.addEventListener(ev, stopRepeat);
}
$('demo').onchange = (e) => (e.target.blur(), post('/api/demo', { id: state.game.id, on: e.target.checked ? 1 : 0 }))
  .then(() => {
    status(e.target.checked ? 'Modalità demo' : 'Tocca a te!');
    if (!e.target.checked) $('gameBox').scrollIntoView({ behavior: 'smooth', block: 'start' });  // pad in view
  }).catch(fail);

// ---------------------------------------------------------------------------
// Sections of the newer modes, and the general alarm/night extras.
const DAY_NAMES = ['L', 'M', 'M', 'G', 'V', 'S', 'D'];
function clock(minutes) { return minutes < 0 ? '?' : Math.floor(minutes / 60) + ':' + String(minutes % 60).padStart(2, '0'); }
// WMO weather code -> emoji, grouped like the lamp's icons.
function weatherEmoji(code) {
  if (code < 0) return '·';
  if (code === 0) return '☀️';
  if (code <= 2) return '🌤️';
  if (code === 3) return '☁️';
  if (code === 45 || code === 48) return '🌫️';
  if ((code >= 71 && code <= 77) || code === 85 || code === 86) return '❄️';
  if (code >= 95) return '⛈️';
  if (code >= 51) return '🌧️';
  return '☁️';
}

function renderExtras() {
  const s = state;
  // Forecast: the same days as the lamp (today and the next 3), then 12
  // columns for the next hours, bar = rain probability, label = temperature.
  const fd = $('fcDays');
  fd.innerHTML = '';
  for (const [y, m, d, code, lo, hi, rain] of (s.weather ? s.weather.days : [])) {
    const date = new Date(y, m - 1, d);
    const today = new Date();
    const label = date.toDateString() === today.toDateString() ? 'Oggi'
      : date.toLocaleDateString('it-IT', { weekday: 'short' }).replace('.', '');
    const card = document.createElement('div');
    card.innerHTML = '<b>' + label + '</b><small>' + d + '/' + m + '</small><span class="ic">' + weatherEmoji(code) +
      '</span>' + Math.round(lo) + '° / <b>' + Math.round(hi) + '°</b><small>💧 ' + rain + '%</small>';
    fd.appendChild(card);
  }
  const fc = $('fcChart');
  fc.innerHTML = '';
  const hours = s.weather ? s.weather.hours : [];
  for (const [h, t, rain] of hours) {
    const col = document.createElement('div');
    col.innerHTML = '<span class="t">' + Math.round(t) + '°</span><div class="bar" style="height:' + Math.max(2, rain * 0.7) +
      'px" title="' + rain + '%"></div><span>' + h + '</span>';
    fc.appendChild(col);
  }
  $('fcInfo').textContent = s.forecast || 'Previsioni in arrivo...';

  if (!dirty.web) {
    $('infoWord').checked = s.web.word; $('infoHistory').checked = s.web.history; $('infoCalendar').checked = s.web.calendar;
    $('icalUrl').value = s.web.url; $('webPos').value = s.web.pos;
  }
  $('historyStatus').textContent = s.web.history ? 'Wikipedia: ' + (s.web.historyStatus || 'in attesa') : '';
  $('calendarStatus').textContent = s.web.calendar ? 'Stato: ' + (s.web.calendarStatus || 'in attesa') : '';
  $('webPreview').textContent = [s.web.wordText, s.web.event].filter(Boolean).join(' · ');

  if (!dirty.cd) { $('cdLabel').value = s.countdown.label; $('cdDate').value = s.countdown.date; $('cdTime').value = s.countdown.time; }
  $('cdInfo').textContent = s.countdown.sentence;

  const days = $('alarmDays');
  if (!days.children.length) {
    DAY_NAMES.forEach((d, i) => {
      const l = document.createElement('label');
      l.innerHTML = '<input type="checkbox" data-day="' + i + '"> ' + d;
      l.querySelector('input').oninput = () => { dirty.alarm = true; };
      days.appendChild(l);
    });
  }
  if (!dirty.alarm) {
    $('alarmOn').checked = s.alarm.on; $('alarmTime').value = hhmm(s.alarm.time);
    $('alarmRamp').value = s.alarm.ramp; $('alarmHold').value = s.alarm.hold;
    for (const c of days.querySelectorAll('input')) c.checked = !!(s.alarm.days & (1 << c.dataset.day));
  }

  if (!dirty.night) $('nightSun').checked = s.nightSun;
  const sun = s.weather && s.weather.sunset >= 0 ? '(oggi ' + clock(s.weather.sunset) + ' - ' + clock(s.weather.sunrise) + ')' : '';
  $('sunTimes').textContent = sun;
  for (const id of ['nightStart', 'nightEnd']) $(id).disabled = $('nightSun').checked;
  $('skyInfo').textContent = (s.weather && s.weather.sunrise >= 0 ? 'Oggi alba ' + clock(s.weather.sunrise) + ', tramonto ' + clock(s.weather.sunset) + ' · ' : '') +
    s.moon.name + ' (illuminata al ' + s.moon.lit + '%)';

  const picked = s.playlistPos >= 0 ? s.active : s.mode;
  if (picked === 'gallery' && !galleryLoaded) loadGallery();
  // The editor opens on the drawing the lamp is showing, not a blank page.
  if (picked === 'gallery' && ed.pristine && s.galleryCurrent) {
    ed.pristine = false;
    openDrawing(s.galleryCurrent, false);
  }
  highlightGallery();
}

for (const id of ['infoWord', 'infoHistory', 'infoCalendar', 'icalUrl', 'webPos']) $(id).addEventListener('input', () => { dirty.web = true; });
$('saveWeb').onclick = () => post('/api/web', {
  word: $('infoWord').checked ? 1 : 0, history: $('infoHistory').checked ? 1 : 0, calendar: $('infoCalendar').checked ? 1 : 0,
  url: $('icalUrl').value.trim(), pos: $('webPos').value,
}).then(() => { dirty.web = false; render(); status('Salvato: i dati arrivano in qualche secondo'); }).catch(fail);

for (const id of ['cdLabel', 'cdDate', 'cdTime']) $(id).addEventListener('input', () => { dirty.cd = true; });
$('saveCd').onclick = () => post('/api/countdown', { label: $('cdLabel').value, date: $('cdDate').value, time: $('cdTime').value || '00:00' })
  .then(() => { dirty.cd = false; render(); status('Conto alla rovescia salvato'); }).catch(fail);

for (const id of ['alarmOn', 'alarmTime', 'alarmRamp', 'alarmHold']) $(id).addEventListener('input', () => { dirty.alarm = true; });
$('saveAlarm').onclick = () => {
  let days = 0;
  for (const c of $('alarmDays').querySelectorAll('input')) if (c.checked) days |= 1 << c.dataset.day;
  post('/api/alarm', { on: $('alarmOn').checked ? 1 : 0, time: minutesOf($('alarmTime').value || '07:00'), days,
    ramp: $('alarmRamp').value, hold: $('alarmHold').value })
    .then(() => { dirty.alarm = false; render(); status('Sveglia salvata'); }).catch(fail);
};
$('testAlarm').onclick = () => post('/api/alarm', { cmd: 'test' }).then(() => status('Alba di prova: 1 minuto')).catch(fail);
$('nightSun').addEventListener('input', () => {
  dirty.night = true;
  for (const id of ['nightStart', 'nightEnd']) $(id).disabled = $('nightSun').checked;
});

// ---------------------------------------------------------------------------
// Pixel editor. Frames are 256 levels (0-255) row by row, like the panel.
const LEVELS = [255, 170, 100, 50, 20, 0];
const ed = { frames: [new Uint8Array(256)], cur: 0, level: 255, id: '', frameMs: 250, pristine: true };
const cv = $('canvas'), cx = cv.getContext('2d');
let galleryLoaded = false, galleryItems = [];
function b64(bytes) { let s = ''; for (let i = 0; i < bytes.length; i += 4096) s += String.fromCharCode.apply(null, bytes.subarray(i, i + 4096)); return btoa(s); }
function unb64(s) { return Uint8Array.from(atob(s), (c) => c.charCodeAt(0)); }
function allFrames() { const out = new Uint8Array(ed.frames.length * 256); ed.frames.forEach((f, i) => out.set(f, i * 256)); return out; }
function gray(v) { return 'rgb(' + v + ',' + v + ',' + v + ')'; }

function drawCanvas() {
  const f = ed.frames[ed.cur], size = cv.width / 16;
  for (let i = 0; i < 256; i++) {
    cx.fillStyle = gray(f[i]);
    cx.fillRect((i % 16) * size, Math.floor(i / 16) * size, size, size);
  }
  cx.strokeStyle = 'rgba(128,128,128,0.25)';
  for (let k = 1; k < 16; k++) {
    cx.beginPath(); cx.moveTo(k * size, 0); cx.lineTo(k * size, cv.height); cx.stroke();
    cx.beginPath(); cx.moveTo(0, k * size); cx.lineTo(cv.width, k * size); cx.stroke();
  }
  $('frameInfo').textContent = 'Fotogramma ' + (ed.cur + 1) + ' di ' + ed.frames.length;
}

// Live preview on the lamp while drawing (throttled).
let draftTimer = null;
function sendDraft(all) {
  clearTimeout(draftTimer);
  draftTimer = setTimeout(() => {
    const data = all ? allFrames() : ed.frames[ed.cur];
    fetch('/api/draw', { method: 'POST', body: new URLSearchParams({ data: b64(data), frameMs: ed.frameMs }) }).catch(() => {});
  }, all ? 0 : 120);
}

const swatches = $('swatches');
for (const level of LEVELS) {
  const b = document.createElement('button');
  b.style.background = gray(level);
  b.title = level ? 'Luminosità ' + Math.round(level / 2.55) + '%' : 'Gomma';
  b.onclick = () => { ed.level = level; for (const x of swatches.children) x.classList.toggle('on', x === b); };
  if (level === 255) b.classList.add('on');
  swatches.appendChild(b);
}
let painting = false;
function paint(e) {
  ed.pristine = false;
  const r = cv.getBoundingClientRect();
  const x = Math.floor((e.clientX - r.left) / r.width * 16), y = Math.floor((e.clientY - r.top) / r.height * 16);
  if (x < 0 || x > 15 || y < 0 || y > 15) return;
  const f = ed.frames[ed.cur];
  if (f[y * 16 + x] === ed.level) return;
  f[y * 16 + x] = ed.level;
  drawCanvas();
  sendDraft(false);
}
cv.addEventListener('pointerdown', (e) => { painting = true; cv.setPointerCapture(e.pointerId); paint(e); });
cv.addEventListener('pointermove', (e) => { if (painting) paint(e); });
for (const ev of ['pointerup', 'pointercancel']) cv.addEventListener(ev, () => { painting = false; });

function edit(fn) { fn(ed.frames[ed.cur]); drawCanvas(); sendDraft(false); }
$('toolFill').onclick = () => edit((f) => f.fill(ed.level));
$('toolClear').onclick = () => edit((f) => f.fill(0));
$('toolInvert').onclick = () => edit((f) => { for (let i = 0; i < 256; i++) f[i] = 255 - f[i]; });
$('framePrev').onclick = () => { ed.cur = (ed.cur + ed.frames.length - 1) % ed.frames.length; drawCanvas(); sendDraft(false); };
$('frameNext').onclick = () => { ed.cur = (ed.cur + 1) % ed.frames.length; drawCanvas(); sendDraft(false); };
$('frameAdd').onclick = () => { if (ed.frames.length >= 32) return status('Al massimo 32 fotogrammi'); ed.frames.splice(ed.cur + 1, 0, new Uint8Array(256)); ed.cur++; drawCanvas(); sendDraft(false); };
$('frameDup').onclick = () => { if (ed.frames.length >= 32) return status('Al massimo 32 fotogrammi'); ed.frames.splice(ed.cur + 1, 0, ed.frames[ed.cur].slice()); ed.cur++; drawCanvas(); sendDraft(false); };
$('frameDel').onclick = () => { if (ed.frames.length === 1) return edit((f) => f.fill(0)); ed.frames.splice(ed.cur, 1); ed.cur = Math.min(ed.cur, ed.frames.length - 1); drawCanvas(); sendDraft(false); };
$('framePlay').onclick = () => { sendDraft(true); status('Anteprima sulla lampada'); };
$('fps').onchange = (e) => { ed.frameMs = +e.target.value; if (ed.frames.length > 1) sendDraft(true); };
$('newDrawing').onclick = () => { ed.pristine = false; ed.frames = [new Uint8Array(256)]; ed.cur = 0; ed.id = ''; $('drawName').value = ''; drawCanvas(); sendDraft(false); };

$('saveDrawing').onclick = async () => {
  try {
    const res = await fetch('/api/gallery/save', { method: 'POST', body: new URLSearchParams({
      id: ed.id, name: $('drawName').value.trim() || 'Disegno', frameMs: ed.frameMs, data: b64(allFrames()) }) });
    if (!res.ok) throw new Error(await res.text());
    ed.id = (await res.json()).id;
    status('Salvato nella galleria');
    loadGallery();
  } catch (e) { fail(e); }
};

// Loads a saved drawing into the editor; `live` also shows it as a draft
// on the lamp (not needed when it is already what the lamp shows).
async function openDrawing(id, live) {
  try {
    const it = await (await fetch('/api/gallery/item?id=' + encodeURIComponent(id))).json();
    const all = unb64(it.data);
    ed.frames = []; for (let i = 0; i < all.length; i += 256) ed.frames.push(all.slice(i, i + 256));
    ed.cur = 0; ed.id = it.id; ed.frameMs = it.frameMs; ed.pristine = false; $('drawName').value = it.name;
    $('fps').value = [...$('fps').options].reduce((a, o) => Math.abs(o.value - it.frameMs) < Math.abs(a - it.frameMs) ? +o.value : a, 250);
    drawCanvas();
    if (live) { sendDraft(true); cv.scrollIntoView({ behavior: 'smooth' }); }
  } catch (e) { /* keep what the editor has */ }
}

async function loadGallery() {
  galleryLoaded = true;
  try { galleryItems = await (await fetch('/api/gallery')).json(); } catch (e) { return; }
  const box = $('gallery');
  box.innerHTML = galleryItems.length ? '' : '<p class="hint">Ancora nessun disegno salvato.</p>';
  for (const d of galleryItems) {
    const row = document.createElement('div');
    row.className = 'item';
    row.dataset.id = d.id;
    const thumb = document.createElement('canvas');
    thumb.width = thumb.height = 16;
    const img = thumb.getContext('2d').createImageData(16, 16), px = unb64(d.thumb);
    for (let i = 0; i < 256; i++) { img.data.set([px[i], px[i], px[i], 255], i * 4); }
    thumb.getContext('2d').putImageData(img, 0, 0);
    const name = document.createElement('span');
    name.className = 'name';
    name.textContent = d.name + (d.frames > 1 ? ' (' + d.frames + ' fotogrammi)' : '');
    const show = document.createElement('button'); show.textContent = 'Mostra';
    show.onclick = () => post('/api/gallery/show', { id: d.id }).then(() => status('Mostro «' + d.name + '»')).catch(fail);
    const open = document.createElement('button'); open.textContent = 'Modifica';
    open.onclick = () => openDrawing(d.id, true);
    const del = document.createElement('button'); del.textContent = '✕';
    del.onclick = async () => {
      if (!confirm('Eliminare «' + d.name + '»?')) return;
      await fetch('/api/gallery/delete', { method: 'POST', body: new URLSearchParams({ id: d.id }) });
      if (ed.id === d.id) ed.id = '';
      loadGallery(); refresh();
    };
    row.append(thumb, name, show, open, del);
    box.appendChild(row);
  }
  highlightGallery();
}
function highlightGallery() {
  for (const row of $('gallery').children) row.classList && row.classList.toggle('on', row.dataset.id === state.galleryShow);
  $('showAll').classList.toggle('on', state.galleryShow === 'all');
}
$('showAll').onclick = () => post('/api/gallery/show', { id: 'all' }).then(() => status('Tutti i disegni a rotazione')).catch(fail);

// Image / GIF import: centre-crop to a square, scale to 16x16, brightness
// from luminance; animated GIFs frame by frame where the browser can
// decode them (ImageDecoder), otherwise just the first frame.
function toFrame(src, w, h) {
  const c = document.createElement('canvas');
  c.width = c.height = 16;
  const g = c.getContext('2d');
  g.imageSmoothingEnabled = true; g.imageSmoothingQuality = 'high';
  const side = Math.min(w, h);
  g.drawImage(src, (w - side) / 2, (h - side) / 2, side, side, 0, 0, 16, 16);
  const d = g.getImageData(0, 0, 16, 16).data, out = new Uint8Array(256);
  for (let i = 0; i < 256; i++) out[i] = Math.round((0.2126 * d[i * 4] + 0.7152 * d[i * 4 + 1] + 0.0722 * d[i * 4 + 2]) * d[i * 4 + 3] / 255);
  return out;
}
$('importFile').onchange = async (e) => {
  const file = e.target.files[0];
  if (!file) return;
  ed.pristine = false;
  status('Converto ' + file.name + '...');
  try {
    const frames = [];
    let duration = 0;
    if (window.ImageDecoder && file.type === 'image/gif') {
      const dec = new ImageDecoder({ data: await file.arrayBuffer(), type: file.type });
      await dec.tracks.ready;
      const n = Math.min(dec.tracks.selectedTrack.frameCount, 32);
      for (let i = 0; i < n; i++) {
        const { image } = await dec.decode({ frameIndex: i });
        frames.push(toFrame(image, image.displayWidth, image.displayHeight));
        duration += (image.duration || 100000) / 1000;
        image.close();
      }
    } else {
      const bmp = await createImageBitmap(file);
      frames.push(toFrame(bmp, bmp.width, bmp.height));
    }
    // Stretch the contrast over all frames, and/or invert.
    let lo = 255, hi = 0;
    for (const f of frames) for (const v of f) { lo = Math.min(lo, v); hi = Math.max(hi, v); }
    for (const f of frames) {
      for (let i = 0; i < 256; i++) {
        let v = f[i];
        if ($('importContrast').checked && hi > lo) v = Math.round((v - lo) * 255 / (hi - lo));
        if ($('importInvert').checked) v = 255 - v;
        f[i] = v;
      }
    }
    ed.frames = frames; ed.cur = 0; ed.id = '';
    if (frames.length > 1) {
      ed.frameMs = Math.max(40, Math.min(1000, Math.round(duration / frames.length)));
      $('fps').value = [...$('fps').options].reduce((a, o) => Math.abs(o.value - ed.frameMs) < Math.abs(a - ed.frameMs) ? +o.value : a, 250);
    }
    $('drawName').value = file.name.replace(/\.[^.]+$/, '').slice(0, 40);
    drawCanvas();
    sendDraft(frames.length > 1);
    status(frames.length > 1 ? frames.length + ' fotogrammi importati: premi Salva per tenerli' : 'Immagine importata: premi Salva per tenerla');
  } catch (err) { status('Impossibile leggere questa immagine'); }
  e.target.value = '';
};
drawCanvas();

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
for (const id of ['textPos']) {
  $(id).onchange = (e) => post('/api/settings', { [id]: e.target.value }).then(() => status('Altezza cambiata')).catch(fail);
}
// The list can be long, so it isn't part of the state: it is fetched when
// the quotes section is shown and after every change.
let quotesLoaded = false;
function loadQuotes() {
  quotesLoaded = true;
  return fetch('/api/quotes').then((r) => r.json()).then((q) => {
    if (!dirty.quotes) $('quotes').value = q.quotes;
    $('quotesInfo').textContent = (q.custom ? 'Le tue frasi: ' : 'Frasi predefinite: ') + q.count + '.';
  }).catch((e) => { quotesLoaded = false; throw e; });
}
$('saveQuotes').onclick = () => post('/api/quotes', { quotes: $('quotes').value })
  .then(() => { dirty.quotes = false; return loadQuotes(); }).then(() => status('Frasi salvate')).catch(fail);
$('resetQuotes').onclick = () => post('/api/quotes', { quotes: '' })
  .then(() => { dirty.quotes = false; return loadQuotes(); }).then(() => status('Frasi predefinite ripristinate')).catch(fail);
$('openPlace').onclick = () => { $('placeBox').open = true; $('placeBox').scrollIntoView({ behavior: 'smooth' }); };
$('demoStyle').onchange = (e) => post('/api/settings', { demoStyle: e.target.value })
  .then(() => status('Stile cambiato')).catch(fail);
$('ambient').onchange = (e) => post('/api/settings', { ambient: e.target.value }).then(() => status('Animazione cambiata')).catch(fail);

$('playlistOn').onchange = () => { dirty.playlist = true; };
$('addItem').onclick = () => { addPlaylistRow(state.modes[0].id, 5); dirty.playlist = true; };
$('savePlaylist').onclick = () => post('/api/playlist', { on: $('playlistOn').checked ? 1 : 0, items: playlistValue() })
  .then(() => { dirty.playlist = false; render(); status('Playlist salvata'); }).catch(fail);

for (const id of ['nightOn', 'nightStart', 'nightEnd', 'nightMode', 'nightBrightness']) {
  $(id).addEventListener('input', () => { dirty.night = true; $('nightDimBox').hidden = $('nightMode').value !== 'dim'; });
}
$('saveNight').onclick = () => post('/api/night', {
  on: $('nightOn').checked ? 1 : 0, sun: $('nightSun').checked ? 1 : 0,
  start: minutesOf($('nightStart').value), end: minutesOf($('nightEnd').value),
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
// The font can be picked in Display and in the scrolling text's section.
for (const id of ['textFont', 'textFontText']) {
  $(id).onchange = (e) => {
    $('textFont').value = $('textFontText').value = e.target.value;
    post('/api/settings', { textFont: e.target.value }).then(() => status('Font cambiato')).catch(fail);
  };
}
$('transition').onchange = (e) => post('/api/settings', { transition: e.target.value })
  .then(() => status('Passaggio: ' + e.target.selectedOptions[0].textContent.toLowerCase())).catch(fail);
$('brightness').onchange = (e) => post('/api/settings', { brightness: e.target.value }).catch(fail);

// Firmware update: upload with progress, then wait for the lamp to come
// back with the new version.
$('fwFile').onchange = () => { $('fwUpload').disabled = !$('fwFile').files.length; };
$('fwUpload').onclick = () => {
  const file = $('fwFile').files[0];
  if (!file) return;
  if (!/\.bin$/i.test(file.name) || /factory/i.test(file.name)) {
    status('Scegli il file firmware.bin (non firmware.factory.bin)');
    return;
  }
  const before = state.version;
  const bar = $('fwProgress');
  bar.hidden = false; bar.value = 0;
  $('fwUpload').disabled = true;
  const form = new FormData();
  form.append('firmware', file, file.name);
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/update');
  xhr.upload.onprogress = (e) => {
    if (e.lengthComputable) { bar.value = Math.round(e.loaded * 100 / e.total); status('Caricamento ' + bar.value + '%'); }
  };
  xhr.onload = () => {
    if (xhr.status !== 200) {
      status(xhr.responseText || 'Aggiornamento non riuscito');
      $('fwUpload').disabled = false; bar.hidden = true;
      return;
    }
    status('Firmware caricato: la lampada si riavvia...');
    const started = Date.now();
    const wait = setInterval(() => {
      fetch('/api/state').then((r) => r.json()).then((s) => {
        clearInterval(wait);
        state = s; render(); bar.hidden = true; $('fwFile').value = ''; $('fwUpload').disabled = true;
        status(s.version !== before ? 'Aggiornamento completato: versione ' + s.version : 'La lampada è ripartita');
      }).catch(() => {
        if (Date.now() - started > 90000) { clearInterval(wait); status('La lampada non risponde: controlla che sia accesa e connessa'); }
      });
    }, 2000);
  };
  xhr.onerror = () => { status('Caricamento interrotto: riprova'); $('fwUpload').disabled = false; bar.hidden = true; };
  xhr.send(form);
};

// Live preview: what the lamp shows, polled a few times a second while
// the page is in view. Off LEDs are faint dots, lit ones white discs.
const preview = $('preview'), pctx = preview.getContext('2d');
let previewBusy = false;
function drawPreview(hex) {
  if (hex.length !== 512) return;
  const cell = preview.width / 16;
  pctx.fillStyle = '#000';
  pctx.fillRect(0, 0, preview.width, preview.height);
  for (let i = 0; i < 256; i++) {
    const v = parseInt(hex.substr(i * 2, 2), 16);
    pctx.fillStyle = v ? 'rgba(255,255,255,' + (0.15 + 0.85 * v / 255).toFixed(3) + ')' : '#1c1c1c';
    pctx.beginPath();
    pctx.arc((i % 16 + 0.5) * cell, ((i >> 4) + 0.5) * cell, cell * 0.4, 0, 2 * Math.PI);
    pctx.fill();
  }
}
setInterval(() => {
  if (document.hidden || previewBusy) return;
  previewBusy = true;
  fetch('/api/frame').then((r) => r.text()).then(drawPreview).catch(() => {}).finally(() => { previewBusy = false; });
}, 200);

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

  auto modeJson = [](const Mode *m) {
    String j = "{\"id\":" + jsonString(m->id()) + ",\"name\":" + jsonString(m->name());
    j += ",\"action\":" + (m->actionName() ? jsonString(m->actionName()) : String("null"));
    return j + ",\"hasSpeed\":" + jsonBool(m->hasSpeed()) + ",\"speed\":" + String(speedLevel(m->id())) + "}";
  };
  json += ",\"modes\":[";
  bool first = true;
  for (uint8_t i = 0; i < MODE_COUNT; i++) {
    if (MODES[i]->hidden()) continue;
    if (!first) json += ',';
    first = false;
    json += modeJson(MODES[i]);
  }
  json += "],\"activeMode\":" + modeJson(currentMode());

  json += ",\"text\":" + jsonString(settings.text) + ",\"textFont\":" + jsonString(settings.textFont);
  json += ",\"textPos\":" + jsonString(settings.textPosition);
  json += ",\"brightness\":" + String(settings.brightness) + ",\"vertical\":" + jsonBool(settings.vertical);
  json += ",\"transition\":" + jsonString(settings.transition);
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

  json += ",\"forecast\":" + jsonString(ForecastMode::summary());
  const WebInfo info = webInfoNow();
  json += ",\"web\":{\"word\":" + jsonBool(settings.infoWord) + ",\"history\":" + jsonBool(settings.infoHistory) +
          ",\"calendar\":" + jsonBool(settings.infoCalendar) + ",\"url\":" + jsonString(settings.icalUrl) +
          ",\"pos\":" + jsonString(settings.webPosition) + ",\"wordText\":" + jsonString(info.word) +
          ",\"event\":" + jsonString(info.event) + ",\"historyStatus\":" + jsonString(info.historyStatus) +
          ",\"calendarStatus\":" + jsonString(info.calendarStatus) + "}";
  json += ",\"countdown\":{\"label\":" + jsonString(settings.countdownLabel) + ",\"date\":" +
          jsonString(settings.countdownDate) + ",\"time\":" + jsonString(settings.countdownTime) +
          ",\"sentence\":" + jsonString(CountdownMode::sentence()) + "}";
  json += ",\"alarm\":{\"on\":" + jsonBool(settings.alarmOn) + ",\"time\":" + String(settings.alarmTime) +
          ",\"days\":" + String(settings.alarmDays) + ",\"ramp\":" + String(settings.alarmRamp) +
          ",\"hold\":" + String(settings.alarmHold) + "}";
  const float phase = moonPhase(time(nullptr));
  json += ",\"moon\":{\"name\":" + jsonString(moonPhaseName(phase)) + ",\"lit\":" +
          String((int)lroundf(moonIllumination(phase) * 100)) + "}";
  json += ",\"version\":" + jsonString(String(FIRMWARE_COMMIT) + " del " + FIRMWARE_BUILT);
  json += ",\"galleryCurrent\":" + jsonString(galleryMode().currentId());
  json += ",\"demoStyle\":" + jsonString(settings.demoStyle);
  json += ",\"galleryShow\":" + jsonString(settings.galleryShow) + ",\"nightSun\":" + jsonBool(settings.nightSun);

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
  const Weather weather = weatherNow();
  if (weather.valid) {
    json += ",\"weather\":{\"temp\":" + String(weather.temperature, 1) + ",\"code\":" + String(weather.code);
    json += ",\"rainSoon\":" + jsonBool(rainSoon(weather));
    json += ",\"sunrise\":" + String(weather.sunrise) + ",\"sunset\":" + String(weather.sunset) + ",\"hours\":[";
    for (int i = 0; i < weather.hours; i++) {
      if (i) json += ',';
      json += "[" + String((weather.firstHour + i) % 24) + "," + String(weather.hourlyTemp[i], 1) + "," +
              String(weather.hourlyRain[i]) + "]";
    }
    // Daily forecast as on the lamp: [year, month, day, code, min, max, rain %].
    json += "],\"days\":[";
    for (int i = 0; i < weather.days; i++) {
      if (i) json += ',';
      json += "[" + String(weather.dayYear[i]) + "," + String(weather.dayMonth[i]) + "," + String(weather.dayOfMonth[i]) + "," +
              String(weather.dayCode[i]) + "," + String(weather.dayMin[i], 1) + "," + String(weather.dayMax[i], 1) + "," +
              String(weather.dayRain[i]) + "]";
    }
    json += "]}";
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
  const Animation *a = findAnimation(id);
  if (!(a && a->isGame())) return badRequest("Gioco sconosciuto");
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
  if (quotes.length() > QUOTES_MAX) return badRequest(("Troppo testo: al massimo " + String(QUOTES_MAX) + " caratteri").c_str());
  settings.quotes = quotes;
  if (!saveQuotes()) return server.send(500, "text/plain", "Non riesco a salvare le frasi nella memoria della lampada");
  if (strcmp(currentMode()->id(), "quotes") == 0) restartMode();
  sendState();
}

// The quotes list being used (the user's, or the built-in one) and how
// many there are.
static void sendQuotes() {
  const bool custom = settings.quotes.length() > 0;
  const String list = custom ? settings.quotes : String(QuotesMode::defaultQuotes());
  server.send(200, "application/json",
              "{\"custom\":" + jsonBool(custom) + ",\"count\":" + String(QuotesMode::count()) +
                  ",\"quotes\":" + jsonString(list) + "}");
}

// What the panel shows right now, for the page's preview: 256 levels as
// hex, row by row from the top-left (as seen on the lamp).
static void handleFrame() {
  static const char HEX_DIGITS[] = "0123456789abcdef";
  char out[TOTAL_PIXELS * 2 + 1];
  int n = 0;
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      const uint8_t v = display.shownLevel(x, y);
      out[n++] = HEX_DIGITS[v >> 4];
      out[n++] = HEX_DIGITS[v & 15];
    }
  }
  out[n] = 0;
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/plain", out);
}

static void handleSettings() {
  if (server.hasArg("brightness")) settings.brightness = constrain(server.arg("brightness").toInt(), 1, 255);
  if (server.hasArg("textFont")) {
    const String font = server.arg("textFont");
    if (font != "small" && font != "big" && font != "mini") return badRequest("Font sconosciuto");
    settings.textFont = font;
    Display::setScrollFont(fontForSettings());
    restartMode();  // scrolling widths depend on the font
  }
  if (server.hasArg("transition")) {
    const String style = server.arg("transition");
    if (style != "fade" && style != "wipe" && style != "none") return badRequest("Passaggio sconosciuto");
    settings.transition = style;
    display.setTransition(transitionForSettings());
  }
  if (server.hasArg("vertical")) {
    settings.vertical = server.arg("vertical") == "1";
    display.setRotation(rotationForSettings());
    restartMode();  // redraw straight away in the new orientation
  }
  if (server.hasArg("textPos")) {
    const String pos = server.arg("textPos");
    if (pos != "random" && pos != "top" && pos != "middle" && pos != "bottom") return badRequest("Altezza non valida");
    settings.textPosition = pos;
    if (strcmp(currentMode()->id(), "text") == 0) restartMode();  // show it at the new height now
  }
  if (server.hasArg("demoStyle")) {
    const String style = server.arg("demoStyle");
    if (style != "auto" && style != "rows3" && style != "pages" && style != "rows2") return badRequest("Stile sconosciuto");
    settings.demoStyle = style;
    if (strcmp(currentMode()->id(), "demo") == 0) restartMode();  // a new quote in the new style
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
  requestWeatherUpdate();
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
  settings.nightSun = server.arg("sun") == "1";
  settings.nightStart = constrain(server.arg("start").toInt(), 0, 1439);
  settings.nightEnd = constrain(server.arg("end").toInt(), 0, 1439);
  settings.nightMode = mode;
  settings.nightBrightness = constrain(server.arg("brightness").toInt(), 1, 255);
  saveSettings();
  refreshModes();
  sendState();
}

static void handleWeb() {
  const String pos = server.arg("pos"), url = server.arg("url");
  if (pos != "random" && pos != "top" && pos != "middle" && pos != "bottom") return badRequest("Altezza non valida");
  if (url.length() > 500) return badRequest("Link troppo lungo");
  settings.infoWord = server.arg("word") == "1";
  settings.infoHistory = server.arg("history") == "1";
  settings.infoCalendar = server.arg("calendar") == "1";
  settings.icalUrl = url;
  settings.webPosition = pos;
  saveSettings();
  requestWebInfoUpdate();
  if (strcmp(currentMode()->id(), "web") == 0) restartMode();
  sendState();
}

static void handleCountdown() {
  const String date = server.arg("date"), time = server.arg("time");
  if (date.length() && date.length() != 10) return badRequest("Data non valida");
  if (time.length() != 5) return badRequest("Ora non valida");
  String label = server.arg("label");
  label.trim();
  settings.countdownLabel = label.length() ? label.substring(0, 40) : String("L'evento");
  settings.countdownDate = date;
  settings.countdownTime = time;
  saveSettings();
  if (strcmp(currentMode()->id(), "countdown") == 0) restartMode();
  sendState();
}

static void handleAlarm() {
  const String cmd = server.arg("cmd");
  if (cmd == "stop") {
    SunriseMode::dismiss();
  } else if (cmd == "test") {
    SunriseMode::test();
  } else {
    settings.alarmOn = server.arg("on") == "1";
    settings.alarmTime = constrain(server.arg("time").toInt(), 0, 1439);
    settings.alarmDays = server.arg("days").toInt() & 0x7F;
    settings.alarmRamp = constrain(server.arg("ramp").toInt(), 5, 60);
    settings.alarmHold = constrain(server.arg("hold").toInt(), 1, 120);
    saveSettings();
  }
  refreshModes();
  sendState();
}

// --- Gallery ---------------------------------------------------------------

static String base64(const uint8_t *data, size_t length) {
  size_t outLength = 0;
  mbedtls_base64_encode(nullptr, 0, &outLength, data, length);
  String out;
  out.reserve(outLength);
  std::vector<unsigned char> buf(outLength + 1);
  mbedtls_base64_encode(buf.data(), buf.size(), &outLength, data, length);
  buf[outLength] = 0;
  out = (const char *)buf.data();
  return out;
}

static bool unbase64(const String &text, std::vector<uint8_t> &out) {
  size_t length = 0;
  mbedtls_base64_decode(nullptr, 0, &length, (const unsigned char *)text.c_str(), text.length());
  out.resize(length);
  return mbedtls_base64_decode(out.data(), out.size(), &length, (const unsigned char *)text.c_str(), text.length()) == 0 &&
         (out.resize(length), true);
}

static void handleGalleryList() {
  String json = "[";
  bool first = true;
  for (const Drawing &d : galleryList()) {
    if (!first) json += ',';
    first = false;
    json += "{\"id\":" + jsonString(d.id) + ",\"name\":" + jsonString(d.name) + ",\"frames\":" + String(d.frameCount()) +
            ",\"frameMs\":" + String(d.frameMs) + ",\"thumb\":\"" + base64(d.frames.data(), 256) + "\"}";
  }
  server.send(200, "application/json", json + "]");
}

static void handleGalleryItem() {
  Drawing d;
  if (!galleryLoad(server.arg("id"), d)) return badRequest("Disegno non trovato");
  server.send(200, "application/json",
              "{\"id\":" + jsonString(d.id) + ",\"name\":" + jsonString(d.name) + ",\"frameMs\":" + String(d.frameMs) +
                  ",\"data\":\"" + base64(d.frames.data(), d.frames.size()) + "\"}");
}

static bool readFrames(std::vector<uint8_t> &frames, uint16_t &frameMs) {
  if (!unbase64(server.arg("data"), frames) || frames.empty() || frames.size() % 256 ||
      frames.size() / 256 > GALLERY_MAX_FRAMES) {
    return false;
  }
  frameMs = constrain(server.arg("frameMs").toInt(), 30, 2000);
  return true;
}

static void handleGallerySave() {
  Drawing d;
  if (!readFrames(d.frames, d.frameMs)) return badRequest("Disegno non valido");
  d.id = server.arg("id");
  d.name = server.arg("name");
  if (!gallerySave(d)) return badRequest("Impossibile salvare (galleria piena?)");
  server.send(200, "application/json", "{\"id\":" + jsonString(d.id) + "}");
}

static void handleGalleryDelete() {
  const String id = server.arg("id");
  galleryDelete(id);
  if (settings.galleryShow == id) {
    settings.galleryShow = "all";
    saveSettings();
  }
  if (strcmp(currentMode()->id(), "gallery") == 0) restartMode();
  server.send(204);
}

static void handleGalleryShow() {
  settings.galleryShow = server.arg("id") == "all" ? String("all") : server.arg("id");
  setMode("gallery");
  saveSettings();
  sendState();
}

// The editor's drawing in progress, shown live.
static void handleDraw() {
  std::vector<uint8_t> frames;
  uint16_t frameMs;
  if (!readFrames(frames, frameMs)) return badRequest("Disegno non valido");
  if (strcmp(currentMode()->id(), "gallery") != 0) setMode("gallery");
  galleryMode().showDraft(frames.data(), frames.size() / 256, frameMs);
  server.send(204);
}

// --- Firmware update (OTA) -----------------------------------------------------
// The new image goes into the spare app partition; only if it verifies does
// the lamp boot from it, so a bad upload leaves the running firmware alone.

static String updateError;

static void showUpdateProgress(size_t done, size_t total) {
  display.clear();
  const float filled = total ? (float)done / total * COLS : 0;
  for (int x = 0; x < COLS; x++) {
    display.setLevel(x, 7, 40);
    display.setLevel(x, 8, 40);
    const float f = filled - x;
    if (f > 0) {
      display.setLevel(x, 7, 40 + 215 * fminf(f, 1));
      display.setLevel(x, 8, 40 + 215 * fminf(f, 1));
    }
  }
  display.render();
}

static void handleUpdateUpload() {
  HTTPUpload &up = server.upload();
  const size_t total = server.clientContentLength();
  switch (up.status) {
    case UPLOAD_FILE_START:
      updateError = "";
      if (!up.filename.endsWith(".bin") || up.filename.indexOf("factory") >= 0) {
        updateError = "serve il file firmware.bin";
      } else if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
        updateError = Update.errorString();
      }
      showUpdateProgress(0, total);
      break;
    case UPLOAD_FILE_WRITE:
      if (updateError.length() == 0 && Update.write(up.buf, up.currentSize) != up.currentSize) {
        updateError = Update.errorString();  // e.g. "Wrong Magic Byte": not an ESP32 firmware
      }
      showUpdateProgress(up.totalSize + up.currentSize, total);
      break;
    case UPLOAD_FILE_END:
      if (updateError.length() == 0 && !Update.end(true)) updateError = Update.errorString();
      break;
    case UPLOAD_FILE_ABORTED:
      Update.abort();
      updateError = "caricamento interrotto";
      break;
  }
}

static void handleUpdateDone() {
  if (updateError.length() || Update.hasError()) {
    if (Update.isRunning()) Update.abort();
    server.send(400, "text/plain", "Aggiornamento non riuscito: " + updateError);
    restartMode();  // back to what was on the panel
    return;
  }
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", "ok");
  delay(500);  // let the answer reach the browser
  ESP.restart();
}

void webBegin() {
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", PAGE); });
  server.on("/api/state", HTTP_GET, sendState);
  server.on("/api/frame", HTTP_GET, handleFrame);
  server.on("/api/mode", HTTP_POST, handleMode);
  server.on("/api/action", HTTP_POST, handleAction);
  server.on("/api/speed", HTTP_POST, handleSpeed);
  server.on("/api/input", HTTP_POST, handleInput);
  server.on("/api/demo", HTTP_POST, handleDemo);
  server.on("/api/text", HTTP_POST, handleText);
  server.on("/api/quotes", HTTP_POST, handleQuotes);
  server.on("/api/quotes", HTTP_GET, sendQuotes);
  server.on("/api/settings", HTTP_POST, handleSettings);
  server.on("/api/location", HTTP_POST, handleLocation);
  server.on("/api/timezone", HTTP_POST, handleTimezone);
  server.on("/api/playlist", HTTP_POST, handlePlaylist);
  server.on("/api/night", HTTP_POST, handleNight);
  server.on("/api/web", HTTP_POST, handleWeb);
  server.on("/api/countdown", HTTP_POST, handleCountdown);
  server.on("/api/alarm", HTTP_POST, handleAlarm);
  server.on("/api/gallery", HTTP_GET, handleGalleryList);
  server.on("/api/gallery/item", HTTP_GET, handleGalleryItem);
  server.on("/api/gallery/save", HTTP_POST, handleGallerySave);
  server.on("/api/gallery/delete", HTTP_POST, handleGalleryDelete);
  server.on("/api/gallery/show", HTTP_POST, handleGalleryShow);
  server.on("/api/draw", HTTP_POST, handleDraw);
  server.on("/api/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
  server.begin();
}

void webLoop() { server.handleClient(); }
