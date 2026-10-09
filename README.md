# OBEGRÄNSAD on Sparkle IoT XH-S3E

Standalone firmware that drives the salvaged IKEA OBEGRÄNSAD 16x16 LED
matrix from a **Sparkle IoT XH-S3E** board (ESP32-S3-WROOM-1-N16R8, 16MB
flash / 8MB octal PSRAM, WiFi+BT). A small web page over WiFi switches
between modes: scrolling text (by default **dare mighty things**), a quote
of the hour, clock + weather, a 12-hour forecast, things from the web
(word of the day, "on this day", your calendar), Conway's Game of Life,
Super Mario and other games, animations, your own drawings, a sand timer
and a sunrise alarm. (Inspired by
[ph1p/ikea-led-obegraensad](https://github.com/ph1p/ikea-led-obegraensad),
which this reuses the panel's shift-register wiring table from.)

## How the panel works

Behind the diffuser are 4 identical 8x8 plates (256 LEDs total) driven by a
single **256-bit shift-register chain** - not a multiplexed row/column
matrix. Four signals control it:

- **IN** - serial data in
- **CLK** - shift clock
- **CLA** - latch/store clock
- **EN** - output enable (active low)

Because the registers hold their state, the MCU only needs to push a new
256-bit frame when the image changes. Each LED is just on or off, and EN
(PWM) dims the whole panel at once.

### Grayscale

The panel's driver chips only know on or off for each LED (the one EN pin
dims the whole panel at once), so levels in between are made in time,
with binary code modulation: each pixel's brightness (0-255,
gamma-corrected to 32 steps) is split into 5 bit planes that are shown in
a loop, plane *n* for 2^n x 100 us. A full cycle takes 3.1 ms (~320 Hz).
The animations use it for fading trails, fire, twinkling stars and soft
edges.

Each plane has to change at exactly the right moment, or the LEDs visibly
tremble. A hardware timer (gptimer) on CPU core 1 - WiFi runs on core 0 -
fires once per plane, when its time is up (about 1600 interrupts a second,
re-armed each time for the next plane's length), and its interrupt wakes a
top-priority task on the same core, which latches the plane already
shifted into the registers and then shifts in the next one while it is
shown: the switch happens on time, whatever the network is doing. It
costs a few percent of core 1. `REFRESH_HW_TIMER` in
`include/constants.h` switches back to the older timing (an esp_timer
callback on core 0, which WiFi traffic can delay); `GRAYSCALE false` goes
back to plain on/off pixels.

## Wiring

1. Open the lamp (see below) and unplug/remove the original controller
   board from the lowest plate's 6-pin connector.
2. Wire the connector to the XH-S3E as follows:

| Panel connector | XH-S3E (ESP32-S3-WROOM-1-N16R8) | Notes |
| :--------------: | :------------------------------: | ----- |
| GND | GND | common ground |
| VCC | 5V | see power notes below |
| EN | GPIO5 | active low, keep held low |
| IN | GPIO9 | SPI MOSI |
| CLK | GPIO7 | SPI SCK |
| CLA | GPIO6 | latch |
| BUTTON (optional) | GPIO4 | other leg to GND |

These GPIOs were chosen deliberately for the **N16R8** variant of this
module:

- **GPIO26-37** are wired internally to the octal SPI flash and octal PSRAM
  on the N16R8 module - do not use them as GPIO, you'll crash the chip.
- **GPIO19/20** are the native USB D-/D+ lines.
- **GPIO43/44** are the default UART0 TX/RX used by the onboard USB-serial
  chip for flashing/`Serial`.
- **GPIO0/3/45/46** are strapping pins sampled at boot (boot mode, flash
  voltage) - best left untouched.

GPIO4/5/6/7/9 avoid all of the above and are the same assignment the
reference project already uses for ESP32-S3 boards.

### Power

- Run a separate 5V feed to the panel's VCC rather than sourcing it off the
  XH-S3E's onboard 5V/3.3V regulator - the panel can pull more current than a
  small dev board regulator likes, and a sagging supply is the classic cause
  of flicker or a black panel (see the original project's troubleshooting
  notes). A 5V/2A USB supply into the panel's VCC/GND, with GND common to
  the XH-S3E, works well.
- The panel's logic inputs are driven from a 5V rail but read a 3.3V ESP32-S3
  output as HIGH just fine in practice (this is exactly how the reference
  project runs on 3.3V boards) - just make sure GND is common between the
  two supplies.

### Opening the lamp

IKEA uses rivets, not screws: slide a screwdriver between a rivet and the
back panel and pry gently, or drill the rivets out for a cleaner (but
permanent) opening.

## Firmware

```
include/
  constants.h        - pins, matrix size, rotation, default text, WiFi names
  secrets.example.h  - template for your WiFi credentials (copy to secrets.h)
  font_small.h       - 8px-tall proportional font (a-z, A-Z, 0-9, . , : ; ! ? ' - %)
  font_mini.h        - 5px-tall capitals (Previsioni and the "Piccola" choice; "Grande" is built from font_small)
  font_compact.h     - font_small with letters one pixel narrower (Media, when horizontal)
  font_short.h       - font_small folded to 6 rows, lowercase (Media, when vertical)
  font_tiny.h        - 4px-tall capitals ("Minima" and Demo: three lines)
  display.h, modes.h, settings.h, web.h
  ui.h               - shared look of the panel (label, value, card, brightness steps, tone, waiting dots)
  sprite.h           - the one picture format (rows of characters, shades, frames, marks)
  sprite_atlas.h     - every sprite of the atlas, by name (spr::PET_FROG, spr::WEATHER_RAIN...)
  catalog.h          - the scene catalog: one id for everything the lamp can show
  texts.h            - the firmware's messages (errors, download states), in one place
src/
  display.cpp        - shift-register driver, font renderer, brightness, transitions
  ui.cpp             - the shared panel components, "waiting" and "no WiFi" signs
  sprite_atlas.cpp   - the atlas: every picture the lamp draws (icons, pet, weather, game characters)
  catalog.cpp        - the scene catalog (modes, animations, games, drawings)
  sprites.cpp        - drawing sprites; retouches from the page (LittleFS /sprites/)
  modes.cpp          - list of modes + switching between them
  modes/             - one file per mode (board.cpp: the Game of Life's drawing board)
  animations/        - the animations and games ("Animazioni", "Giochi")
  settings.cpp       - SETTING_DEFS: every setting described once; flash (NVS), validation, migrations
  backup.cpp         - all settings to one file and back
  events.cpp         - event log; keeps a new firmware only once it runs fine (rollback)
  timekeeping.cpp    - NTP time sync (time zone: TIMEZONE in constants.h)
  net.cpp            - background task for everything downloaded
  weather.cpp        - weather, 12-hour forecast, sunrise/sunset (Open-Meteo)
  webinfo.cpp        - word of the day, Wikipedia "on this day", iCal calendar
  world.cpp          - air quality and the Space Station's position
  netfetch.cpp       - the one HTTPS client, retry rule, status and flash cache for all of them
  live.cpp           - the frame and short-state formats shared by the page's live channel and Bluetooth
  formula.cpp        - the Formule mode's expression compiler and evaluator
  moon.cpp           - moon phase from the date, the sun's position
  occasions.cpp      - special days (New Year, Christmas, Halloween, ..., birthdays in the calendar)
  gallery.cpp        - drawings saved in flash (LittleFS)
  ble.cpp            - Bluetooth LE remote control (see "Bluetooth remote")
  commands.cpp       - the remote commands, the same for Bluetooth, /api/cmd and the page
  web.cpp            - web server: JSON API, live updates (the page itself is in web/)
  main.cpp           - WiFi, button, main loop
web/
  page.html          - the control page (HTML); style.css and app.js are its CSS and JS
content/
  frasi_dell_ora.txt - built-in quotes of "Frase dell'ora"
scripts/
  build_info.py      - firmware version (git commit, build time) for the page
  quotes.py          - content/frasi_dell_ora.txt -> include/quotes_builtin.h
  webpage.py         - web/ -> include/webpage.h (one page, CSS and JS inlined, gzipped)
  check_texts.py     - stops the build if a retired name comes back, if a texts.h message is
                       copied into src/ or if the page lists a setting's choices by itself
cardputer/           - Bluetooth remote for the M5Stack Cardputer ADV (its own PlatformIO project)
platformio.ini
```

Whether the lamp hangs horizontally or vertically is picked on the web page
(default: horizontal). The matching rotations are `ROTATION_HORIZONTAL`
(`270`) and `ROTATION_VERTICAL` (`0`) in `include/constants.h`, clockwise; if
the image comes out upside down in one orientation, change that value by
180. If it's mirrored (some panels get reassembled with the connector on a different
edge), flip `FLIP_HORIZONTAL` / `FLIP_VERTICAL` in the same file.

