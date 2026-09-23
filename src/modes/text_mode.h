#pragma once

#include "modes.h"

// Scrolls settings.text across the panel, looping forever.
class TextMode : public Mode {
 public:
  const char *id() const override { return "text"; }
  const char *name() const override { return "Testo scorrevole"; }
  void start() override;
  void update(uint32_t now) override;

 private:
  int offset_ = 0;
  int width_ = 0;
  uint32_t lastStep_ = 0;
};
