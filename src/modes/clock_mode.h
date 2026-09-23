#pragma once

#include "modes.h"
#include "scroller.h"

// Clock (hours on top, minutes below, a dot running round the border for
// the seconds), alternating with the current weather: an icon and the
// temperature.
class ClockMode : public Mode {
 public:
  const char *id() const override { return "clock"; }
  const char *name() const override { return "Orologio e meteo"; }
  void start() override;
  void update(uint32_t now) override;
  const char *actionName() const override { return "Aggiorna meteo"; }
  void action() override;

 private:
  void drawClock(const struct tm &t);
  void drawWeather();

  Scroller waiting_;  // shown until the clock has synced
  bool showWeather_ = false;
  uint32_t phaseStart_ = 0;
  uint32_t lastDraw_ = 0;
};
