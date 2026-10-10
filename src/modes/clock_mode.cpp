#include "modes/clock_mode.h"

#include "animation.h"

#include "display.h"
#include "font_mini.h"
#include "moon.h"
#include "settings.h"
#include "timekeeping.h"
#include "ui.h"
#include "weather.h"
#include "weather_art.h"
#include "weather_icons.h"

// Layout (the outer border is the seconds track):
//
//   +----------------+
//   |tt° picture     |   temperature x1-6, rows 1-5, degree sign x7, row 1
//   |    picture     |   the weather, drawn shaded and moving (weather_art.h),
//   |    picture     |   x8-14, rows 1-9
//   |HH MM           |   hours x1-6 and minutes x8-14, rows 10-14
//   +----------------+
//
// All numbers are in the 5-row digits of the mini font (the text font's
// rounded shapes, a row shorter), so the weather gets a 7x9 picture.
// Everything stays inside the seconds track: the hours have no leading zero
// and a 2-pixel tens digit (only ever 1 or 2), so they take the same
// columns as the temperature above them.
//
// Until there is weather data only the time is shown, in the same digits,
// centred (rows 5-9); until the time is known, the shared "waiting" dots
// (see ui.h).

static const int DIGIT_ROWS = MINI_HEIGHT;  // 5
static const int TIME_ROW = 10;             // the time's top row (with weather)

// 2-pixel-wide tens digits, so a two-digit temperature or hour fits in 6
// columns.
struct NarrowGlyph {
  char c;
  uint8_t rows[DIGIT_ROWS];  // bit 7 = leftmost column
};
static const NarrowGlyph NARROW_TENS[] = {
    {'1', {0x40, 0xC0, 0x40, 0x40, 0x40}},
    {'2', {0x80, 0x40, 0x40, 0x80, 0xC0}},
    {'3', {0x80, 0x40, 0x80, 0x40, 0x80}},
    {'-', {0x00, 0x00, 0xC0, 0x00, 0x00}},
};

static void drawRows(int x, int y, const uint8_t *rows, int width) {
  for (int r = 0; r < DIGIT_ROWS; r++) {
    for (int k = 0; k < width; k++) {
      if (rows[r] & (0x80 >> k)) display.setPixel(x + k, y + r, true);
    }
  }
}

// Tens digit (or minus) in x1-2; false if `c` has no 2-pixel form.
static bool drawNarrow(int y, char c) {
  for (const NarrowGlyph &g : NARROW_TENS) {
    if (g.c != c) continue;
    drawRows(1, y, g.rows, 2);
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

// A digit right-aligned in the 3-column slot at x, so the narrower 1
// doesn't shift the digits next to it.
static void drawDigit(int x, int y, char c) {
  const MiniGlyph *g = findMiniGlyph(c);
  drawRows(x + 3 - g->width, y, g->rows, g->width);
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
// after it on its top row (x7). Two-digit values starting with 1, 2, 3 or a
// minus use 2-pixel tens so the number fits in x1-6; 40 and above take
// x1-7 and go without the degree.
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
  display.setPixel(7, y, true);
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

String ClockMode::status() const {
  if (settings.clockStyle == "binary") return "Quadrante binario";
  if (settings.clockStyle == "words") return "Quadrante a parole";
  if (settings.clockStyle == "wordsen") return "Quadrante a parole, in inglese";
  return "Ora e meteo";
}

void ClockMode::start() {
  lastDraw_ = 0;
  face_ = settings.clockStyle == "weather" ? nullptr : findAnimation(settings.clockStyle);
  if (face_ && !face_->isClockFace()) face_ = nullptr;
  if (face_) face_->start();
  display.beginTransition();
}

void ClockMode::update(uint32_t now) {
  if (face_) {
    if (now - lastDraw_ < face_->frameMs()) return;
    lastDraw_ = now;
    face_->frame(now);
    display.render();
    return;
  }
  if (now - lastDraw_ < 50) return;
  lastDraw_ = now;
  struct tm t;
  if (!localTime(t)) {
    display.clear();
    ui::waiting(now);  // until the clock has synced
    display.render();
    return;
  }

  const Weather weather = weatherNow();
  display.clear();
  if (weather.valid) {
    drawTemperature(1, weather.temperature);
    drawHours(TIME_ROW, t.tm_hour);
    drawNumber(8, TIME_ROW, t.tm_min);
    // Rain within 2 hours: the picture alternates with an umbrella every 2 s.
    if (rainSoon(weather) && (now / 2000) % 2) {
      sprites::draw(spr::WEATHER_UMBRELLA, 8, 2, sprites::frameAt(spr::WEATHER_UMBRELLA, now));
    } else {
      drawWeatherArt(weather.code, weather.isDay, moonPhase(time(nullptr)), 8, 1, 7, 9, now);
    }
  } else {
    // No weather yet: just the time, in the same digits, centred.
    drawHours(5, t.tm_hour);
    drawNumber(8, 5, t.tm_min);
  }
  drawSeconds(t.tm_sec);
  display.render();
}

void ClockMode::action() { requestWeatherUpdate(); }
