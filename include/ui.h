#pragma once

#include <Arduino.h>

// The shared look of what the lamp draws, so every screen reads the same:
//  - three sizes of text: a LABEL (the 4-row capitals, centred, scrolling
//    round when too wide - "ARIA", "ISS"), a VALUE (the 6-row digits of the
//    text font, centred - "27") and running text in the font chosen in
//    Display (Scroller, Pager); the clock and Previsioni keep their own
//    tuned digits;
//  - one CARD layout for a reading: label on top, value in the middle,
//    caption at the bottom (Mondo's air quality);
//  - pictures as rows of characters (ICON): '#' at the given level, '+'
//    always full, ':' a third of it;
//  - four brightness steps (LEVEL_*): full for what matters, text for
//    values, dim for labels and captions, faint for backgrounds;
//  - one "waiting for data" sign - three dots filling in, or a blinking
//    WiFi sign while the lamp is offline.
namespace ui {

static const uint8_t LEVEL_FULL = 255, LEVEL_TEXT = 220, LEVEL_DIM = 120, LEVEL_FAINT = 35;

// A picture given as rows of characters, top-left at (x, y): '#' = `level`,
// '+' = full, ':' = a third of `level`, anything else left as it is.
void icon(int x, int y, const char *const *rows, int count, uint8_t level = LEVEL_FULL);

// A label in the 4-row capitals at row y, centred; wider than the panel it
// scrolls round, timed by `t` (ms). Rows y..y+3 should be free.
void label(const String &text, int y, uint8_t level, uint32_t t);
// A value (digits, sign, letters) in the text font's 6-row digits, centred.
void value(const String &text, int y, uint8_t level = LEVEL_FULL);
// A reading: label on rows 0-3, value on rows 5-10, caption on rows 12-15.
void card(const String &title, const String &reading, const String &caption, uint32_t t);

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
