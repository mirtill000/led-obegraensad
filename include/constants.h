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

// Text to scroll across the matrix and the delay (ms) between scroll steps.
#define MESSAGE "HELLO WORLD   "
#define SCROLL_DELAY_MS 80

// Clockwise rotation (0, 90, 180, 270) applied to the image so text reads
// correctly for how the lamp is mounted. The panel's native "up" is
// vertical; 90 turns it for a lamp hung horizontally. If the text comes out
// upside down, use 270 instead.
#define ROTATION 90

// Flip the image if your panels ended up wired mirrored/upside down.
// Flips are applied before ROTATION.
#define FLIP_HORIZONTAL false
#define FLIP_VERTICAL false
