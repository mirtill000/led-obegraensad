#include "modes/clock_mode.h"

#include "bigdigits.h"
#include "display.h"
#include "settings.h"
#include "timekeeping.h"
#include "weather.h"
#include "weather_icons.h"

// Layout (the outer border is the seconds track):
//
//   +----------------+
//   |HH      icon °  |   hours      x1-7,  rows 1-6   weather icon x9-14, rows 1-7
//   |MM      temp    |   minutes    x1-7,  rows 8-13  temperature  x9-14, rows 8-13
//   |                |                                degree sign  x14,   row 7
//
// The degree sign sits on the icon's last row, in a corner no icon frame
// uses; its neighbours there are kept dark so it never merges with a drop.
//   +----------------+
//
// Until there is weather data the clock uses big digits over the whole
// inner area instead.


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

// Temperature right-aligned to column 14, clamped to -9..99.
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
    // 2-wide tens, 1 gap, 3-wide units: 6 columns, x9-14.
    display.drawBitmap(9, y, narrow->rows, 2, 6);
    display.drawChar(12, y, s[1]);
  } else {
    const int w = Display::textWidth(s.c_str(), 0, s.length()) - 1;
    display.drawText(15 - w, y, s.c_str(), 0, s.length());
  }
}

void ClockMode::start() {
  waiting_.start("in attesa dell'ora");
  lastDraw_ = 0;
}

void ClockMode::update(uint32_t now) {
  struct tm t;
  if (!localTime(t)) {
    waiting_.update(now, SCROLL_DELAY_MS);
    return;
  }
  if (now - lastDraw_ < 50) return;
  lastDraw_ = now;

  const Weather weather = weatherNow();
  display.clear();
  if (weather.valid) {
    drawSmallNumber(1, 1, t.tm_hour);
    drawSmallNumber(1, 8, t.tm_min);
    // Rain within 2 hours: the icon alternates with an umbrella every 2 s.
    const bool umbrella = rainSoon(weather) && (now / 2000) % 2;
    const AnimatedIcon &icon = umbrella ? ICON_UMBRELLA : iconFor(weather.code, weather.isDay);
    const uint8_t frame = (now / icon.frameMs) % icon.frameCount;
    display.drawBitmap(9, 1, icon.frames[frame], 6, 7);
    drawTemperature(8, weather.temperature);  // aligned with the minutes
    display.setPixel(13, 7, false);
    display.setPixel(14, 6, false);
    display.setPixel(14, 7, true);  // degree sign, just above the temperature
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
