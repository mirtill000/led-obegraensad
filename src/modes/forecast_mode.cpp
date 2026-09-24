#include "modes/forecast_mode.h"

#include "display.h"
#include "font_mini.h"
#include "settings.h"
#include "timekeeping.h"
#include "weather.h"
#include "weather_icons.h"

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

static const uint32_t HEADER_STEP_MS = 90;  // header scroll: 1 pixel per step
static const int HEADER_GAP = 8;             // blank pixels between repeats

static const char *const DAYS[] = {"DOM", "LUN", "MAR", "MER", "GIO", "VEN", "SAB"};
static const char *const MONTHS[] = {"GEN", "FEB", "MAR", "APR", "MAG", "GIU",
                                     "LUG", "AGO", "SET", "OTT", "NOV", "DIC"};

// "MILANO  MER 24 SET" (city only until the clock is set).
static String header() {
  String s = Display::fontText(settings.city);
  struct tm t;
  if (localTime(t)) s += String("  ") + DAYS[t.tm_wday] + " " + t.tm_mday + " " + MONTHS[t.tm_mon];
  return s;
}

// Mini-font text limited to rows 0-4, scrolling in a loop when it's wider
// than the display, centred otherwise.
static void drawHeader(uint32_t now) {
  const String text = header();
  const int width = miniWidth(text);
  if (width <= COLS) {
    drawMini((COLS - width) / 2, 0, text, 255);
    return;
  }
  const int period = width + HEADER_GAP;
  const int x = -(int)((now / HEADER_STEP_MS) % period);
  drawMini(x, 0, text, 255);
  drawMini(x + period, 0, text, 255);
}

// Temperature with the tens at x8-10, the units at x12-14 and a one-pixel
// degree sign at x15. The minus is 2 pixels wide, one pixel left of the
// first digit; at -10 and below it takes x5-6.
static void drawTemperature(int y, float celsius, uint8_t level) {
  const int t = constrain((int)lroundf(celsius), -99, 99);
  const int value = abs(t);
  int left = 12;
  if (value >= 10) {
    drawMini(8, y, String(value / 10), level);
    left = 8;
  }
  drawMini(12, y, String(value % 10), level);
  if (t < 0) {
    display.setLevel(left - 3, y + 2, level);
    display.setLevel(left - 2, y + 2, level);
  }
  display.setLevel(15, y, level);
}

// Layout, as in the mockup:
//
//   rows 0-4    city and date, scrolling
//   rows 6-15   icon x0-5 (rows 7-13)   min x8-14 rows 6-10, ° x15
//                                                 max x8-14 rows 11-15, ° x15
//
// Everything is at full brightness.
void ForecastMode::update(uint32_t now) {
  if (now - lastDraw_ < 50) return;
  lastDraw_ = now;
  const Weather w = weatherNow();
  display.clear();
  drawHeader(now);
  if (!w.valid || !w.hasDaily) {
    for (int i = 0; i < 3; i++) display.setLevel(5 + i * 3, 10, 255);  // waiting: "..."
    display.render();
    return;
  }

  // The day's weather (daytime icon); the current one if the API didn't
  // send it.
  const AnimatedIcon &icon = w.todayCode >= 0 ? iconFor(w.todayCode, true) : iconFor(w.code, w.isDay);
  display.drawBitmap(0, 7, icon.frames[(now / icon.frameMs) % icon.frameCount], 6, 7);


  drawTemperature(6, w.todayMin, 255);
  drawTemperature(11, w.todayMax, 255);
  display.render();
}
