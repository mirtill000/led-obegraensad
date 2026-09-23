#pragma once

#include "modes.h"
#include "scroller.h"

// Scrolls settings.text across the middle of the panel on one line,
// looping forever.
class TextMode : public Mode {
 public:
  const char *id() const override { return "text"; }
  const char *name() const override { return "Testo scorrevole"; }
  void start() override;
  void update(uint32_t now) override;

 private:
  Scroller scroller_;
};
