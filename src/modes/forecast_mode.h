#pragma once

#include "modes.h"
#include "scroller.h"

// The next 12 hours: a temperature curve on top and rain-probability bars
// at the bottom, one column per hour, alternating with a scrolling summary
// (range, rain, sunrise and sunset).
class ForecastMode : public Mode {
 public:
  const char *id() const override { return "forecast"; }
  const char *name() const override { return "Previsioni"; }
  void start() override;
  void update(uint32_t now) override;
  const char *actionName() const override { return "Aggiorna"; }
  void action() override;

  // "Prossime 12 ore: 12°-18°, ..." (UTF-8), or "" without data.
  static String summary();

 private:
  void drawChart();

  bool chart_ = true;
  uint32_t phaseStart_ = 0;
  uint32_t lastDraw_ = 0;
  Scroller scroller_;
};
