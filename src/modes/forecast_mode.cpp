#include "modes/forecast_mode.h"

#include "display.h"
#include "font_mini.h"
#include "timekeeping.h"
#include "ui.h"
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

static const uint32_t DAY_MS = 5000;  // each day's screen

// Two-letter Italian weekday names (0 = Sunday), so that name and date fit
// on 16 columns without scrolling.
static const char *const DAYS[] = {"DO", "LU", "MA", "ME", "GI", "VE", "SA"};

// Day of the week (0 = Sunday) for a Gregorian date.
static int weekday(int y, int m, int d) {
  static const int T[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y--;
  return (y + y / 4 - y / 100 + y / 400 + T[m - 1] + d) % 7;
}

// Weekday and day of the month of forecast day `day`; false if unknown.
static bool dayDate(const Weather &w, int day, int &wday, int &mday) {
  if (day < w.days && w.dayYear[day] && w.dayMonth[day] >= 1 && w.dayMonth[day] <= 12) {
    wday = weekday(w.dayYear[day], w.dayMonth[day], w.dayOfMonth[day]);
    mday = w.dayOfMonth[day];
    return true;
  }
  struct tm t;
  if (day != 0 || !localTime(t)) return false;
  wday = t.tm_wday;
  mday = t.tm_mday;
  return true;
}

// Header on rows 0-4, not scrolling: "VE" at x0-6 and the day of the month
// right-aligned to x15 ("VE 26"). The mini font's M is 5 pixels wide, so
// a 3-pixel one stands in for it here.
static void drawDay(const Weather &w, int day) {
  int wday, mday;
  if (!dayDate(w, day, wday, mday)) return;
  static const uint8_t NARROW_M[MINI_HEIGHT] = {0xA0, 0xE0, 0xE0, 0xA0, 0xA0};
  for (int i = 0; i < 2; i++) {
    const char c = DAYS[wday][i];
    if (c != 'M') {
      ui::mini(i * 4, 0, String(c));
      continue;
    }
    for (int r = 0; r < MINI_HEIGHT; r++) {
      for (int k = 0; k < 3; k++) {
        if (NARROW_M[r] & (0x80 >> k)) display.setLevel(i * 4 + k, r, 255);
      }
    }
  }
  const String n(mday);
  ui::mini(COLS - ui::miniWidth(n), 0, n);
}

// Temperature with the tens at x8-10, the units at x12-14 and a one-pixel
// degree sign at x15. The minus is 2 pixels wide, one pixel left of the
// first digit; at -10 and below it takes x5-6.
static void drawTemperature(int y, float celsius, uint8_t level) {
  const int t = constrain((int)lroundf(celsius), -99, 99);
  const int value = abs(t);
  // Each digit right-aligned in its 3-column slot (the 1 is narrower).
  const String units(value % 10), tens(value / 10);
  int left = 15 - ui::miniWidth(units);
  ui::mini(left, y, units, level);
  if (value >= 10) {
    left = 11 - ui::miniWidth(tens);
    ui::mini(left, y, tens, level);
  }
  if (t < 0) {
    display.setLevel(left - 3, y + 2, level);
    display.setLevel(left - 2, y + 2, level);
  }
  display.setLevel(15, y, level);
}

// Layout, as in the mockup:
//
//   rows 0-4    weekday and date, e.g. "VE 26" (still)
//   rows 6-15   icon x0-5 (rows 7-13)   min x8-14 rows 6-10, ° x15
//                                        max x8-14 rows 11-15, ° x15
//
// Everything is at full brightness. The screen shows today, then each of
// the next 3 days in turn, 5 s each, with the mode transition in between.
void ForecastMode::update(uint32_t now) {
  if (now - lastDraw_ < 50) return;
  lastDraw_ = now;
  const Weather w = weatherNow();
  if (dayStart_ == 0) dayStart_ = now;
  if (day_ >= max(1, (int)w.days)) day_ = 0;
  if (now - dayStart_ >= DAY_MS && w.days > 1) {
    day_ = (day_ + 1) % w.days;
    dayStart_ = now;
    display.beginTransition();
  }
  display.clear();
  drawDay(w, day_);
  if (!w.valid || w.days == 0) {
    ui::waiting(now, 10);
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
