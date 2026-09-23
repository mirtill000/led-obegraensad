#pragma once

#include "modes.h"
#include "scroller.h"

// Scrolls settings.text across the panel on one line, looping forever, at
// the height set in settings.textPosition.
class TextMode : public Mode {
 public:
  const char *id() const override { return "text"; }
  const char *name() const override { return "Testo scorrevole"; }
  void start() override;
  void update(uint32_t now) override;

 private:
  Scroller scroller_;
  int row_ = -1;
};
