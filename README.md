# OBEGRÄNSAD on Sparkle IoT XH-S3E

Standalone firmware that drives the salvaged IKEA OBEGRÄNSAD 16x16 LED
matrix from a **Sparkle IoT XH-S3E** board (ESP32-S3-WROOM-1-N16R8, 16MB
flash / 8MB octal PSRAM, WiFi+BT). A small web page over WiFi switches
between modes: scrolling text (by default **dare mighty things**), a quote of the hour, clock + weather, Conway's Game of Life, ambient
animations, or off. (Inspired by
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
256-bit frame when the image changes; there's no continuous refresh loop
required.

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
  font_small.h       - 8px-tall proportional lowercase font (a-z, 0-9, . , ! ? ' -)
  display.h, modes.h, settings.h, web.h
src/
  display.cpp        - shift-register driver, font renderer, brightness
  modes.cpp          - list of modes + switching between them
  modes/             - one file per mode
  settings.cpp       - settings saved in flash (NVS)
  timekeeping.cpp    - NTP time sync (time zone: TIMEZONE in constants.h)
  weather.cpp        - current weather from Open-Meteo (free, no API key)
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

From the page you can pick the active mode, change the scrolling text, set
the weather location, pick an animation, switch between horizontal and
vertical, and set brightness and scroll speed. Modes that have a command of their own ("next
quote", "restart", ...) show it as a button under the mode list. Everything
is saved in flash, so the lamp comes back in the same state after a power
cut.

No push button is needed: everything is on the page. If you do wire one to
GPIO4 (other leg to GND), each press switches to the next mode.

Clock, weather and the hourly quote need your network to have internet
access.

The page has no password: anyone on the same network can use it.

Current modes:

- **Testo scorrevole** - scrolls the text set on the page
- **Frase dell'ora** - a different motivational quote every hour (list in
  `src/modes/quotes_mode.cpp`); button: next quote
- **Orologio e meteo** - hours on top, minutes below, a dot running round
  the border for the seconds; every 20 s it shows the weather for 6 s (icon
  and temperature, refreshed every 15 min); button: refresh weather
- **Gioco della vita** - Conway's Game of Life with wrap-around edges;
  reseeds itself when the pattern dies, freezes or loops; button: restart
- **Animazioni** - digital rain, fire, stars, waves or a "breathing" circle;
  "automatic" changes animation every 5 minutes and shows only stars from
  22:00 to 07:00; button: next animation
- **Spento** - all LEDs off

To add a mode, implement the `Mode` class from `include/modes.h` in
`src/modes/` and add it to `MODES` in `src/modes.cpp`; it shows up on the
page automatically.

### Scrolling text

The text from the page scrolls on one line through the middle of the panel.
`MESSAGE` in `constants.h` is only the default used on first boot.

(The display code can also scroll two lines stacked on top of each other,
split by a `|` - the hourly quotes use that.)

The font is lowercase only (uppercase letters are drawn as lowercase), plus
digits and a few punctuation marks; anything else is shown as a space. To
add characters, add entries to `FONT_GLYPHS` in `include/font_small.h`
(width in pixels + 8 rows, bit 7 = leftmost column).

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
