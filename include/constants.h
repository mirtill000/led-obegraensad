#pragma once

// ---------------------------------------------------------------------------
// Wiring: Sparkle IoT XH-S3E (ESP32-S3-WROOM-1-N16R8) <-> OBEGRÄNSAD panel
//
// The N16R8 module uses GPIO26-37 internally for its Octal SPI flash/PSRAM,
// GPIO19/20 for native USB and GPIO43/44 for UART0 -> all of those must stay
// untouched. The pins below are all free, non-strapping GPIOs and match the
// same signal names used on the panel's 6-pin connector (see README.md).
// ---------------------------------------------------------------------------
#define PIN_ENABLE 5  // EN  - shift register output-enable (active low)
#define PIN_DATA   9  // IN  - serial data in   (SPI MOSI)
#define PIN_CLOCK  7  // CLK - serial clock      (SPI SCK)
#define PIN_LATCH  6  // CLA - latch / store clock
#define PIN_BUTTON 4  // optional push button, other leg to GND

#define COLS 16
#define ROWS 16
#define TOTAL_PIXELS (ROWS * COLS)

// Default text and delay (ms) between scroll steps, used on first boot;
// afterwards both are set from the web page and kept in flash.
// The text scrolls on one line through the middle of the panel.
#define MESSAGE "dare mighty things"
#define SCROLL_DELAY_MS 80

// Grayscale: each LED gets 32 brightness levels by showing 5 bit planes
// for 1, 2, 4, 8 and 16 time units in a loop, ~270 times a second (binary
// code modulation). Set to false for plain on/off pixels, pushed only when
// the image changes.
#define GRAYSCALE true

// Clockwise rotation (0, 90, 180, 270) applied to the image for each way
// the lamp can hang; the orientation itself is picked on the web page.
// The panel's native "up" is vertical. If the text comes out upside down,
// change the value by 180.
#define ROTATION_HORIZONTAL 270
#define ROTATION_VERTICAL 0

// Flip the image if your panels ended up wired mirrored/upside down.
// Flips are applied before the rotation.
#define FLIP_HORIZONTAL false
#define FLIP_VERTICAL false

// Network: hostname (reachable as http://obegransad.local on most systems).
// The lamp only joins the home network from secrets.h; while it can't, it
// keeps retrying every WIFI_RETRY_MS.
#define HOSTNAME "obegransad"
#define WIFI_RETRY_MS 20000

// Local time zone (POSIX TZ string) for the clock and the hourly quotes.
// Default: Italy / Central Europe, with daylight saving time.
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"

// Default weather location (latitude, longitude): Milan. Can be changed from
// the web page.
#define DEFAULT_LATITUDE 45.4642f
#define DEFAULT_LONGITUDE 9.1900f
