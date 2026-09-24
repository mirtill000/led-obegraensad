#pragma once

#include "modes.h"

// Countdown to a date (settings.countdownDate/Time/Label): the event and
// when scroll in the header band, the days left sit below in big digits
// (hours:minutes on the day itself).
class CountdownMode : public Mode {
 public:
  const char *id() const override { return "countdown"; }
  const char *name() const override { return "Conto alla rovescia"; }
  void start() override;
  void update(uint32_t now) override;
  bool hasSpeed() const override { return false; }

  // "Mancano 12 giorni a Vacanze" (UTF-8), or why there's nothing to count.
  static String sentence();

 private:
  uint32_t lastDraw_ = 0;
};
