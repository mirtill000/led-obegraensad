#pragma once

#include "modes.h"
#include "weather.h"

// Daily forecast, one screen per day: today, then the next 3 days in turn.
// Weekday and date along the top ("VE 26"); below are the day's weather
// icon and its minimum and maximum temperature.
class ForecastMode : public Mode {
 public:
  const char *id() const override { return "forecast"; }
  const char *name() const override { return "Previsioni"; }
  void start() override {
    lastDraw_ = 0;
    dayStart_ = 0;
    day_ = 0;
  }
  void update(uint32_t now) override;
  const char *actionName() const override { return "Aggiorna"; }
  void action() override;
  bool hasSpeed() const override { return false; }

  // "Prossime 12 ore: 12°-18°, ..." (UTF-8) for the page, or "" without data.
  static String summary();

 private:
  uint32_t lastDraw_ = 0;
  uint32_t dayStart_ = 0;  // when the current day's screen started
  int day_ = 0;            // 0 = today
};