## WiFi control page

1. Copy `include/secrets.example.h` to `include/secrets.h` and put your WiFi
   name and password in it (`secrets.h` is git-ignored). The firmware
   doesn't build without it.
2. Build and flash. At startup the lamp scrolls its IP address once.
3. Open that address in a browser on the same network, or
   `http://obegransad.local`.

The lamp only uses your home network; it never opens a WiFi network of its
own. Until it manages to connect it scrolls `wifi...` and retries every
20 s; if WiFi drops later it reconnects by itself.

The page is laid out top to bottom as what you use most:

- **Sulla lampada** - a live preview (a 16x16 picture of what the panel
  shows, pushed by the lamp as it changes), with the button and speed of
  the mode on show. While you play a game the preview moves right above
  the pad, so you can play without looking at the lamp.
- **Modalità** - the modes as tiles in three groups (Informazioni, Giochi
  e creatività, Altro), after a *Playlist* tile that starts the playlist
  (lit while it runs; tapping a mode stops it).
- One card with the settings of the mode picked, titled with its name.
- **Impostazioni** (playlist, alarm, night, place, display),
  **Collegamenti** (phone notifications, Bluetooth) and **Sistema**
  (diagnostics, backup, firmware update): folding rows in three cards.

One set of components throughout (`web/style.css`): filled buttons for the
main action of a card (Salva, Mostra...), outlined ones for the others,
plain text for the destructive or minor ones; the longer explanations are
folded behind "Come funziona"; confirmations and errors appear as a toast
at the bottom of the screen; light and dark follow the phone.

The page stays up to date by itself over one live channel (Server-Sent
Events, `http://<lamp>:81/events`), in the same formats the Cardputer gets
over Bluetooth (`src/live.cpp`, described in `include/remote_protocol.h`):

- `frame` - the panel as seen, at most every 150 ms and only when it
  changes: 256 levels 0-15, two pixels per byte (Bluetooth: 128 bytes;
  here 256 hex digits);
- `now` - the short state (mode, button, game, demo, keys, brightness,
  time), the very JSON of the Bluetooth state: when it changes (checked
  every 250 ms) the page fetches the whole state at once, so a mode picked
  on another phone or by the playlist shows up immediately;
- `state` - all of `/api/state`, when it changes (checked every 2 s);
- `board` - the Game of Life's drawing board, whenever someone draws.

Game keys go to the same port (`GET :81/input?k=L`) on a kept-alive
connection, so a held arrow doesn't open a new connection ten times a
second. Port 81 never blocks: what a connection can't take yet waits in its
own buffer (and meanwhile only the latest frame is sent), and a page that
stops reading for 5 s - a phone going to sleep mid-game - is dropped
instead of freezing the panel. If port 81 can't be reached the page falls
back to polling (`/api/frame` five times a second, `/api/state` every 15 s,
`/api/canvas` every second while drawing) and `POST /api/input` for keys.

If loop() ever gets stuck for 20 s a watchdog restarts the lamp (the
diagnostics then say "watchdog" as the last restart's reason).

Below it are the modes; the one on the panel is highlighted,
with its own command button ("next quote", "jump", ...; the space bar
works too) and a **speed** slider (1-9, per mode). Below that only the
settings of the mode being shown appear (the text, the quotes list, the
animation menu, ...). General settings are in collapsible sections:

- **Playlist** - scenes of the catalog (see *One catalog of scenes*
  below) shown in turn, each for the minutes you choose: a mode, a single
  animation (*Volo sulle colline* 10 min), a game (as a demo) or one of your
  drawings; what the playlist picks doesn't change the animation, game or
  drawing chosen in their own modes (by default clock 10 min, quote 3 min,
  animations 5 min, games 5 min). With **Cambia per
  fascia oraria** up to four time slots ("scene") each have their own
  start time, brightness and list - e.g. mornings clock and forecast,
  evenings quotes and animations, late at night just a dim clock; a slot
  lasts until the next one (the last carries on past midnight) and
  starts its list from the top. The night schedule and the alarm still
  win. Picking something by hand stops the playlist.
- **Sveglia con l'alba** - on the chosen days, from 5-60 minutes before the
  alarm a sun rises on the panel while the brightness slowly goes up; it
  stays bright for a while after. It wins over everything else; the mode
  button (or the page) stops it, and "Prova" shows a one-minute sunrise.
  *Dopo la sveglia* picks the scene shown when it is over (e.g. the clock
  with the weather, or *Finestra sul cielo*), as if chosen on the page;
  by default what was on comes back (setting `alarmScene`).
- **Giorno e notte** - between two times (e.g. 23:00-07:00), or from sunset
  to sunrise, the lamp is off, shows only stars, or keeps going at a lower
  brightness. The night wins over the playlist and over the mode picked by
  hand.
- **Luogo e ora** - today's sunrise, sunset and moon phase; search a city by name (the browser asks Open-Meteo's
  free geocoding service and sends the lamp just the coordinates) and pick
  the time zone; picking a city also picks its time zone when it's in the
  list.
