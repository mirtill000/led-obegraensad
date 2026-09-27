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

  // Animazioni (grouped) and Giochi: the same list, split by kind.
  const sel = $('ambient'), gsel = $('games');
  if (!sel.options.length) {
    sel.add(new Option('Automatica (cambia ogni 5 minuti)', 'auto'));
    gsel.add(new Option('Automatica (cambia ogni 5 minuti, in demo)', 'auto'));
    let group = null;
    for (const a of s.animations) {
      if (a.game) { gsel.add(new Option(a.name, a.id)); continue; }
      if (!group || group.label !== a.group) { group = document.createElement('optgroup'); group.label = a.group; sel.appendChild(group); }
      group.appendChild(new Option(a.name, a.id));
    }
  }
  sel.value = s.ambient;
  gsel.value = s.games;
  const playing = s.animations.find((a) => a.id === s.animation);
  $('ambientInfo').textContent = playing && s.active === 'ambient' ? 'In riproduzione: ' + playing.name + (s.night ? ' (modalità notte)' : '') : '';
  $('gamesInfo').textContent = playing && s.active === 'games' ? 'In gioco: ' + playing.name : '';

  if (!dirty.playlist) {
    $('playlistOn').checked = s.playlistOn;
    $('scenesOn').checked = s.scenesOn;
    renderPlaylist(s.playlist.split(',').filter(Boolean).map((i) => i.split(':')));
    $('scenes').innerHTML = '';
    for (const scene of s.scenes.split(';').filter(Boolean)) {
      const [hhmm, bright, items] = scene.split('|');
      addScene(hhmm.slice(0, 2) + ':' + hhmm.slice(2), +bright, items);
    }
    showScenes();
  }
  [...$('scenes').children].forEach((el, i) => el.classList.toggle('on', i === s.scene));
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
  $('gameStyle').value = s.gameStyle;
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
  dino: { keys: ['U', 'D', 'A'], labels: { U: '↑', D: '↓', A: 'Salta' }, repeat: true, hint: '↑ o Salta per saltare, ↓ per abbassarti (tienilo premuto). Tastiera: frecce e spazio.' },
  kong: { keys: ['L', 'R', 'U', 'D', 'A'], labels: { U: '↑', D: '↓', A: 'Salta' }, repeat: true, hint: '← → per camminare, ↑ ↓ per le scale, Salta per scavalcare i barili. Tastiera: frecce e spazio.' },
  sonic: { keys: ['L', 'R', 'A'], labels: { A: 'Salta' }, repeat: true, hint: '→ per correre (tienilo premuto), ← per frenare, Salta per saltare in palla e distruggere i nemici. Tastiera: frecce e spazio.' },
  doom: { keys: ['L', 'R', 'U', 'D', 'A'], labels: { L: '↶', R: '↷', U: '↑', D: '↓', A: 'Spara' }, repeat: true, hint: '↑ ↓ per camminare, ← → per girarti, Spara per colpire il diavoletto al centro della vista. Tastiera: frecce e spazio.' },
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
    ? 'In «Automatica» i giochi vanno sempre in demo: scegline uno nel menu dei giochi per giocare.'
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
// Game keys go to port 81 on a kept-alive connection (no new connection
// per key); if that fails, to /api/input.
let fastKeys = true;
function sendKey(key) {
  const slow = () => fetch('/api/input', { method: 'POST', body: new URLSearchParams({ key }), keepalive: true }).catch(() => {});
  if (!fastKeys) return slow();
  fetch('http://' + location.hostname + ':81/input?k=' + key, { mode: 'no-cors', cache: 'no-store' })
    .catch(() => { fastKeys = false; slow(); });
}
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
  if (!editing('hgMin')) $('hgMin').value = String(s.hourglass.minutes);
  $('notifyNight').checked = s.notifyNight;
  $('bleOn').checked = s.ble.on;
  $('bleInfo').innerHTML = s.ble.on ? 'Nome: <b>obegransad</b> · PIN: <b>' + String(s.ble.pin).padStart(6, '0') + '</b> · '
    + (s.ble.connected ? 'telecomando collegato' : 'nessun telecomando collegato') : 'Spento.';
  const hl = s.hourglass.left;
  $('hgInfo').textContent = s.active !== 'hourglass' ? '' : !s.hourglass.running ? 'Tempo scaduto.'
    : 'Resta ' + (hl >= 60 ? Math.floor(hl / 60) + ' min ' : '') + (hl % 60) + ' s circa.';

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
function notifyAddress() {
  return 'http://' + location.host + '/api/notify?text=' + encodeURIComponent($('notifyText').value) +
    ($('notifyIcon').value ? '&icon=' + $('notifyIcon').value : '');
}
for (const id of ['notifyText', 'notifyIcon']) $(id).addEventListener('input', () => { $('notifyUrl').textContent = notifyAddress(); });
$('notifyUrl').textContent = notifyAddress();
$('notifySend').onclick = () => fetch('/api/notify', { method: 'POST', body: new URLSearchParams({ text: $('notifyText').value, icon: $('notifyIcon').value }) })
  .then((r) => r.ok ? r.json() : r.text().then((t) => { throw new Error(t); }))
  .then((j) => status(j.ok ? 'Notifica inviata' : 'Ignorata: è notte'))
  .catch(fail);
