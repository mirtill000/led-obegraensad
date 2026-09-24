# OBEGRÄNSAD on Sparkle IoT XH-S3E

Standalone firmware that drives the salvaged IKEA OBEGRÄNSAD 16x16 LED
matrix from a **Sparkle IoT XH-S3E** board (ESP32-S3-WROOM-1-N16R8, 16MB
flash / 8MB octal PSRAM, WiFi+BT). A small web page over WiFi switches
between modes: scrolling text (by default **dare mighty things**), a quote
of the hour, clock + weather, a 12-hour forecast, things from the web
(word of the day, "on this day", your calendar), Conway's Game of Life,
Super Mario and other games, animations, your own drawings, a countdown
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

To get levels in between, the firmware uses binary code modulation: each
pixel's brightness (0-255, gamma-corrected to 32 steps) is split into 5 bit
planes, and a hardware timer keeps pushing them in a loop, holding plane
*n* for 2^n x 120 us. A full cycle takes 3.7 ms (~270 Hz, no visible
flicker) and costs a few percent of one CPU core. The animations use it for
fading trails, fire, twinkling stars and soft edges. Set `GRAYSCALE` to
`false` in `include/constants.h` to go back to plain on/off pixels.

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
  font_small.h       - 8px-tall proportional font (a-z, A-Z, à-ù, 0-9, . , : ! ? ' -)
  font_mini.h        - 5px-tall capitals (the "Mini" font; "Grande" is built from font_small)
  display.h, modes.h, settings.h, web.h
src/
  display.cpp        - shift-register driver, font renderer, brightness
  modes.cpp          - list of modes + switching between them
  modes/             - one file per mode
  animations/        - the animations of the "Animazioni" mode
  settings.cpp       - settings saved in flash (NVS)
  timekeeping.cpp    - NTP time sync (time zone: TIMEZONE in constants.h)
  net.cpp            - background task for everything downloaded
  weather.cpp        - weather, 12-hour forecast, sunrise/sunset (Open-Meteo)
  webinfo.cpp        - word of the day, Wikipedia "on this day", iCal calendar
  moon.cpp           - moon phase from the date
  gallery.cpp        - drawings saved in flash (LittleFS)
  web.cpp            - control page + JSON API
  main.cpp           - WiFi, button, main loop
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

The page shows the modes at the top; the one on the panel is highlighted,
with its own command button ("next quote", "jump", ...; the space bar
works too) and a **speed** slider (1-9, per mode). Below that only the
settings of the mode being shown appear (the text, the quotes list, the
animation menu, ...). General settings are in collapsible sections:

- **Playlist** - modes shown in turn, each for the minutes you choose
  (e.g. clock 10 min, quote 3 min, animations 5 min). Picking a mode by
  hand stops the playlist.
- **Sveglia con l'alba** - on the chosen days, from 5-60 minutes before the
  alarm a sun rises on the panel while the brightness slowly goes up; it
  stays bright for a while after. It wins over everything else; the mode
  button (or the page) stops it, and "Prova" shows a one-minute sunrise.
- **Giorno e notte** - between two times (e.g. 23:00-07:00), or from sunset
  to sunrise, the lamp is off, shows only stars, or keeps going at a lower
  brightness. The night wins over the playlist and over the mode picked by
  hand.
- **Luogo e ora** - today's sunrise, sunset and moon phase; search a city by name (the browser asks Open-Meteo's
  free geocoding service and sends the lamp just the coordinates) and pick
  the time zone; picking a city also picks its time zone when it's in the
  list.
- **Display** - horizontal/vertical, brightness, and the font of all
  scrolling text: *Attuale* (proportional, 8 pixels, lowercase and
  accents), *Grande* (the same font doubled with the EPX/Scale2x algorithm,
  which keeps diagonals smooth: it fills the whole panel, for reading from
  across the room) or *Mini 3x5* (capitals only, 5 pixels). Fixed layouts
  like the clock digits keep their own font.

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
  every pass, the default)
- **Frase dell'ora** - a different quote every hour, scrolling on one line
  at the chosen height (top, middle, bottom or variable, like the text).
  The list is edited on the page (one per line); "restore" brings back the
  built-in one (in `src/modes/quotes_mode.cpp`). Button: next quote
- **Orologio e meteo** - one screen: hours and minutes on the left, an
  animated weather icon (falling rain or snow, flashing lightning, drifting
  clouds, ...) and the temperature with a one-pixel degree sign on the
  right, and a dot running round the border for the seconds. Weather is
  refreshed every 15 min; until the first reading arrives the clock uses
  big digits. Button: refresh weather
- **Previsioni** - today at a glance on one screen: on top a sun on the
  horizon with an up/down arrow and the sunrise or sunset time (they
  alternate every 4 s), at the bottom today's minimum (dim) and maximum
  (bright) temperature, plus an animated drop when rain is likely
  (>= 50%) today. The web page still shows the hourly chart. The clock
  also shows an umbrella next to the weather icon when rain is likely
  (>= 60%) within 2 hours
