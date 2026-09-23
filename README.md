# OBEGRÄNSAD "HELLO WORLD" on Sparkle IoT XH-S3E

Minimal standalone firmware that drives the salvaged IKEA OBEGRÄNSAD 16x16
LED matrix from a **Sparkle IoT XH-S3E** board (ESP32-S3-WROOM-1-N16R8,
16MB flash / 8MB octal PSRAM, WiFi+BT) and scrolls a message across it
forever (by default **dare mighty things** on two lines). No WiFi/web UI -
just the display driver, a small lowercase font and a
scroll loop. (Inspired by [ph1p/ikea-led-obegraensad](https://github.com/ph1p/ikea-led-obegraensad),
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
  constants.h   - pin assignment, matrix size, message text
  font_small.h  - 7px-tall proportional lowercase font (a-z, 0-9, . , ! ? ' -)
  display.h
src/
  display.cpp   - shift-register driver + font renderer
  main.cpp      - calls display.scrollTextOnce(MESSAGE) forever
platformio.ini
```

To change the message or scroll speed, edit `MESSAGE` / `SCROLL_DELAY_MS` in
`include/constants.h`. `ROTATION` (0/90/180/270, clockwise) sets which way
the text reads: the default `270` is for a lamp mounted horizontally; use `0`
if yours stands vertically, and `90` if the text comes out upside down. If
it's mirrored (some panels get reassembled with the connector on a different
edge), flip `FLIP_HORIZONTAL` / `FLIP_VERTICAL` in the same file.

A `|` in `MESSAGE` splits it into two lines stacked on top of each other
that scroll together, top line first (e.g. `"dare|mighty things"`); without
`|` a single line scrolls through the middle of the panel.

The font is lowercase only (uppercase letters are drawn as lowercase), plus
digits and a few punctuation marks; anything else is shown as a space. To
add characters, add entries to `FONT_GLYPHS` in `include/font_small.h`
(width in pixels + 7 rows, bit 7 = leftmost column).

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