$('bleOn').onchange = () => {
  if (!confirm('La lampada si riavvia. Continuare?')) { $('bleOn').checked = !$('bleOn').checked; return; }
  fetch('/api/ble', { method: 'POST', body: new URLSearchParams({ on: $('bleOn').checked ? 1 : 0 }) }).catch(() => {});
  status('Riavvio in corso…');
};
$('bleForget').onclick = () => {
  if (!confirm('I telecomandi abbinati andranno riabbinati con un nuovo PIN, e la lampada si riavvia. Continuare?')) return;
  fetch('/api/ble', { method: 'POST', body: new URLSearchParams({ forget: 1 }) }).catch(() => {});
  status('Riavvio in corso…');
};
$('notifyNight').onchange = () => post('/api/settings', { notifyNight: $('notifyNight').checked ? 1 : 0 }).catch(fail);
$('hgMin').onchange = () => post('/api/hourglass', { minutes: $('hgMin').value });
$('hgStart').onclick = () => post('/api/hourglass', { minutes: $('hgMin').value, start: 1 });
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
function addPlaylistRow(id, minutes, list = $('playlist')) {
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
  list.appendChild(row);
}
function playlistValue(list = $('playlist')) {
  return [...list.children].map((row) => {
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
    $('quotesInfo').textContent = 'In rotazione: ' + q.count + ' frasi (' + q.builtIn + ' predefinite + ' + (q.count - q.builtIn) + ' tue).';
  }).catch((e) => { quotesLoaded = false; throw e; });
}
$('saveQuotes').onclick = () => post('/api/quotes', { quotes: $('quotes').value })
  .then(() => { dirty.quotes = false; return loadQuotes(); }).then(() => status('Frasi salvate')).catch(fail);
$('resetQuotes').onclick = () => post('/api/quotes', { quotes: '' })
  .then(() => { dirty.quotes = false; return loadQuotes(); }).then(() => status('Frasi aggiunte cancellate')).catch(fail);
$('openPlace').onclick = () => { $('placeBox').open = true; $('placeBox').scrollIntoView({ behavior: 'smooth' }); };
$('demoStyle').onchange = (e) => post('/api/settings', { demoStyle: e.target.value })
  .then(() => status('Stile cambiato')).catch(fail);
$('ambient').onchange = (e) => post('/api/settings', { ambient: e.target.value }).then(() => status('Animazione cambiata')).catch(fail);
$('games').onchange = (e) => post('/api/settings', { games: e.target.value }).then(() => status('Gioco cambiato')).catch(fail);

$('playlistOn').onchange = () => { dirty.playlist = true; };
// Time slots: start time, brightness (0 = as in Display) and their own list.
function showScenes() {
  $('scenesBox').hidden = !$('scenesOn').checked;
  $('plainPlaylist').hidden = $('scenesOn').checked;
  $('addScene').hidden = $('scenes').children.length >= 4;
}
function addScene(time, bright, items) {
  const box = document.createElement('div');
  box.className = 'scene';
  box.innerHTML = '<div class="row"><input type="time" class="st"><button class="x" title="Rimuovi fascia">✕</button></div>' +
    '<div class="row"><label style="margin:0;flex:0 0 auto">Luce</label><input type="range" class="sb" min="0" max="255"><span class="sv narrow"></span></div>' +
    '<div class="list items" style="margin-top:6px"></div><button class="link add">+ Aggiungi modalità</button>';
  const list = box.querySelector('.items'), sb = box.querySelector('.sb'), sv = box.querySelector('.sv');
  box.querySelector('.st').value = time;
  sb.value = bright;
  const label = () => { sv.textContent = +sb.value ? Math.round(sb.value / 2.55) + '%' : 'come Display'; };
  label();
  sb.oninput = () => { label(); dirty.playlist = true; };
  box.querySelector('.st').oninput = () => { dirty.playlist = true; };
  box.querySelector('.x').onclick = () => { box.remove(); dirty.playlist = true; showScenes(); };
  box.querySelector('.add').onclick = () => { addPlaylistRow(state.modes[0].id, 5, list); dirty.playlist = true; };
  for (const item of (items || '').split(',').filter(Boolean)) {
    const [id, min] = item.split(':');
    addPlaylistRow(id, min, list);
  }
  $('scenes').appendChild(box);
  showScenes();
}
function scenesValue() {
  return [...$('scenes').children].map((box) => {
    const t = box.querySelector('.st').value || '00:00';
    return t.replace(':', '') + '|' + box.querySelector('.sb').value + '|' + playlistValue(box.querySelector('.items'));
  }).join(';');
}
$('scenesOn').onchange = () => { dirty.playlist = true; showScenes(); };
$('addScene').onclick = () => { addScene('12:00', 0, 'clock:10'); dirty.playlist = true; };
$('addItem').onclick = () => { addPlaylistRow(state.modes[0].id, 5); dirty.playlist = true; };
$('savePlaylist').onclick = () => post('/api/playlist', { on: $('playlistOn').checked ? 1 : 0, items: playlistValue(),
    scenesOn: $('scenesOn').checked ? 1 : 0, scenes: scenesValue() })
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
$('gameStyle').onchange = (e) => post('/api/settings', { gameStyle: e.target.value })
  .then(() => status('Grafica dei giochi: ' + e.target.selectedOptions[0].textContent.split(':')[0].toLowerCase())).catch(fail);
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
let previewBusy = false, live = false;
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
  if (live || document.hidden || previewBusy) return;
  previewBusy = true;
  fetch('/api/frame').then((r) => r.text()).then(drawPreview).catch(() => {}).finally(() => { previewBusy = false; });
}, 200);

