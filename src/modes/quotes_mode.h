#pragma once

#include "modes.h"
#include "scroller.h"

// A different motivational quote every hour, scrolling on one line. The
// quote only changes at the end of a pass, so it is never cut off.
class QuotesMode : public Mode {
 public:
  const char *id() const override { return "quotes"; }
  const char *name() const override { return "Frase dell'ora"; }
  void start() override;
  void update(uint32_t now) override;
  const char *actionName() const override { return "Prossima frase"; }
  void action() override;

 private:
  // Index of the quote for the current hour (plus any skips).
  uint16_t currentIndex() const;

  Scroller scroller_;
  uint16_t shown_ = 0;
  uint16_t skip_ = 0;  // "next quote" presses
};
