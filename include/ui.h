#pragma once

#include <Arduino.h>

// The shared look of the lamp's information screens, so they all read the
// same way:
//  - a header band on rows 0-4 with scrolling text in the mini font
//    (Conto alla rovescia: the event; Previsioni uses the same band for a
//    still "VE 26");
//  - one "waiting for data" animation - three dots filling in -
//    replaced by a blinking WiFi sign while the lamp is offline;
//  - everything at full brightness.
namespace ui {

static const int HEADER_ROWS = 5;             // rows 0-4
static const uint32_t HEADER_STEP_MS = 90;    // header scroll: 1 pixel per step
static const int HEADER_GAP = 8;              // blank pixels between texts

// Mini-font text with its top-left corner at (x, y); returns the x just
// after it (plus one pixel of spacing).
int mini(int x, int y, const String &text, uint8_t level = 255);
// Width of mini-font text in pixels, without trailing spacing.
int miniWidth(const String &text);

// Pixels the header moves before `text` has scrolled by and the one after
// it has reached the left edge.
int headerPeriod(const String &text);
// Header band: `text` scrolled left by `offset` pixels, followed after the
// gap by `next` (pass the same text to loop it).
void header(const String &text, int offset, const String &next);
// A header that just loops `text`, timed by `now`.
void headerLoop(const String &text, uint32_t now);

// Waiting for data: three dots on row `y` appearing one by one, or a blinking
// WiFi sign centred there while WiFi is down.
void waiting(uint32_t now, int y);

}  // namespace ui