// Diagnostics: fetched every 2 s while the section is open.
function duration(s) {
  const d = Math.floor(s / 86400), h = Math.floor(s / 3600) % 24, m = Math.floor(s / 60) % 60;
  return (d ? d + ' g ' : '') + (d || h ? h + ' h ' : '') + m + ' min';
}
function loadDiag() {
  if (!$('diagBox').open || document.hidden) return;
  fetch('/api/diag').then((r) => r.json()).then((d) => {
    const kb = (b) => Math.round(b / 1024) + ' kB';
    const ago = (s) => s < 0 ? 'mai' : s < 90 ? s + ' s fa' : duration(s) + ' fa';
    const rows = [
      ['Sistema'],
      ['Acceso da', duration(d.uptime)],
      ['Ultimo riavvio', d.reset],
      ['Memoria libera (minima)', kb(d.heap) + ' (' + kb(d.minHeap) + ')'],
      ['PSRAM libera', kb(d.psram)],
      ['Temperatura del chip', d.chipTemp.toFixed(0) + ' °C'],
      ['Firmware', d.version],
      ['Rete'],
      ['Wi-Fi', d.ssid + ' · ' + d.rssi + ' dBm (' + (d.rssi > -60 ? 'ottimo' : d.rssi > -70 ? 'buono' : d.rssi > -80 ? 'debole' : 'pessimo') + ')'],
      ['Indirizzo', d.ip],
      ['Pagine in diretta', d.live + (live ? ' (questa compresa)' : ' · questa pagina interroga ogni 0,2 s')],
      ['Meteo', d.weather + ' · ' + ago(d.weatherAge)],
      ['Wikipedia', d.history || '—'],
      ['Calendario', d.calendar || '—'],
      ['Ciclo principale', d.loop.perSec.toLocaleString('it-IT') + ' giri al secondo · il più lungo ' + d.loop.maxMs.toLocaleString('it-IT') + ' ms'],
      ['LED (scala di grigi)'],
    ];
    if (d.refresh.hw) {
      rows.push(['Rinfresco', 'timer hardware · ' + (1e6 / d.refresh.cycleUs).toFixed(0) + ' Hz']);
      rows.push(['Cambi di livello', d.refresh.planes.toLocaleString('it-IT')]);
      rows.push(['Cambi di livello saltati', d.refresh.missed.toLocaleString('it-IT')]);
      rows.push(['Ritardo medio / massimo', d.refresh.avg + ' / ' + d.refresh.max + ' µs']);
    } else {
      rows.push(['Rinfresco', 'esp_timer (senza statistiche)']);
    }
    $('diag').innerHTML = rows.map((r) => r.length === 1 ? '<tr><th colspan="2">' + r[0] + '</th></tr>'
      : '<tr><td>' + r[0] + '</td><td>' + r[1] + '</td></tr>').join('');
  }).catch(() => {});
}
$('diagBox').addEventListener('toggle', loadDiag);
setInterval(loadDiag, 2000);
$('diagReset').onclick = () => fetch('/api/diag/reset', { method: 'POST' }).then(loadDiag).catch(fail);

function refresh() { return fetch('/api/state').then((r) => r.json()).then((s) => { state = s; render(); }); }
refresh().catch(() => status('Lampada non raggiungibile'));
setInterval(() => { if (!live) refresh().catch(() => {}); }, 15000);

// Live updates: the lamp pushes the preview and the state (Server-Sent
// Events on port 81). While the connection is up the polling above pauses;
// if it drops, polling takes over until the browser reconnects.
function startLive() {
  if (!window.EventSource) return;
  const es = new EventSource('http://' + location.hostname + ':81/events');
  es.addEventListener('frame', (e) => { live = true; if (!document.hidden) drawPreview(e.data); });
  es.addEventListener('state', (e) => {
    live = true;
    try { state = JSON.parse(e.data); render(); } catch (err) {}
  });
  es.onerror = () => { live = false; };
}
startLive();
