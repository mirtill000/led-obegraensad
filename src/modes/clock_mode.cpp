#include "modes/clock_mode.h"

#include "bigdigits.h"
#include "display.h"
#include "settings.h"
#include "timekeeping.h"
#include "weather.h"

// Layout (the outer border is the seconds track):
//
//   +----------------+
//   |HH      icon    |   hours      x1-7,  rows 1-6   weather icon x9-14, rows 1-7
//   |MM         °    |   minutes    x1-7,  rows 8-13  degree sign  x14,   row 8
//   |        temp    |                                temperature  x9-14, rows 9-14
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

// Animated weather icons, 6x7 pixels per frame.
struct AnimatedIcon {
  uint16_t frameMs;
  uint8_t frameCount;
  const uint16_t (*frames)[7];
};

static const uint16_t SUN_FRAMES[][7] = {
    {0x0000, 0x3000, 0x7800, 0x7800, 0x3000, 0x0000, 0x0000},
    {0x8400, 0x3000, 0x7800, 0x7800, 0x3000, 0x8400, 0x0000},
    {0x4800, 0xB400, 0x7800, 0x7800, 0xB400, 0x4800, 0x0000},
    {0x8400, 0x3000, 0x7800, 0x7800, 0x3000, 0x8400, 0x0000},
};
static const uint16_t MOON_FRAMES[][7] = {
    {0x3000, 0x4000, 0x8000, 0x8000, 0x4000, 0x3000, 0x0000},
    {0x3400, 0x4000, 0x8000, 0x8800, 0x4000, 0x3000, 0x0000},
    {0x3000, 0x4800, 0x8000, 0x8000, 0x4400, 0x3000, 0x0000},
};
static const uint16_t PARTLY_FRAMES[][7] = {
    {0xA000, 0x4000, 0xB000, 0x3C00, 0x7C00, 0x0000, 0x0000},
    {0x4000, 0xE000, 0x7000, 0x3C00, 0x7C00, 0x0000, 0x0000},
};
static const uint16_t CLOUD_FRAMES[][7] = {
    {0x0000, 0x3000, 0x7800, 0xFC00, 0x7800, 0x0000, 0x0000},
    {0x0000, 0x1800, 0x3C00, 0x7C00, 0x3C00, 0x0000, 0x0000},
    {0x0000, 0x3000, 0x7800, 0xFC00, 0x7800, 0x0000, 0x0000},
    {0x0000, 0x6000, 0xF000, 0xF800, 0xF000, 0x0000, 0x0000},
};
static const uint16_t FOG_FRAMES[][7] = {
    {0x0000, 0xF800, 0x0000, 0x7C00, 0x0000, 0xF800, 0x0000},
    {0x0000, 0x7C00, 0x0000, 0xF800, 0x0000, 0x7C00, 0x0000},
};
static const uint16_t RAIN_FRAMES[][7] = {
    {0x3000, 0x7800, 0xFC00, 0x0000, 0x8800, 0x2000, 0x0800},
    {0x3000, 0x7800, 0xFC00, 0x8800, 0x2000, 0x0800, 0x8000},
    {0x3000, 0x7800, 0xFC00, 0x2000, 0x0800, 0x8000, 0x2000},
    {0x3000, 0x7800, 0xFC00, 0x0800, 0x8000, 0x2000, 0x8800},
};
static const uint16_t SNOW_FRAMES[][7] = {
    {0x3000, 0x7800, 0xFC00, 0x0000, 0x4800, 0x0000, 0x1000},
    {0x3000, 0x7800, 0xFC00, 0x4000, 0x0800, 0x2000, 0x0000},
    {0x3000, 0x7800, 0xFC00, 0x0800, 0x0000, 0x4800, 0x0000},
    {0x3000, 0x7800, 0xFC00, 0x0000, 0x1000, 0x0000, 0x4800},
};
static const uint16_t STORM_FRAMES[][7] = {
    {0x3000, 0x7800, 0xFC00, 0x1000, 0x2000, 0x1000, 0x2000},
    {0x3000, 0x7800, 0xFC00, 0x0000, 0x0000, 0x0000, 0x0000},
    {0x3000, 0x7800, 0xFC00, 0x1000, 0x2000, 0x1000, 0x2000},
    {0x3000, 0x7800, 0xFC00, 0x0000, 0x0000, 0x0000, 0x0000},
    {0x3000, 0x7800, 0xFC00, 0x0000, 0x0000, 0x0000, 0x0000},
    {0x3000, 0x7800, 0xFC00, 0x0000, 0x0000, 0x0000, 0x0000},
};

static const AnimatedIcon ICON_SUN = {400, 4, SUN_FRAMES};
static const AnimatedIcon ICON_MOON = {700, 3, MOON_FRAMES};
static const AnimatedIcon ICON_PARTLY = {600, 2, PARTLY_FRAMES};
static const AnimatedIcon ICON_CLOUD = {700, 4, CLOUD_FRAMES};
static const AnimatedIcon ICON_FOG = {800, 2, FOG_FRAMES};
static const AnimatedIcon ICON_RAIN = {150, 4, RAIN_FRAMES};
static const AnimatedIcon ICON_SNOW = {350, 4, SNOW_FRAMES};
static const AnimatedIcon ICON_STORM = {150, 6, STORM_FRAMES};

// Shown in turn with the weather icon when rain is on its way.
static const uint16_t UMBRELLA_FRAMES[][7] = {
    {0x3000, 0x7800, 0xFC00, 0x1000, 0x1000, 0x5000, 0x2000},
    {0x3000, 0x7800, 0xFC00, 0x9000, 0x1400, 0x5000, 0x2000},
    {0x3000, 0x7800, 0xFC00, 0x1400, 0x9000, 0x5400, 0x2000},
};
static const AnimatedIcon ICON_UMBRELLA = {300, 3, UMBRELLA_FRAMES};

// WMO weather code -> icon (https://open-meteo.com/en/docs).
static const AnimatedIcon &iconFor(int code, bool isDay) {
  if (code == 0) return isDay ? ICON_SUN : ICON_MOON;
  if (code <= 2) return isDay ? ICON_PARTLY : ICON_CLOUD;
  if (code == 3) return ICON_CLOUD;
  if (code == 45 || code == 48) return ICON_FOG;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return ICON_SNOW;
  if (code >= 95) return ICON_STORM;
  if (code >= 51) return ICON_RAIN;
  return ICON_CLOUD;
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
    drawTemperature(9, weather.temperature);
    display.setPixel(14, 8, true);  // degree sign, above the temperature
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
