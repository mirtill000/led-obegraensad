#pragma once

#include "modes.h"

// "Orologio": by default clock and weather on one screen - hours and
// minutes on the left, an animated weather icon and the temperature on the
// right, a dot running round the border for the seconds. settings.clockStyle
// picks another face instead: binary, in Italian words, in English words
// (the clock-face animations in src/animations/clocks.cpp).
class ClockMode : public Mode {
 public:
  const char *id() const override { return "clock"; }
  const char *name() const override { return "Orologio"; }
  void start() override;
  void update(uint32_t now) override;
  const char *actionName() const override { return "Aggiorna meteo"; }
  void action() override;
  bool hasSpeed() const override { return false; }

 private:
  uint32_t lastDraw_ = 0;
  Animation *face_ = nullptr;  // the face on show, nullptr = clock and weather
};