- **Dal web** - in turn: the word of the day (built-in list), an "on this
  day" event from Italian Wikipedia, and the next event of your calendar
  (paste its secret iCal link, e.g. from Google Calendar; recurring events
  aren't supported). Choose the sources and the height on the page
- **Gioco della vita** - Conway's Game of Life with wrap-around edges, 4
  generations a second. Each game starts from an empty board with a small
  pattern in the middle (R-pentomino, acorn, diehard, ...) that grows for
  40-150 generations; when the board dies, freezes or loops a new game
  starts; button: restart
- **Super Mario** - side-scrolling platformer (Mario is a 5x7 sprite in
  grayscale: cap, face and moustache, overalls, shoes) with pipes, pits, goombas
  (stomp them) and coins. In demo mode an autopilot simulates the next
  moves and jumps at the best moment; otherwise you jump. At game over it
  shows the score and starts again; button: restart
- **Animazioni** - one animation, or "automatic" (a different one every 5
  minutes); button: next animation. The animations, in
  `src/animations/`:
  - *Atmosfere*: digital rain, fire, stars, waves
  - *Giochi*: **Tetris** - in demo mode, for each piece the computer tries
    every rotation and column and picks the best by stack height, holes and
    surface; **Snake** - in demo mode it takes the shortest way to the food
    only if it can still reach its tail afterwards; **Pong** - you against
    the computer, first to 5; **Breakout** - 3 lives, faster at each level;
    **Flappy Bird**; **Space Invaders** - waves that get faster; **2048** -
    tile brightness shows the value
  - *Orologi*: analog (antialiased hands, smooth seconds), binary (one
    column of bits per digit of HH:MM, a bar filling with the seconds), in
    words ("sono le tre e un quarto", "è l'una meno cinque"...)
  - *3D e demo*: rotating wireframe cube, plasma, metaballs, endless zoom
    into the Mandelbrot set
- **Disegni** - your drawings and animations, one or all in turn; the
  gallery starts with a few examples (a beating heart, the Super Mario
  mushroom, a cat, a flower, Pac-Man, a space invader). On the page there
  is a 16x16 pixel editor, which opens on the drawing the lamp is showing (6 brightness levels, fill, invert,
  up to 32 frames, 2-15 frames a second) that shows the drawing live on
  the lamp while you draw; you can also import a photo or an animated GIF
  (cropped to a square, turned into grayscale, contrast stretched). Up to
  60 drawings are saved in flash
- **Conto alla rovescia** - days left to a date in big digits (hours and
  minutes on the day), alternating with "Mancano 12 giorni a Vacanze"
- **Spento** - all LEDs off

### Games and demo mode

Super Mario and all the games in the animations have a **Modalità demo** checkbox (on by
default), shown on the page while the game is on the panel:

- **on** - the game plays by itself and ignores input;
- **off** - you play, with the on-screen pad or the keyboard: Mario jumps
  with *Salta*, space or up (a press just before landing still counts);
  Tetris moves with left/right, rotates with up, drops with down or space;
  Snake and 2048 use the arrows; Pong up/down; Breakout left/right; Flappy
  Bird flies with *Vola*, space or up; Space Invaders moves with left/right
  and shoots with *Spara* or space. In the paddle games holding an arrow
  down keeps moving.

Games shown by the "automatic" animation rotation or by the night schedule
always run as demos. Controls go to the lamp over WiFi, so expect a small
delay; lower the game's speed if it's too hard.

To add a mode, implement the `Mode` class from `include/modes.h` in
`src/modes/` and add it to `MODES` in `src/modes.cpp`; to add an animation,
implement `Animation` from `include/animation.h` in `src/animations/` and
list it in `src/animations/animations.cpp`. Both show up on the page
automatically.

### Scrolling text

The text from the page scrolls on one line, at the height picked on the
page (variable by default).
`MESSAGE` in `constants.h` is only the default used on first boot.

(The display code can also scroll two lines stacked on top of each other,
split by a `|`.)

The text is case sensitive: the font has lowercase and capital letters
(capitals are one pixel taller), digits, a few punctuation marks and the
Italian accented lowercase letters (à è é ì ò ù). Accented capitals are
shown without the accent; anything else is shown as a space. To
add characters, add entries to `FONT_GLYPHS` in `include/font_small.h`
(width in pixels + 8 rows, bit 7 = leftmost column).

## Updating over WiFi

After the first flash over USB, later versions can go in from the page:
**Aggiornamento firmware** (at the bottom) shows the installed version (git
commit and build time) and takes the file `.pio/build/xhs3e/firmware.bin`
produced by `pio run` (not `firmware.factory.bin`). While it uploads, the
panel shows a progress bar; the new image is written to the spare app
partition and verified, then the lamp restarts on it, keeping all settings
and drawings. If the upload fails or the file isn't valid, the lamp keeps
running the firmware it had. As with the rest of the page there is no
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
