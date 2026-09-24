#include "modes/clock_mode.h"

#include "bigdigits.h"
#include "display.h"
#include "font_mini.h"
#include "settings.h"
#include "timekeeping.h"
#include "ui.h"
#include "weather.h"
#include "weather_icons.h"

// Layout (the outer border is the seconds track):
//
//   +----------------+
//   |temp° icon      |   temperature x1-6, rows 2-6, degree sign x8, row 2
//   |                |   weather icon x9-14, rows 1-7
//   |HH MM           |   hours x1-6 and minutes x8-14, rows 9-13
//   +----------------+
//
// All numbers are in the mini font (3x5), as in Previsioni.
//
// Everything stays inside the seconds track: the hours have no leading zero
// and a 2-pixel tens digit (only ever 1 or 2), so they take the same
// columns as the temperature above them.
//
// Until there is weather data the clock uses big digits over the whole
// inner area instead, and until the time is known the shared "waiting"
// dots (see ui.h).


// 2-pixel-wide tens digits, so a two-digit temperature or hour fits in 6
// columns: the mini font's square shapes, squeezed (its 1 is already 2
// pixels wide).
struct NarrowGlyph {
  char c;
  uint8_t rows[MINI_HEIGHT];  // bit 7 = leftmost column
};
static const NarrowGlyph NARROW_TENS[] = {
    {'2', {0xC0, 0x40, 0xC0, 0x80, 0xC0}},
    {'3', {0xC0, 0x40, 0xC0, 0x40, 0xC0}},
    {'-', {0x00, 0x00, 0xC0, 0x00, 0x00}},
};

// Tens digit (or minus) in x1-2; false if `c` has no 2-pixel form.
static bool drawNarrow(int y, char c) {
  if (c == '1') {
    ui::mini(1, y, "1");
    return true;
  }
  for (const NarrowGlyph &g : NARROW_TENS) {
    if (g.c != c) continue;
    for (int r = 0; r < MINI_HEIGHT; r++) {
      for (int k = 0; k < 2; k++) {
        if (g.rows[r] & (0x80 >> k)) display.setPixel(1 + k, y + r, true);
      }
    }
    return true;
  }
  return false;
}

// Border pixel for second `s`: clockwise from the top centre. The 16x16
// border has exactly 60 pixels, one per second.
static void borderPixel(int s, int &x, int &y) {
  if (s < 8) { x = 8 + s; y = 0; return; }
  s -= 8;
  if (s < 15) { x = 15; y = 1 + s; return; }
  s -= 15;
  if (s < 15) { x = 14 - s; y = 15; return; }
  s -= 15;
  if (s < 15) { x = 0; y = 14 - s; return; }
  s -= 15;
  x = 1 + s;
  y = 0;
}

// A mini-font digit right-aligned in the 3-column slot at x, so the
// narrower 1 doesn't shift the digits next to it.
static void drawDigit(int x, int y, char c) {
  const String d(c);
  ui::mini(x + 3 - ui::miniWidth(d), y, d);
}

// Hours in x1-6: the units at x4-6 and, from 10, a 2-pixel tens digit at
// x1-2 (the same columns as the temperature's).
static void drawHours(int y, int hour) {
  if (hour >= 10) drawNarrow(y, '0' + hour / 10);
  drawDigit(4, y, '0' + hour % 10);
}

// Two-digit number in the slots at x and x + 4.
static void drawNumber(int x, int y, int value) {
  drawDigit(x, y, '0' + value / 10);
  drawDigit(x + 4, y, '0' + value % 10);
}

// Temperature from x1, clamped to -9..99, with a one-pixel degree sign
// after it on its top row. Two-digit values starting with 1, 2, 3 or a
// minus use 2-pixel tens so the number fits in x1-6; the degree sits at x8,
// a column away from the digits' full top row, and the icon pixel next to
// it is turned off. 40 and above take x1-7 and go without it.
static void drawTemperature(int y, float celsius) {
  int t = (int)lroundf(celsius);
  if (t < -9) t = -9;
  if (t > 99) t = 99;
  const String s(t);
  if (s.length() == 1) {
    drawDigit(4, y, s[0]);
  } else if (drawNarrow(y, s[0])) {
    drawDigit(4, y, s[1]);
  } else {
    drawNumber(1, y, t);  // x1-7: no room for the degree sign
    return;
  }
  display.setPixel(9, y, false);  // keep the degree apart from the icon
  display.setPixel(8, y, true);
}

// Seconds with a fractional part: milliseconds since the second last
// changed, so the dot can glide between border pixels.
static float smoothSeconds(int sec) {
  static int lastSec = -1;
  static uint32_t secStart = 0;
  if (sec != lastSec) {
    lastSec = sec;
    secStart = millis();
  }
  return sec + min(999u, (unsigned)(millis() - secStart)) / 1000.0f;
}

// The seconds dot, gliding round the border: its light is shared between
// the two pixels it is between (so it moves smoothly instead of jumping
// once a second) and it leaves a trail that fades over 3 pixels.
static void drawSeconds(int sec) {
  static const float TRAIL = 3.0f;
  const float pos = smoothSeconds(sec);
  const int head = (int)floorf(pos);
  for (int i = head + 1; i >= head - (int)TRAIL; i--) {
    const float d = pos - i;  // how far behind the dot this pixel is
    float v = d < 0 ? 1 + d : 1 - d / TRAIL;  // d < 0: the pixel it is entering
    if (v <= 0) continue;
    v = v * v;  // falls off quickly behind the head
    int x, y;
    borderPixel(((i % 60) + 60) % 60, x, y);
    const uint8_t level = (uint8_t)(v * 255 + 0.5f);
    if (level > display.getLevel(x, y)) display.setLevel(x, y, level);  // never dims a digit
  }
}

void ClockMode::start() { lastDraw_ = 0; }

void ClockMode::update(uint32_t now) {
  if (now - lastDraw_ < 50) return;
  lastDraw_ = now;
  struct tm t;
  if (!localTime(t)) {
    display.clear();
    ui::waiting(now, 7);  // until the clock has synced
    display.render();
    return;
  }

  const Weather weather = weatherNow();
  display.clear();
  if (weather.valid) {
    drawHours(9, t.tm_hour);
    drawNumber(8, 9, t.tm_min);
    // Rain within 2 hours: the icon alternates with an umbrella every 2 s.
    const bool umbrella = rainSoon(weather) && (now / 2000) % 2;
    const AnimatedIcon &icon = umbrella ? ICON_UMBRELLA : iconFor(weather.code, weather.isDay);
    const uint8_t frame = (now / icon.frameMs) % icon.frameCount;
    display.drawBitmap(9, 1, icon.frames[frame], 6, 7);
    drawTemperature(2, weather.temperature);  // after the icon: it clears a pixel of it
  } else {
    display.drawBitmap(2, 1, BIG_DIGITS[t.tm_hour / 10], 5, 6);
    display.drawBitmap(8, 1, BIG_DIGITS[t.tm_hour % 10], 5, 6);
    display.drawBitmap(2, 9, BIG_DIGITS[t.tm_min / 10], 5, 6);
    display.drawBitmap(8, 9, BIG_DIGITS[t.tm_min % 10], 5, 6);
  }
  drawSeconds(t.tm_sec);
  display.render();
}

void ClockMode::action() { requestWeatherUpdate(); }
