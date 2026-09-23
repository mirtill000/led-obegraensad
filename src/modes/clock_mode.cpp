#include "modes/clock_mode.h"

#include "display.h"
#include "settings.h"
#include "timekeeping.h"
#include "weather.h"

static const uint32_t CLOCK_MS = 20000;    // how long the clock stays up
static const uint32_t WEATHER_MS = 6000;   // how long the weather stays up

// 5x6 digits for the clock (bit 15 = leftmost column).
static const uint16_t BIG_DIGITS[10][6] = {
    {0x7000, 0x8800, 0x8800, 0x8800, 0x8800, 0x7000},  // 0
    {0x2000, 0x6000, 0x2000, 0x2000, 0x2000, 0x7000},  // 1
    {0x7000, 0x8800, 0x1000, 0x2000, 0x4000, 0xF800},  // 2
    {0xF000, 0x0800, 0x7000, 0x0800, 0x0800, 0xF000},  // 3
    {0x1000, 0x3000, 0x5000, 0x9000, 0xF800, 0x1000},  // 4
    {0xF800, 0x8000, 0xF000, 0x0800, 0x0800, 0xF000},  // 5
    {0x7000, 0x8000, 0xF000, 0x8800, 0x8800, 0x7000},  // 6
    {0xF800, 0x0800, 0x1000, 0x2000, 0x4000, 0x4000},  // 7
    {0x7000, 0x8800, 0x7000, 0x8800, 0x8800, 0x7000},  // 8
    {0x7000, 0x8800, 0x8800, 0x7800, 0x0800, 0x7000},  // 9
};

// 9-row weather icons, drawn centred at the top of the panel.
struct Icon {
  uint8_t width;
  uint16_t rows[9];
};

static const Icon ICON_SUN = {9, {0x0800, 0x4100, 0x1C00, 0x3E00, 0xBE80, 0x3E00, 0x1C00, 0x4100, 0x0800}};
static const Icon ICON_MOON = {10, {0x3800, 0x6080, 0xC000, 0xC280, 0xC000, 0xC000, 0xC000, 0x6000, 0x3800}};
static const Icon ICON_PARTLY = {12, {0x2000, 0xA800, 0x7000, 0xF700, 0x5880, 0x2060, 0x2010, 0x2010, 0x1FE0}};
static const Icon ICON_CLOUD = {11, {0x0000, 0x0000, 0x0E00, 0x3100, 0x40C0, 0x8020, 0x8020, 0x7FC0, 0x0000}};
static const Icon ICON_FOG = {11, {0x0000, 0xFFC0, 0x0000, 0x7FE0, 0x0000, 0xFFC0, 0x0000, 0x7FE0, 0x0000}};
static const Icon ICON_RAIN = {11, {0x0E00, 0x3100, 0x40C0, 0x8020, 0x7FC0, 0x0000, 0x2480, 0x4900, 0x9200}};
static const Icon ICON_SNOW = {11, {0x0E00, 0x3100, 0x40C0, 0x8020, 0x7FC0, 0x0000, 0x4440, 0x1100, 0x4440}};
static const Icon ICON_STORM = {11, {0x0E00, 0x3100, 0x40C0, 0x8020, 0x73C0, 0x0C00, 0x1E00, 0x0400, 0x0800}};

// WMO weather code -> icon (https://open-meteo.com/en/docs).
static const Icon &iconFor(int code, bool isDay) {
  if (code == 0) return isDay ? ICON_SUN : ICON_MOON;
  if (code <= 2) return ICON_PARTLY;
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

void ClockMode::start() {
  waiting_.start("in attesa|dell'ora");
  showWeather_ = false;
  phaseStart_ = millis();
  lastDraw_ = 0;
}

void ClockMode::update(uint32_t now) {
  struct tm t;
  if (!localTime(t)) {
    waiting_.update(now, settings.speedMs);
    return;
  }

  if (!weather.valid) updateWeather();  // retries at most once a minute

  // Alternate clock / weather; skip the weather until there is some.
  if (now - phaseStart_ >= (showWeather_ ? WEATHER_MS : CLOCK_MS)) {
    phaseStart_ = now;
    showWeather_ = !showWeather_ && weather.valid;
    if (!showWeather_) updateWeather();  // refreshes at most every 15 min
  }

  if (now - lastDraw_ < 100) return;
  lastDraw_ = now;
  if (showWeather_) {
    drawWeather();
  } else {
    drawClock(t);
  }
}

void ClockMode::action() {
  updateWeather(true);
  showWeather_ = weather.valid;
  phaseStart_ = millis();
  lastDraw_ = 0;
}

void ClockMode::drawClock(const struct tm &t) {
  display.clear();
  const int values[2] = {t.tm_hour, t.tm_min};
  for (int line = 0; line < 2; line++) {
    const int y = line == 0 ? 1 : 9;
    display.drawBitmap(2, y, BIG_DIGITS[values[line] / 10], 5, 6);
    display.drawBitmap(8, y, BIG_DIGITS[values[line] % 10], 5, 6);
  }
  int x, y;
  borderPixel(t.tm_sec % 60, x, y);
  display.setPixel(x, y, true);
  display.render();
}

void ClockMode::drawWeather() {
  display.clear();
  const Icon &icon = iconFor(weather.code, weather.isDay);
  display.drawBitmap((COLS - icon.width) / 2, 0, icon.rows, icon.width, 9);

  // Temperature on the bottom rows, with a 2x2 degree sign.
  const String temp = String((int)lroundf(weather.temperature));
  const int textW = Display::textWidth(temp.c_str(), 0, temp.length());
  const int x = (COLS - (textW + 2) + 1) / 2;
  display.drawText(x, ROWS - 6, temp.c_str(), 0, temp.length());
  for (int dy = 0; dy < 2; dy++) {
    for (int dx = 0; dx < 2; dx++) display.setPixel(x + textW + dx, ROWS - 6 + dy, true);
  }
  display.render();
}