- **Display** - horizontal/vertical, brightness, the font of all text,
  scrolling or in still pages (text, quotes, web info, notifications,
  word clock). The choices, by height: *Grande* (16 px: the small font
  doubled with the EPX/Scale2x algorithm, which keeps diagonals smooth,
  filling the whole panel for reading from across the room; pages would
  hold two letters, so with it the text always scrolls), *Media* (8 px:
  font A, proportional, lowercase; the default; 2 lines a page - and, when
  the lamp hangs vertically, a 6-px lowercase version so the text is
  shorter), *Piccola* (5 px, capitals only; 2 lines a page) or *Minima*
  (4 px capitals; 3 lines a page). Also how the lamp changes picture -
  one rule for everything (`beginTransition()` / `beginPageTransition()`
  in `display.h`): a new scene (mode, animation, game, a drawing of the
  gallery) comes in with *Dissolvenza* (cross-fade, 0.6 s, the default),
  *Tendina da sinistra* (the new image sweeps in, 0.5 s), *Pixel sparsi*
  (pixel by pixel in a scattered order, 0.7 s) or *Stacco netto*; a new
  page within a scene (pages of a text, cards of Mondo, days of
  Previsioni, the egg hatching) always cross-fades, unless it is
  *Stacco netto*.

- **Notifiche dal telefono** - anything that can open a web address can
  send the lamp a notification: `http://<lamp>/api/notify?text=Lavatrice%20finita&icon=check`
  (GET, form POST, or POST JSON `{"text":"...","icon":"..."}`; text up to
  200 characters, accents welcome; icons `bell`, `mail`, `check`, `alert`,
  `heart`, `phone`, `home`, `star`, or none - or an animation of the
  catalog, `icon=a/fireworks` (also `scene=`), played for 6 seconds before
  the text, e.g. fireworks for a goal). An icon drops in and moves
  (the bell swings, the phone shakes, the heart beats, the others glow),
  then the text shows as still pages, then the lamp goes back to what it
  was doing and carries on from where it was (a game or the sand timer
  isn't restarted). Up to 4 wait in a queue; only the sunrise alarm wins
  over them, and at night they're ignored unless *Anche di notte* is on.
  The section has a test form and the address to copy: on an iPhone, an
  automation in the Shortcuts app ("when an email from ... arrives", "when
  I leave home", a time) with the action *Get contents of URL*; on Android
  HTTP Shortcuts, Tasker or MacroDroid. Cloud services such as IFTTT call
  from the Internet, so they need a port forward or a tunnel to the lamp.
- **Diagnostica** - uptime and why the lamp last restarted, free memory,
  chip temperature, firmware; WiFi signal and address; the last weather,
  Wikipedia and calendar fetches; the pages connected live; Bluetooth
  (on, remote connected); whether the firmware is new and still on trial
  (see "Updating over WiFi"); the recent events (starts and why, updates,
  rollbacks, restored settings, WiFi lost for over half a minute and back:
  the last 30, kept in flash); how many
  rounds the main loop makes a second and the longest one (over a few
  hundred ms the panel stood still that long); and how steady the grayscale refresh is:
  plane changes done and missed, average and worst delay after the timer
  tick (`GET /api/diag`; refreshed every 2 s while the section is open).

Everything is saved in flash, so the lamp comes back in the same state
after a power cut.

No push button is needed: everything is on the page. If you do wire one to
GPIO4 (other leg to GND), each press switches to the next mode.

Clock, weather, the hourly quote and the night schedule need your network
to have internet access (for the time and the weather).

The page has no password: anyone on the same network can use it.

Current modes:

- **Testo scorrevole** - scrolls the text set on the page, at the height
  chosen there: top, middle, bottom or variable (a different height at
  every pass, the default) - or shows it as still pages like the hourly
  quote, in the font chosen there (the same setting as
  in Display). With *Media*, this and all other scrolling text (web info, word clock, game scores) use its
  compact variant (`include/font_compact.h`): letters one pixel narrower -
  3 instead of 4 - where the shape allows it, so more text fits; the
  digits are already 3 pixels wide, the narrowest that stays readable
- **Frase dell'ora** - a different quote every hour, shown as still pages
  in the font chosen in Display - 2 lines a page with Media or Piccola, 3
  with Minima, scrolling with Grande - (words kept whole where they fit,
  split by Italian syllables where they don't - CO / ME, CON / SAPE / VO /
  LEZ / ZA -,
  2.5 s a page, set by the speed slider; the same as the Demo mode's "A
  pagine"), over and over until the hour changes.
  The built-in list is 100 motivational quotes from
  `content/frasi_dell_ora.txt` (one per line, UTF-8; `scripts/quotes.py`
  turns it into a header at every build, so edit the text file). On the
  page you can add your own (up to 16000 characters, saved in flash as
  `/quotes.txt`): they join the 100 in the rotation, and "Cancella le mie
  frasi" removes them. Button: next quote
- **Orologio** - one of four faces (*Quadrante* on the page, setting
  `clockStyle`): binary (one column of bits per digit of HH:MM, a bar
  filling with the seconds), in Italian words ("sono le tre e un quarto",
  "è l'una meno cinque"...), in English words ("it's quarter past three"),
  or - the default - clock and weather on one screen, numbers in the text font: on top the
  temperature with a
  one-pixel degree sign and an animated weather icon (falling rain or snow,
  flashing lightning, drifting clouds, ...), below the time (hours without a
  leading zero, then the minutes), and a dot gliding round the border for the
  seconds (it moves continuously, its light shared between neighbouring
  pixels, with a short fading trail). Weather is
  refreshed every 15 min; until the first reading arrives the clock shows
  just the time, centred, and until the time is known the waiting dots
  (see below). Button: refresh weather
- **Previsioni** - the daily forecast for today and the next 3 days, one
  screen per day, 5 s each: the weekday in two letters and the date ("VE
  26", still - "VEN 26" wouldn't fit in 16 pixels) along the top; below, the day's weather icon and its minimum over its maximum, each
  with a degree dot set one column apart, as in the clock. The page shows
  the same four days (icon, minimum, maximum, chance of rain) and a chart
  of the next 12 hours. The clock
  also shows an umbrella next to the weather icon when rain is likely
  (>= 60%) within 2 hours
- **Dal web** - in turn: the word of the day (built-in list), an "on this
  day" event from Italian Wikipedia, and the next event of your calendar
  (paste its secret iCal link, e.g. from Google Calendar; recurring events
  aren't supported). Choose the sources and the height on the page (or
  still pages, like the hourly quote). Until
  the first data arrives it shows the waiting dots
- **Mondo** - news from the world in pixels (`src/modes/world_mode.cpp`,
  data from `src/world.cpp`): two cards taking turns with a cross-fade
  (`Display::beginFade()`). The air quality where the lamp is - "ARIA" at
  the top, the European index in the middle (the 3x6 digits of the text
  fonts, brighter than the words), its band at
  the bottom ("BUONA", "MODERATA", ...; scrolling when it's long;
  Open-Meteo, every 30 minutes) - for 8 seconds; then the International
  Space Station on a 16x8 world map with its trail and you as a steady dot
  (with the lamp vertical, "ISS" above the map; wheretheiss.at, every 20
  seconds) for 10. Downloads run only while the mode is shown and for 15
  minutes after; button: the other card
- **Gioco della vita** - Conway's Game of Life with wrap-around edges, 4
  generations a second. Each game starts from an empty board with a small
  pattern in the middle (R-pentomino, acorn, diehard, ...) that grows for
  40-150 generations; when the board dies, freezes or loops a new game
  starts; button: restart. It has a drawing board (`src/modes/board.cpp`)
  for your own first generation, shared by everyone: on the page you
  paint with the finger or the mouse (pencil or eraser); strokes go to the
  lamp in batches every 60 ms (`POST /api/paint`, `p=x,y,level;...`) and
  every open page fetches the board each second (`GET /api/canvas`), so
  two phones draw together. The Cardputer draws with a blinking cursor
  (arrows, space lights or clears a cell). As soon as someone draws, the
  lamp shows the board; *Fai vivere* (then the mode's button) sets it
  going as generation zero; *Salva* stores it among the Disegni. The
  drawing survives restarts (NVS blob `canvas`, in the settings backup)
- **Animazioni** - one animation, or "automatic" (a different one every 5
  minutes); button: next animation. Every animation runs on its own clock,
  which the speed slider runs from x0.25 to x4, while frames keep coming
  at the animation's own rate: slow is a smooth slow motion, not fewer
  frames, and a busy lamp doesn't change the pace (the ones that move in
  steps - particles, fire, blinking icons - take as many steps as the
  clock has gone through). The animations (the games have their
  own mode, below), in `src/animations/`:
  - *Atmosfere*: digital rain, fire, stars, waves, and *Finestra sul
    cielo* (`src/animations/sky.cpp`): the sky outside right now, seen
    through a window (its frame and sill drawn) looking south over the
    roofs - the sun with its rays and the moon with its phase where they
    really are, the light of the hour (a dim day sky, the glow of sunrise
    and sunset on the sun's side, stars and a few lit windows at night),
    puffy clouds drifting, as many as the current weather says, and its
    rain, snow, fog or lightning (a bolt from the clouds to the roofs), and
    *Acquario* (`src/animations/aquarium.cpp`): fish swimming as a school
    (apart from each other, along with their neighbours, towards the
    group, turning before the glass), weeds swaying, bubbles, sand; the
    page shows *Dai da mangiare* while it plays (or key A, also from the
    Cardputer): a few flakes sink and the fish dart for them. Any
    animation can offer such a button (`Animation::pokeName()` / `poke()`).
    Then the night scenes (`src/animations/nightscapes.cpp`): *Faro* (a
    lighthouse on a cliff, its beam sweeping through the mist and flashing
    when it faces you, waves breaking below), *Lucciole* (fireflies over
    the grass, each blinking at its own pace and falling into step with
    the others, until they scatter again), *Aurora boreale* (curtains of
    light waving over the mountains, shimmering in the lake), *Pioggia sul
    vetro* (drops growing on the pane and sliding down in fits and starts,
    leaving trails, city lights blurred behind), *Falò* (flames licking up
    from glowing logs, sparks rising), *Stelle cadenti* (the Milky Way and
    now and then a shooting star - a shower of them during the Perseids,
    9-14 August), *Stelle circolari* (the sky turning round the pole star,
    the stars drawing arcs as in a long exposure), *Treno nella notte* (a
    train with lit windows crossing the countryside under the moon) and
    *Respiro* (one wave of light breathing in for 4 s and out for 6 s)
  - *Icone geek*: *Alieno* (a walking Space Invader), Pac-Man chased by a ghost, a
    terminal (four lines in a 3x3 font) typing commands whose answers are
    the lamp's own - `ls` its files, `w` the time and uptime, `ip` its
    address, `df` free flash, `top` free memory and chip temperature,
    `pwd`, `cal` today's date -, a rocket among the stars, a cup of coffee
    with steam, a 3.5" floppy (the shutter slides, lines get written on
    the label), a Game Boy with a Tetris piece falling on its screen,
    Matrix rain, the hacker emblem (a glider going through its four
    generations in a 3x3 grid), and five more drawn after a sheet of
    pixel-art icons: *Teschio* (it chatters its teeth, its eyes glow),
    *Casetta* (smoke from the chimney, windows lighting up), *Hamburger*
    (the top bun hops, sesame seeds glint), *Uccellino* (it hops along
    the ground and pecks, blinking) and *Dado* (it
    rolls, faces flashing by slower and slower, then settles); their
    pictures are in the sprite atlas (`geek.*`), retouchable from the page
  - *Ricorrenze* (`src/animations/seasonal.cpp`): *Neve* (a snowy
    night: a crescent moon and stars, a far white ridge, a pine and a
    cottage with snow on the roof, its window flickering like a fire and
    smoke bent by the wind; the snow falls in three depths - far flakes
    small, dim and slow, near ones bright and quick - blown by gusts, and
    settles in drifts that grow and slowly sink back), *Albero di Natale*
    (on a snowy night: shaded tiers with snow on the branch tips, a
    garland whose lights a wave runs down, each glowing on the needles
    around it, the star pulsing and now and then sparkling, presents
    underneath, a few flakes), fireworks, floating hearts, *Zucca di
    Halloween* (a full moon and stars, a bat flapping across now and
    then, the pumpkin round with its ribs, its carved face lit by a candle
    that flickers and gutters), a birthday cake whose greeting scrolls by.
    They are offered only around their days - in the lists, the automatic
    choice, the catalog and the playlist (which skips them otherwise):
    snow December - February, the tree 1 December - 6 January, fireworks
    26 December - 6 January, hearts 1 - 15 February, the pumpkin 15
    October - 2 November, the cake on a birthday (`inSeason()` in
    `occasions.cpp`; `v a/pumpkin` in July answers "Fuori stagione").
    With **Ricorrenze** on (Impostazioni), on a special day the lamp shows
    its animation for the first minute of every hour, then goes back to
    what it was doing (not at night, not during a game): fireworks for New
    Year (from 22:00 on 31 December), the tree on 24-26 December, snow the
    rest of December, hearts on Valentine's day, the pumpkin on Halloween, the
    cake on a birthday of the calendar ("Dal web": an event today whose
    title says "compleanno" or "birthday", matched by day and month so
    Google's yearly birthdays count) - `src/occasions.cpp`
  - *3D e demo*: rotating wireframe cube, plasma, metaballs, endless zoom
    into the Mandelbrot set; and, rendered per pixel
    (`src/animations/demos3d.cpp`): *Cubo solido* (16 rays per pixel
    against the spinning cube, faces shaded by the light, 50 frames a
    second, the spin gently speeding up and slowing down) and *Tunnel* (a
    twisting tiled pipe you fly down); and raymarched
    (`src/animations/raymarch.cpp`: scenes as signed distance functions,
    one ray per pixel corner shared by the four pixels around it, normals,
    soft shadows): *Metaball 3D* (three spheres melting into each other as they
    orbit, with a glow along their edges); and Saturn, drawn flat so it
    reads at 16x16 (`src/animations/saturn.cpp`): *Saturno* (the classic
    picture, the rings rocking slowly, their shadow across the planet's
    bands), *Saturno e la luna* (half lit by the sun, a moon going round
    it, behind the planet and then in front) and *Saturno da vicino* (a
    close-up, the planet in a corner, its grooved rings across the sky)
  - *3D e demo*, the flights (`src/animations/flights.cpp`): voxel
    landscapes, Comanche style - one ray per column walked front to back
    over a heightmap, with sub-pixel ridges, fog in the distance and a
    camera that eases over the ground ahead. One engine (`Flight`), three
    worlds that only say what the ground is and how it is lit:
    - *Volo sulle colline*: rolling hills, brighter with height;
    - *Città di notte*: down an avenue between blocks of buildings, lit
      windows that now and then go on or off, blinking beacons on the
      towers; now and then the flight climbs over the roofs;
    - *Sopra le nuvole*: ridges with snow above the snow line and a
      drifting cloud layer the flight sometimes sinks through (all goes
      white) and comes out of.
- **Giochi** - one game, or "automatic" (a different one every 5
  minutes, always as a demo); button: next game. Every game derives from
  `ArcadeGame` (`src/animations/arcade_game.h`), which gives them the
  group, the demo switch and the same ending: "Punti N" scrolls by, then a
  new game. The games, also in `src/animations/`: **Super Mario** - side-scrolling platformer (Mario is a
  5x7 sprite in grayscale: cap, face and moustache, overalls, shoes)
  with pipes, pits, goombas (stomp them) and coins; in demo mode an
  autopilot simulates the next moves and jumps at the best moment,
  otherwise you jump. At game over it shows the score and starts again
  (it used to be a mode of its own: a saved choice moves here by
  itself); **Tetris** - a well 10 columns wide, 14 (the whole panel) with the lamp vertical; in demo mode, for each piece the computer tries
  every rotation and column and picks the best by stack height, holes and
  surface (points: the lines cleared); **Snake** - in demo mode it takes the shortest way to the food
  only if it can still reach its tail afterwards (after a long hunger it
  risks it anyway; points: the food eaten); **Tron** - two light cycles leaving a wall behind them, first
  to 3 rounds; player 1 drives from the page, player 2 from a second phone
  set to *Giocatore 2* or from the Cardputer (*P* switches player; the
  keys of player 2 are the lower-case ones, `k l`); a cycle nobody drives
  is driven by the lamp, which steers into the most room (flood fill),
  keeps straight when it can and dodges head-on crashes - in demo mode
  both are; **Pong** - you against
  the computer, first to 5; **Breakout** - 3 lives, faster at each level;
  **Flappy Bird**; **Space Invaders** - waves that get faster; **Labirinto 3D** - a first-person
  maze drawn by raycasting (one ray per column, walls shaded by
  distance, a faint floor): find the pulsing block at the far end. The
  map is shown at the start; in demo mode the computer keeps its right
  hand on the wall, which always finds the exit; **Dino** - the
  runner of Chrome's offline page: cacti and pterodactyls (low: jump,
  middle: duck, high: run under) faster and faster; in demo mode the
  computer simulates running, jumping and ducking and picks the first
  that keeps it alive; **Donkey Kong** - four floors joined by ladders,
  Mario climbing up to Pauline in three levels taken in turn (one follows
  the other straight away, no title screen): *1* Kong throws barrels that roll down in a zigzag (and
  sometimes down a ladder); *2* no barrels, but fires - the first out of
  the oil drum at the bottom, the others on the middle floors - wander the
  floors, climb the ladders and drift towards Mario; *3* barrels and fires
  together, new ladders. The characters are small gray-level sprites:
  Mario 2x3 (cap, face, legs walking, facing where he goes), Kong 5x3
  (arms up when he throws), Pauline 2x3, with a heart over her at the
  rescue. 3 lives, faster each time round the three levels; in demo mode
  Mario jumps what comes at him, waits on the ladder while something
  passes the top and backs off from what comes down it;
  **Doom** - a first-person shooter drawn like Labirinto 3D (walls
  shaded by distance): imps standing in the level, hidden behind walls
  and bigger as they come, throw fireballs you can see coming; a gun at
  the bottom of the view and the health bar on the bottom row. Kill them
  all for the next level (more imps, some health back); the score is 100
  per imp and 500 per level. In demo mode the computer turns to the
  nearest imp in sight and shoots, or walks the shortest way to one;
  **Sonic** - he runs faster and faster over rolling Green Hill ground
  (the checkerboard soil), collecting rings (counted along the top row);
  a jump curls him into a spinning ball that smashes the motobugs, springs
  throw him up to rings high in the sky. Spikes, or an enemy hit while not
  in a ball, cost all the rings (blinking for a moment); with no rings, or
  in a pit, a life. 3 lives; the score is distance, rings and enemies. In
  demo mode the computer simulates running on and jumping at every moment
  ahead, and jumps at the first moment that clears everything
- **Formule** - the panel drawn by a formula typed on the page, like
  tixy.land: an expression of `t` (seconds), `i` (pixel 0-255), `x`, `y`
  (0-15), evaluated for every pixel 30 times a second; 1 or more is full
  brightness, 0 off, negatives faint. JavaScript syntax (arithmetic, `**`,
  comparisons, `&& || !`, bitwise, `?:`, `PI`, the `Math` functions) is
  compiled once into a small stack program (`src/formula.cpp`), so a new
  formula runs at once without new firmware (`POST /api/formula` with
  `f=`; a formula that doesn't parse is refused with the reason and the
  position, e.g. "manca «)» (posizione 12)"). The page has a dozen
  examples to start from (waves, sonar, a bouncing ball, a beating heart...)
- **Disegni** - your drawings and animations, one or all in turn; the
  gallery starts with a few examples (a beating heart, the Super Mario
  mushroom, a cat, a flower, Pac-Man, a space invader). On the page there
  is a 16x16 pixel editor, which opens on the drawing the lamp is showing (6 brightness levels, fill, invert,
  up to 32 frames, 2-15 frames a second) that shows the drawing live on
  the lamp while you draw; you can also import a photo or an animated GIF
  (cropped to a square, turned into grayscale, contrast stretched). Up to
  60 drawings are saved in flash
- **Clessidra** - a sand timer (1 minute to 1 hour, set on the page;
  *Ricomincia* starts it again). A glass of two round bulbs between
  wooden caps, with a glint on the upper left; 44 grains, one LED each,
  with real falling-sand physics: they pile up in a cone, slide down the
  slopes (rolling off where the glass curves in), open a crater in the top
  bulb; the neck lets one grain through at a time,
  at the pace that empties the top in the time chosen. *Gira* turns it
  over like a real one (the time left becomes the time that had run); at
  the end the sand pulses for a few seconds. The page shows the time left
  (the time is the setting `hgMin`; `POST /api/hourglass` starts it over)
- **Animaletto** - a Tamagotchi-style pet (`src/modes/pet_mode.cpp`). It
  hatches from an egg after 5 minutes into a frog (*ragazzo*), drawn as a
  fuzzy felt plush (a dimmer outline and a faint shimmering fur around
  it), its big eyes on two bumps. It gets
  hungry and bored, sleeps from 22 to 7 (a "z" floats up; it naps when
  exhausted), leaves droppings and, after about two hours of hunger, dirt
  or sadness, falls ill (it is drawn dimmer; it never dies). Care comes as
  keys - L *Pappa* (an apple falls and is eaten), R *Gioca* (a bouncing
  ball), U *Pulisci* (a broom line sweeps the droppings), D *Medicina*,
  A *Coccole* (a heart) - from the page's buttons, the keyboard arrows and
  space, or the Cardputer; the mode's button feeds it. Its needs blink in
  the corner (apple, note, cross). Time runs while other modes are shown
  and, with the clock set, while the lamp is off: it catches up on up to
  three days. The page shows name, age, mood and three bars, and can rename
  it or start a new egg (`POST /api/pet` with `name` or `reset=1`); the
  state is the NVS blob `pet`, so it is in the settings backup.
- **Bonsai**, **Gatto**, **Draghetto** - three more creatures on one base
  (`src/modes/creature.cpp`: a life simulated a minute at a time, also
  while other modes are shown or the lamp is off, up to three days of
  catch-up; the state an NVS blob named after the mode, so in the backup;
  care as keys from the page, the arrows or the Cardputer; the page shows
  title, mood and bars, renames and starts over with `POST /api/creature`
  `id=` and `name=` or `reset=1`). None of them dies.
  - *Bonsai* (`bonsai_mode.cpp`): a little tree in a pot on the balcony.
    It drinks about half the pot a day, more in the heat, and the real
    rain (the weather) waters it; it grows every day it is well - full
    size in about two weeks -, leans its lit side towards the sun (left in
    the morning, right in the afternoon), blossoms in spring, drops leaves
    in autumn, rests in winter. Stray shoots grow and want pruning; thirst
    wilts it, too much water rots it; fertiliser doubles its growth for a
    day but burns the roots more than once in three days. Keys: L
    *Annaffia* (a watering can pours), R *Pota* (the shoots fall), U
    *Concima* (sparkles from the soil).
  - *Gatto* (`cat_mode.cpp`, sprites `cat.*`): it strolls, sits and licks
    its paw, now and then sits right in front of the clock hiding a digit,
    sleeps curled up at night (22-7 or the lamp's night mode), and after a
    notification wakes up and bats it about. Hungry, it shows a bowl in the
    corner; neglected, it sulks with its back to you, its tail flicking,
    and ignores laser and yarn until stroked. Keys: L *Pappa* (a bowl), R
    *Laser* (it runs after the dot), U *Gomitolo* (it bats the ball back),
    A *Carezza* (it purrs, a heart rises).
  - *Draghetto* (`dragon_mode.cpp`, sprites `dragon.*`): an egg, a cub for
    its first day, a youngster until the third, then grown into what it
    was trained for - fire dragon (spikes; it puffs flames when stroked),
    sky dragon (great wings; it hovers), wise dragon (long horns, beard) -,
    or a round lazy dragon if hardly trained; training fades a point a day,
    so a grown dragon can still change. Three mini-games: R *Riflessi* (six
    arrows, each shown a little shorter: press it in time - agility), U
    *Memoria* (repeat the arrows it shows, one more each round - mind), D
    *Forza* (press as fast as you can for 4 s, the flame grows - strength).
    L *Pappa*, A *Coccole*.
- **Demo** (under Diagnostica, *Prova i font*: a tool, not among the
  modes) - the hourly quotes shown three ways, to compare how a long
  text reads on 16x16 LEDs (a whole quote never fits one screen: they
  average 79 characters, a screen holds 12-16): *3 righe* - split into
  three lines of about the same length that scroll together, in the
  4-row Tiny font (`include/font_tiny.h`, capitals, the smallest that
  stays readable: 4+1+4+1+4 rows), a third of the scrolling; *A pagine* -
  still screens of three lines, words kept whole where they fit, one
  every 2.5 s; *2 righe* - two lines scrolling together in the Mini font.
  "A turno" changes style at every quote; button: next quote; the speed
  slider sets the scrolling and the page time
- **Spento** - all LEDs off

Everything the lamp draws follows one set of rules (`include/ui.h`): three
sizes of text - a *label* (the 4-row capitals, centred, scrolling round
when too wide), a *value* (the 6-row digits of the text font, centred) and
running text in the font picked in Display; one *card* for a reading
(label, value, caption: Mondo's air quality); pictures as rows of
characters drawn by one `ui::icon()` (pet, icons, special days); four
brightness steps (full, text, dim, faint); and the same sign while data is
missing - three dots filling in, or a blinking WiFi symbol when the lamp
is offline, always on row 10. The clock and Previsioni keep their own
tuned digits, narrower where two numbers share the panel.

### Games and demo mode

Most games follow **Grafica dei giochi** in the Display section:
*Sfumata* (the default) uses shades of gray - fading snake, dimmer
settled Tetris blocks, Mario's shaded sprite, the maze's distance-shaded
walls - and *Nitida* draws every LED fully on or off (the maze then uses a
fixed dot pattern for depth, Mario becomes a lit silhouette). Pong,
Breakout, Flappy Bird and Space Invaders are always sharp; Doom always
uses shades of gray for its walls. In-between brightness is made by switching LEDs on and off
very fast; if it trembles on your lamp, pick Nitida.

Each game declares its style (`style()`: `Selectable`, `Crisp` or
`Shaded`) and its controls (`controls()`: the keys it uses out of L R U D A,
the pad's labels, whether held arrows repeat, the hint under the pad). The
firmware sends them along - the style in the page's list of games (shown in
the game box and under Grafica dei giochi), the controls with the game on
show (`state.game.pad`) and over Bluetooth (`"c"`, `"ca"`) - so the page
and the Cardputer keep no list of their own.

All the games have a **Modalità demo** checkbox (on by
default), shown on the page while the game is on the panel:

- **on** - the game plays by itself and ignores input;
- **off** - you play, with the on-screen pad or the keyboard: Mario jumps
  with *Salta*, space or up (a press just before landing still counts);
  Tetris moves with left/right, rotates with up, drops with down or space;
  Snake uses the arrows; Pong up/down; Breakout left/right; Flappy
  Bird flies with *Vola*, space or up; Space Invaders moves with left/right
  and shoots with *Spara* or space; Dino jumps with up, space or *Salta*
  and ducks with down (hold it); in Donkey Kong left/right walk, up/down
  climb the ladders and *Salta* (space) jumps; in Doom up/down walk, left/right turn and *Spara* (space) shoots; in Sonic right runs (hold it), left brakes and *Salta* (space) jumps; in Labirinto 3D up/down walk a step,
  left/right turn and *Mappa* (or space) shows the map. In the paddle games holding an arrow
  down keeps moving.

Games shown by the "automatic" choice of the Giochi mode
always run as demos. Controls go to the lamp over WiFi, so expect a small
delay; lower the game's speed if it's too hard.

To add a mode, implement the `Mode` class from `include/modes.h` in
`src/modes/` and add it to `MODES` in `src/modes.cpp`; to add an animation,
implement `Animation` from `include/animation.h` in `src/animations/` and
list it in `src/animations/animations.cpp`. Both show up on the page
automatically. An animation draws from the `now` it is given and nothing
else (no frame counters); if it moves in steps, it says `fixedStep()`.
Shaded scenes go through `ui::tone()`, so their dark shades survive the
panel (its dimmest step is already a fifth of full light).

### One catalog of scenes

Everything the lamp can show is a *scene* with one id (`include/catalog.h`,
`src/catalog.cpp`): a mode by its id (`clock`, `pet`, `world`...), an
animation as `a/<id>` (`a/voxel`, `a/aquarium`), a game as `g/<id>`
(`g/doom`), a drawing of the gallery as `d/<id>`. The same id is used
everywhere:

- the playlist and its time slots (`settings.playlist`, `scene:minutes,...`;
  an animation, game or drawing picked there is shown through the mode's
  `setPick()`, without touching what was chosen in that mode);
- the alarm (`alarmScene`: what comes on when the sunrise is over);
- notifications (`icon=a/fireworks`: an animation before the text);
- the API: `GET /api/catalog` lists every scene `{id, name, group, kind}`,
  `POST /api/show` with `scene=a/voxel` shows one, and `/api/state` says
  which is on (`"scene"`);
- Bluetooth and the Cardputer: the command `v <scene>` and the catalog
  characteristic (its *Giochi*, *Modalita'*, *Animazioni* and *Disegni*
  menus all send `v`);
- the page: every list where you pick something to show (playlist, time
  slots, after the alarm, the notification's animation) is the catalog,
  grouped as Modalità, the animation groups, Giochi, Disegni.

### Pictures: one format, one atlas

Every picture the lamp draws - notification icons, the pet, weather icons,
the geek icons, the seasonal pictures, Mario, the dino, Sonic, Doom's imp,
the world map - is a sprite in one format (`include/sprite.h`) and lives
in one atlas (`src/sprite_atlas.cpp`, 56 sprites). A sprite is rows of
characters, frames one after the other: `.` transparent, `0`-`9` shades
(`0` an opaque black), `#` the level it is drawn at, `:` a third of it, `+`
always full, letters are marks whose meaning is up to the code (the frog's
eyes and mouth, Mario's cap, overalls and shoes). `sprites::draw()` draws a
frame, `sprites::frameAt()` picks it by time, `sprites::shade()` reads a
pixel (for collisions).

On the page, *Sistema > Immagini della lampada* lists the atlas, shows
each sprite large, lets you repaint it pixel by pixel (transparent, black,
four greys, always-full), try it on the lamp and save it: the lamp uses
the retouched version at once (`GET /api/sprites`, `POST /api/sprite`
with `name` and `rows`, or `reset=1` for the original; kept in LittleFS
under `/sprites/`, not in the settings backup).

### Scrolling text

The text from the page scrolls on one line, at the height picked on the
page (variable by default).
`MESSAGE` in `constants.h` is only the default used on first boot.

(The display code can also scroll two lines stacked on top of each other,
split by a `|`.)

The text is case sensitive: the font has lowercase and capital letters
(capitals are one pixel taller), digits and a few punctuation marks.
Digits have the same soft, rounded shapes ("Morbido") everywhere - text
font, mini font, big digits, the clock's 2-pixel tens - with a 1 without
a foot; in fixed layouts each digit sits right-aligned in its slot so the
narrower 1 doesn't shift the others.
Accented letters are written with an apostrophe, as when typing without
accents - "perché" scrolls as `perche'`, "È" as `E'` - because a
one-pixel accent is lost on the panel; this goes for every font and all
text (scrolling text, quotes, web info, ...). Anything else is shown as a
space. To
add characters, add entries to `FONT_GLYPHS` in `include/font_small.h`
(width in pixels + 8 rows, bit 7 = leftmost column).

## Bluetooth remote (Cardputer ADV)

The lamp is also a Bluetooth LE device (`src/ble.cpp`), so it can be driven
without WiFi from a remote that pairs with it once. `cardputer/` is a
firmware for the **M5Stack Cardputer ADV** that does it: a menu to pick
games, modes and animations, a game controller with the lamp's picture on
its screen, a text editor to send scrolling text and notifications, and
brightness.

**On the lamp** the **Bluetooth (telecomando)** section of the page shows
the name (`obegransad`) and a 6-digit PIN, made at random on first boot;
it can switch Bluetooth off and "forget the remotes", which also picks a
new PIN (both restart the lamp). Pairing uses the PIN as a static passkey
with bonding and MITM protection, and every characteristic needs that
encrypted, authenticated link: a device that doesn't know the PIN can't
read or send anything. Up to 3 remotes stay paired. The BLE stack runs on
core 0 next to WiFi, like the network, away from the LED refresh.

**On the Cardputer**:

    cd cardputer
    pio run -t upload        # hold G0 while plugging in if it doesn't start

The first time it asks for the PIN (type it, Enter), then finds the lamp,
pairs and remembers it. Keys: `;` `.` `,` `/` are the arrows (as printed on
the keycaps), Enter chooses, `` ` `` goes back.

- **Telecomando** - the lamp's panel in large on the left (sent by
  the lamp as it changes, at most every 150 ms); the arrows play (held
  down they repeat), Space or Enter is the main button (jump, shoot,
  drop), `D` hands control to you (in "Automatica" it first pins the game
  showing now, so `D` always gets you out of the demo; press it again to
  put the demo back), `X` is the mode's button (next game); for a mode
  that isn't a game, the line the lamp gives about it ("Pixel · Ha fame",
  "Aria 27 discreta · ISS a 4210 km", "Generazione 52"...) - the same line
  the page shows under its preview - and its keys as the mode names them
- **Giochi**, **Modalita'**, **Animazioni** - the lamp's own lists (read
  from it, so new games show up by themselves); picking a game goes
  straight to the controller
- **Scrivi un testo** - type it, Enter: the lamp switches to Testo
  scorrevole with it
- **Notifica** - `fn` + `,` `/` pick the icon, type the text, Enter: shown
  like a notification from `/api/notify`
- **Luminosita'** - `,` `/` in steps of 16
- **Impostazioni lampada** - the lamp's settings as its page offers them
  (font, text position, brightness and sun, orientation, transition, clock
  face, game graphics, special days, playlist, time slots, night, alarm,
  ...): the list, with names and choices, comes from the lamp (the
  `settings` characteristic, built from `SETTING_DEFS`), so a setting added
  to the lamp appears here without touching the Cardputer; `,` `/` change
  the selected one (`o <name> <value>`, checked by the lamp like the page's)
- **Abbinamento (PIN)** - `F` forgets the lamp (after changing its PIN)

The screen and keyboard run on their own task pinned to core 1, redrawing
a single full-screen off-screen canvas (in internal SRAM, this board has no
PSRAM) at ~30 fps and pushing it in one SPI burst; the Bluetooth work,
whose scan blocks for a few seconds at a time, runs on a separate task on
core 0 and reaches the UI only through thread-safe snapshots and a command
queue. That split is what keeps the screen from flickering or freezing
while it searches or reconnects.

**The protocol**, for other remotes, is defined once in
`include/remote_protocol.h` (the Cardputer firmware includes the same
file) and the commands are carried out by `src/commands.cpp` for every
source: Bluetooth, the page's buttons and `POST /api/cmd` with `c=<command>`
(e.g. `curl -d "c=g sonic" http://obegransad.local/api/cmd`). Service
`8f3e0000-5c1a-4a6b-9b8e-0b5e6a1d0bea` with four characteristics
(`...0001` to `...0004`):

| Characteristic | | |
|---|---|---|
| command `…0001` | write | one command per write: `k L` (key L/R/U/D/A), `v a/voxel` (any scene of the catalog: `clock`, `a/voxel`, `g/doom`, `d/<id>`), `m clock` (mode), `g doom` / `a rain` (game / animation, or `auto`), `d 0` / `d 1` (demo), `x` (mode button), `n` (next mode), `b 128` (brightness), `t text` (show a text), `p bell\|text` (notification), `s 5` (speed 1-9), `w 3 4 255` (paint a cell of the Game of Life's board), `w c` (clear it), `w l` (set it going) |
| state `…0002` | read, notify | `{"m":"games","mn":"Giochi","x":"Prossimo gioco","g":"doom","gn":"Doom","d":0,"f":0,"c":"LRUDA","ca":"Spara","cl":"↶|↷|||Spara","n":1,"b":200,"t":"15:42"}`, sent when it changes (`n`: players, `s`: the mode's status line, for modes that have one) |
| frame `…0003` | read, notify | 128 bytes: the 256 LEDs as levels 0-15, two per byte (high nibble first), row by row |
| catalog `…0004` | read | the scene catalog: lines `M`/`A`/`G`/`D`, tab, id (without `a/`, `g/`, `d/`), tab, name: modes, animations, games, drawings; updated when a drawing is saved or deleted |
| settings `…0005` | read, notify | the settings a remote may change, one per line: name, kind (`B` on/off, `C` choice, `N` number), value, label, choices, choice names, min, max (tabs); the notification only says "read again" |

## Data from the web

Everything the lamp downloads - weather (every 15 minutes), Wikipedia's "on
this day" (once a day), the calendar (every 15 minutes), air quality
(every 30 minutes) and the Space Station (every 20 seconds, only while
Mondo is shown) - goes through `src/netfetch.cpp`: one HTTPS client, one
retry rule (after a failure again in 1 minute, then 2, 4, ... up to the
normal period), one status shown in the page and in Diagnostica ("aggiornato
3 min fa", "errore 503 · riprovo tra 4 min · ultimo dato di 2 h fa"), and
the last good answer kept in flash (`/cache/` in LittleFS) for weather,
Wikipedia and air quality, so after a restart the lamp shows them before
the network is back ("dalla memoria").

## Texts

The words people read come from one place each, so the same thing reads the
same on the panel, the page and the Cardputer, and a translation touches a
known set of files:

- messages - refusals, errors, the state of downloads - are in
  `include/texts.h` (`txt::UNKNOWN_GAME`, `txt::FETCH_UPDATED`...);
- names of modes, games and animations are each class's `name()`, read by
  the page and the Cardputer from the lamp;
- names of settings and of their choices ("Media (8 px)", "Solo stelle"...)
  are in `SETTING_DEFS`: the page builds its lists from the state's
  `choices`, the Cardputer from the settings characteristic.

`scripts/check_texts.py` runs before every build and fails it if a retired
name comes back, if a message of `texts.h` is written out again in `src/`,
or if the page lists the choices of a setting by itself.

## Settings

Every setting is described once, in `SETTING_DEFS` (`src/settings.cpp`):
its name, its key in flash (NVS), its type, limits or allowed values, its
default and what changing it affects (the font, the orientation, what is
shown, the weather, the playlist...). From that one table:

- `loadSettings()` reads them (the default for anything missing or out of
  range) and `saveSettings()` writes **only the ones that changed** - a
  setting saved again unchanged costs no flash write;
- the page gets them all in its state as `"settings": {name: value}`;
- the page changes any of them with `POST /api/settings name=value&...`:
  every value is checked first against the table (an unknown choice, a
  number out of range, a playlist naming a mode that doesn't exist...), so
  one refused value changes nothing and the answer says which and why;
  then the effects of what changed are applied once.

The other `/api/...` endpoints are actions, not settings (show a text, a
mode's button, start the hourglass, test the alarm, a game key...).

**Brightness that follows the sun** (Display): with it on, the brightness
set in Display is the one with the sun high; from 15 degrees above the
horizon down to civil dusk (6 degrees below) it slides smoothly to the
chosen minimum, computed from the lamp's latitude and longitude. A time
slot with its own brightness and the night's "dim" still win.

## Backup of the settings

**Backup delle impostazioni** (near the bottom of the page) downloads every
setting as one file, `obegransad-backup.json` (`GET /api/backup`): all the
keys the lamp keeps in flash - so settings added in later versions are
included without changing this feature - plus the added quotes; the
drawings of the gallery aren't. Restoring a file (`POST /api/restore` with
the file as the body) replaces all settings and restarts the lamp; the file
is read completely before anything is changed, so a broken one changes
nothing.

Saved settings carry a version (`cfgVer`): when a new firmware changes a
default or a setting's meaning, `loadSettings()` updates the saved values
once. Version 2, for instance, adds Giochi to a playlist and a 13:00 time
slot that were still the old defaults, while a playlist you edited stays as
it is.

## Updating over WiFi

After the first flash over USB, later versions can go in from the page:
**Aggiornamento firmware** (at the bottom) shows the installed version (git
commit and build time) and takes the file `.pio/build/xhs3e/firmware.bin`
produced by `pio run` (not `firmware.factory.bin`). While it uploads, the
panel shows the percentage and a progress bar at the bottom with a
blinking "packet" running along it; at the end "OK" (or "ERR" blinking if the lamp
rejects the file); the new image is written to the spare app
partition and verified, then the lamp restarts on it, keeping all settings
and drawings. If the upload fails or the file isn't valid, the lamp keeps
running the firmware it had. A new firmware is also on trial: it is kept
only after running for a minute with WiFi up; if it crashes or hangs before
that (the watchdog restarts it), the lamp goes back to the previous
firmware by itself, and the event log says so. As with the rest of the page there is no
password, so anyone on your network could do this.

`scripts/build_info.py` (run by PlatformIO before each build) writes the
version into `include/build_info.h`.

From the command line, the `ota` environment builds and uploads in one go:

```bash
pio run -e ota -t upload                              # to obegransad.local
pio run -e ota -t upload --upload-port 192.168.1.50   # or by IP
```

It sends `firmware.bin` with curl, like the page does
(`curl -F "firmware=@firmware.bin" http://obegransad.local/api/update`), and
fails if the lamp rejects it.

## Build & flash

Requires [PlatformIO](https://platformio.org/) (VS Code extension or the
`pio` CLI).

```bash
pio run                 # build
pio run --target upload # build + flash over USB
pio device monitor      # serial monitor, 115200 baud
```

`platformio.ini` targets `esp32-s3-devkitc-1` with the flash/PSRAM settings
required for the N16R8 module (16MB flash, octal PSRAM). If your board
exposes only a native USB port (no separate USB-serial chip) and you don't
get a COM port or serial output, uncomment the `ARDUINO_USB_CDC_ON_BOOT`
build flags at the bottom of `platformio.ini`. If upload doesn't start
automatically, hold **BOOT**, tap **RESET**, then release **BOOT** to force
the board into download mode.
