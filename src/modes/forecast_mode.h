#pragma once

#include "modes.h"

// Today at a glance, on one screen:
//  - top: sunrise and sunset in turn (every 4 s): a sun on the horizon with
//    a triangle pointing up (sunrise) or down (sunset), the time below;
//  - bottom: today's minimum (dimmer) and maximum temperature, and a
//    falling drop when rain is likely (>= 50%).
class ForecastMode : public Mode {
 public:
  const char *id() const override { return "forecast"; }
  const char *name() const override { return "Previsioni"; }
  void start() override { lastDraw_ = 0; }
  void update(uint32_t now) override;
  const char *actionName() const override { return "Aggiorna"; }
  void action() override;
  bool hasSpeed() const override { return false; }

  // "Prossime 12 ore: 12°-18°, ..." (UTF-8) for the page, or "" without data.
  static String summary();

 private:
  uint32_t lastDraw_ = 0;
};
