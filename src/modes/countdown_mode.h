#pragma once

#include "modes.h"
#include "scroller.h"

// Countdown to a date (settings.countdownDate/Time/Label): the days left in
// big digits, alternating with "Mancano 12 giorni a Vacanze" scrolling by.
class CountdownMode : public Mode {
 public:
  const char *id() const override { return "countdown"; }
  const char *name() const override { return "Conto alla rovescia"; }
  void start() override;
  void update(uint32_t now) override;

  // "Mancano 12 giorni a Vacanze" (UTF-8), or why there's nothing to count.
  static String sentence();

 private:
  bool number_ = true;
  uint32_t phaseStart_ = 0;
  uint32_t lastDraw_ = 0;
  Scroller scroller_;
  int row_ = -1;
};
