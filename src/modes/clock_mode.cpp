#include "modes/clock_mode.h"

#include "bigdigits.h"
#include "display.h"
#include "settings.h"
#include "timekeeping.h"
#include "ui.h"
#include "weather.h"
#include "weather_icons.h"

// Layout (the outer border is the seconds track):
//
//   +----------------+
//   |temp° icon      |   temperature x1-6, rows 1-6, degree sign x7, row 1
//   |                |   weather icon x9-14, rows 1-7
//   |HH MM           |   hours x1-7 and minutes x9-15, rows 8-13
//   +----------------+
//
// "HH MM" needs 15 columns, one more than inside the seconds track, so the
// minutes' last column lies on the track's right edge.
//
// Until there is weather data the clock uses big digits over the whole
// inner area instead, and until the time is known the shared "waiting"
// dots (see ui.h).


// 2-pixel-wide tens digits so a two-digit temperature fits in 6 columns.
struct NarrowGlyph {
  char c;
  uint16_t rows[6];
};
static const NarrowGlyph NARROW_TENS[] = {
    {'1', {0x4000, 0xC000, 0x4000, 0x4000, 0x4000, 0x4000}},
    {'2', {0xC000, 0x4000, 0xC000, 0x8000, 0x8000, 0xC000}},
    {'3', {0xC000, 0x4000, 0xC000, 0x4000, 0x4000, 0xC000}},
    {'-', {0x0000, 0x0000, 0x0000, 0xC000, 0x0000, 0x0000}},
};

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

// Two-digit number in the small font with its left edge at x.
static void drawSmallNumber(int x, int y, int value) {
  char buf[3] = {(char)('0' + value / 10), (char)('0' + value % 10), 0};
  display.drawText(x, y, buf, 0, 2);
}

// Temperature from x1, clamped to -9..99, with a one-pixel degree sign
// after it on its top row. Two-digit values starting with 1, 2, 3 or a
// minus use 2-pixel tens so the number fits in x1-6 and the degree sits at
// x7, clear of the icon; 40 and above take x1-7 and go without it.
static void drawTemperature(int y, float celsius) {
  int t = (int)lroundf(celsius);
  if (t < -9) t = -9;
  if (t > 99) t = 99;
  const String s(t);
  const NarrowGlyph *narrow = nullptr;
  if (s.length() == 2) {
    for (const NarrowGlyph &g : NARROW_TENS) {
      if (g.c == s[0]) narrow = &g;
    }
  }
  if (narrow) {
    // 2-wide tens, 1 gap, 3-wide units: 6 columns, x1-6.
    display.drawBitmap(1, y, narrow->rows, 2, 6);
    display.drawChar(4, y, s[1]);
  } else {
    const int w = Display::textWidth(s.c_str(), 0, s.length()) - 1;
    display.drawText(w <= 6 ? 7 - w : 1, y, s.c_str(), 0, s.length());
    if (w > 6) return;  // no room for the degree sign
  }
  display.setPixel(8, y, false);  // keep the degree apart from the icon
  display.setPixel(7, y, true);
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
    drawSmallNumber(1, 8, t.tm_hour);
    drawSmallNumber(9, 8, t.tm_min);
    // Rain within 2 hours: the icon alternates with an umbrella every 2 s.
    const bool umbrella = rainSoon(weather) && (now / 2000) % 2;
    const AnimatedIcon &icon = umbrella ? ICON_UMBRELLA : iconFor(weather.code, weather.isDay);
    const uint8_t frame = (now / icon.frameMs) % icon.frameCount;
    display.drawBitmap(9, 1, icon.frames[frame], 6, 7);
    drawTemperature(1, weather.temperature);  // after the icon: it clears a pixel of it
  } else {
    display.drawBitmap(2, 1, BIG_DIGITS[t.tm_hour / 10], 5, 6);
    display.drawBitmap(8, 1, BIG_DIGITS[t.tm_hour % 10], 5, 6);
    display.drawBitmap(2, 9, BIG_DIGITS[t.tm_min / 10], 5, 6);
    display.drawBitmap(8, 9, BIG_DIGITS[t.tm_min % 10], 5, 6);
  }
  // Seconds dot with a short fading trail.
  static const uint8_t TRAIL[3] = {255, 70, 20};
  for (int i = 2; i >= 0; i--) {
    int x, y;
    borderPixel((t.tm_sec - i + 60) % 60, x, y);
    display.setLevel(x, y, TRAIL[i]);
  }
  display.render();
}

void ClockMode::action() { requestWeatherUpdate(); }
