#pragma once

#include <Arduino.h>

// The shared look of the lamp's information screens, so they all read the
// same way:
//  - text in the text font (font A, 8 rows), e.g. the scrolling header of
//    Conto alla rovescia; only Previsioni, which has no room for it, keeps
//    the 5-row mini font for its still "VE 26";
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

// Header band on rows 0-7: `text` in the text font (compact letters, like
// all scrolling text) scrolling in a loop,
// timed by `now` (capitals on rows 0-5, the descenders below); UTF-8.
void textHeaderLoop(const String &text, uint32_t now);

// Waiting for data: three dots appearing one by one, or a blinking WiFi
// sign while WiFi is down - always at the same height (row 10: below the
// header of the screens that have one).
static const int WAITING_ROW = 10;
void waiting(uint32_t now);

}  // namespace ui
