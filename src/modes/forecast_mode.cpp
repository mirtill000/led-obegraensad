#include "modes/forecast_mode.h"

#include "display.h"
#include "gfx.h"
#include "weather.h"

static const uint32_t CHART_MS = 10000;
static const int FIRST_X = 2;  // hours at columns 2-13

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

void ForecastMode::start() {
  chart_ = true;
  phaseStart_ = millis();
  lastDraw_ = 0;
}

void ForecastMode::action() {
  requestWeatherUpdate();
  start();
}

void ForecastMode::update(uint32_t now) {
  if (chart_) {
    if (now - phaseStart_ >= CHART_MS) {
      const String text = summary();
      scroller_.start(text.length() ? text : String("Previsioni in arrivo..."));
      chart_ = false;
      return;
    }
    if (now - lastDraw_ < 200) return;
    lastDraw_ = now;
    drawChart();
  } else if (scroller_.update(now, interval(SCROLL_DELAY_MS))) {
    start();
  }
}

void ForecastMode::drawChart() {
  const Weather w = weatherNow();
  display.clear();
  if (!w.valid || w.hours == 0) {
    for (int i = 0; i < 3; i++) display.setLevel(5 + i * 3, 8, 120);  // waiting: "..."
    display.render();
    return;
  }
  float lo = w.hourlyTemp[0], hi = lo;
  for (int i = 0; i < w.hours; i++) {
    lo = min(lo, w.hourlyTemp[i]);
    hi = max(hi, w.hourlyTemp[i]);
  }
  if (hi - lo < 4) {  // flat days still get a readable curve
    const float mid = (hi + lo) / 2;
    lo = mid - 2;
    hi = mid + 2;
  }
  // Rain probability: bars on rows 10-15, the top pixel partly lit.
  for (int i = 0; i < w.hours; i++) {
    const float h = w.hourlyRain[i] / 100.0f * 6;
    for (int r = 0; r < 6; r++) {
      const float f = h - r;
      if (f > 0) gfx::plot(FIRST_X + i, 15 - r, 0.35f * fminf(f, 1));
    }
  }
  // Hour marks every 3 hours, faint, between the two areas.
  for (int i = 0; i < w.hours; i += 3) gfx::plot(FIRST_X + i, 9, 0.08f);
  // Temperature: antialiased line over rows 1-8 (warmest on top).
  float px = 0, py = 0;
  for (int i = 0; i < w.hours; i++) {
    const float x = FIRST_X + i, y = 8 - (w.hourlyTemp[i] - lo) / (hi - lo) * 7;
    if (i) gfx::line(px, py, x, y, 1.0f);
    px = x;
    py = y;
  }
  if (w.hours == 1) gfx::plot(FIRST_X, (int)py, 1);
  display.render();
}
