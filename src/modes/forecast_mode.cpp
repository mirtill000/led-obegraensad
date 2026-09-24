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

// Day of the week (0 = Sunday) for a Gregorian date.
static int weekday(int y, int m, int d) {
  static const int T[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y--;
  return (y + y / 4 - y / 100 + y / 400 + T[m - 1] + d) % 7;
}

// "MILANO  MER 24 SET" for forecast day `day` (city only without a date).
static String header(const Weather &w, int day) {
  String s = Display::fontText(settings.city);
  if (day < w.days && w.dayYear[day] && w.dayMonth[day] >= 1 && w.dayMonth[day] <= 12) {
    s += String("  ") + DAYS[weekday(w.dayYear[day], w.dayMonth[day], w.dayOfMonth[day])] + " " +
         w.dayOfMonth[day] + " " + MONTHS[w.dayMonth[day] - 1];
  } else if (day == 0) {
    struct tm t;
    if (localTime(t)) s += String("  ") + DAYS[t.tm_wday] + " " + t.tm_mday + " " + MONTHS[t.tm_mon];
  }
  return s;
}

// Scrolls the current day's header along rows 0-4, with the next day's
// coming in behind it. Returns true once that one has reached x0, i.e.
// when the screen should move on to the next day.
bool ForecastMode::drawHeader(const Weather &w, uint32_t now) {
  const int next = w.days > 0 ? (day_ + 1) % w.days : 0;
  const String text = header(w, day_);
  const int period = miniWidth(text) + HEADER_GAP;
  const int offset = (now - dayStart_) / HEADER_STEP_MS;
  if (offset >= period) return true;
  drawMini(-offset, 0, text, 255);
  drawMini(period - offset, 0, header(w, next), 255);
  return false;
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
//                                        max x8-14 rows 11-15, ° x15
//
// Everything is at full brightness. The screen shows today, then each of
// the next 3 days in turn, moving on each time the header has scrolled by.
void ForecastMode::update(uint32_t now) {
  if (now - lastDraw_ < 50) return;
  lastDraw_ = now;
  const Weather w = weatherNow();
  if (dayStart_ == 0) dayStart_ = now;
  if (day_ >= max(1, (int)w.days)) day_ = 0;
  display.clear();
  if (drawHeader(w, now)) {
    day_ = w.days > 0 ? (day_ + 1) % w.days : 0;
    dayStart_ = now;
    display.clear();
    drawHeader(w, now);
  }
  if (!w.valid || w.days == 0) {
    for (int i = 0; i < 3; i++) display.setLevel(5 + i * 3, 10, 255);  // waiting: "..."
    display.render();
    return;
  }

  // The day's weather (daytime icon); the current one if the API didn't
  // send a daily code.
  const AnimatedIcon &icon = w.dayCode[day_] >= 0 ? iconFor(w.dayCode[day_], true)
                                                  : iconFor(w.code, day_ == 0 ? w.isDay : true);
  display.drawBitmap(0, 7, icon.frames[(now / icon.frameMs) % icon.frameCount], 6, 7);

  drawTemperature(6, w.dayMin[day_], 255);
  drawTemperature(11, w.dayMax[day_], 255);
  display.render();
}
