#include "modes/forecast_mode.h"

#include "display.h"
#include "font_mini.h"
#include "weather.h"

static const uint32_t SUN_SWITCH_MS = 4000;  // sunrise <-> sunset
static const int RAIN_LIKELY = 50;           // % for the drop to show

static String clock(int minutes) { return String(minutes / 60) + ":" + (minutes % 60 < 10 ? "0" : "") + (minutes % 60); }

String ForecastMode::summary() {
  const Weather w = weatherNow();
  if (!w.valid || w.hours == 0) return "";
  float lo = w.hourlyTemp[0], hi = lo;
  int rain = 0, rainAt = 0;
  for (int i = 0; i < w.hours; i++) {
    lo = min(lo, w.hourlyTemp[i]);
    hi = max(hi, w.hourlyTemp[i]);
    if (w.hourlyRain[i] > rain) {
      rain = w.hourlyRain[i];
      rainAt = (w.firstHour + i) % 24;
    }
  }
  String s = String("Prossime 12 ore: ") + (int)lroundf(lo) + "°-" + (int)lroundf(hi) + "°, ";
  s += rain >= 20 ? String("pioggia fino al ") + rain + "% alle " + rainAt : String("niente pioggia");
  if (w.sunrise >= 0 && w.sunset >= 0) s += String(", alba ") + clock(w.sunrise) + ", tramonto " + clock(w.sunset);
  return s;
}

void ForecastMode::action() { requestWeatherUpdate(); }

// Mini-font text at a brightness level; returns the x after it.
static int drawMini(int x, int y, const String &text, uint8_t level) {
  for (unsigned i = 0; i < text.length(); i++) {
    const MiniGlyph *g = findMiniGlyph(text[i]);
    for (int r = 0; r < MINI_HEIGHT; r++) {
      for (int c = 0; c < g->width; c++) {
        if (g->rows[r] & (0x80 >> c)) display.setLevel(x + c, y + r, level);
      }
    }
    x += g->width + 1;
  }
  return x;
}

static int miniWidth(const String &text) {
  int w = 0;
  for (unsigned i = 0; i < text.length(); i++) w += findMiniGlyph(text[i])->width + 1;
  return w - 1;
}

// H:MM centred, the colon as two dim dots in the gap after the hour (a
// separate colon column would not fit "19:25" in 16 pixels).
static void drawTime(int y, int minutes) {
  const String h(minutes / 60);
  const String m = String(minutes % 60 < 10 ? "0" : "") + (minutes % 60);
  const int width = miniWidth(h) + 1 + miniWidth(m);
  int x = (COLS - width) / 2;
  x = drawMini(x, y, h, 255);
  display.setLevel(x - 1, y + 1, 110);
  display.setLevel(x - 1, y + 3, 110);
  drawMini(x, y, m, 255);
}

// Sun half above the horizon (rows 0-3) with a triangle: up = sunrise,
// down = sunset.
static void drawSunIcon(bool rising) {
  static const char *SUN[3] = {"..###..", ".#####.", "#######"};
  for (int y = 0; y < 3; y++) {
    for (int x = 0; x < 7; x++) {
      if (SUN[y][x] == '#') display.setLevel(5 + x, y, 255);
    }
  }
  for (int x = 0; x < COLS - 1; x++) display.setLevel(x, 3, 55);  // horizon
  const int top = rising ? 0 : 1;
  if (rising) {
    display.setLevel(2, top, 170);
    for (int x = 1; x <= 3; x++) display.setLevel(x, top + 1, 170);
  } else {
    for (int x = 1; x <= 3; x++) display.setLevel(x, top, 170);
    display.setLevel(2, top + 1, 170);
  }
}

void ForecastMode::update(uint32_t now) {
  if (now - lastDraw_ < 100) return;
  lastDraw_ = now;
  const Weather w = weatherNow();
  display.clear();
  if (!w.valid || w.sunrise < 0 || w.sunset < 0 || !w.hasDaily) {
    for (int i = 0; i < 3; i++) display.setLevel(5 + i * 3, 8, 120);  // waiting: "..."
    display.render();
    return;
  }

  const bool rising = (now / SUN_SWITCH_MS) % 2 == 0;
  drawSunIcon(rising);
  drawTime(5, rising ? w.sunrise : w.sunset);

  // Minimum (dimmer) and maximum, bottom left.
  int x = drawMini(0, 11, String((int)lroundf(w.todayMin)), 110);
  drawMini(x + 1, 11, String((int)lroundf(w.todayMax)), 255);

  // Rain likely today: a drop falling in the bottom-right corner.
  if (w.todayRain >= RAIN_LIKELY) {
    static const char *DROP[] = {".#.", "###", "###", ".#."};
    const int y0 = 11 + (now / 300) % 2;
    for (int y = 0; y < 4; y++) {
      for (int c = 0; c < 3; c++) {
        if (DROP[y][c] == '#') display.setLevel(13 + c, y0 + y, 200);
      }
    }
  }
  display.render();
}
