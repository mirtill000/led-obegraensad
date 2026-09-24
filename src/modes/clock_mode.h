#pragma once

#include "modes.h"

// Clock and weather on one screen: hours and minutes on the left, an
// animated weather icon and the temperature on the right, and a dot running
// round the border for the seconds.
class ClockMode : public Mode {
 public:
  const char *id() const override { return "clock"; }
  const char *name() const override { return "Orologio e meteo"; }
  void start() override;
  void update(uint32_t now) override;
  const char *actionName() const override { return "Aggiorna meteo"; }
  void action() override;
  bool hasSpeed() const override { return false; }

 private:
  uint32_t lastDraw_ = 0;
};
