#pragma once

#include "display.h"
#include "modes.h"

// All LEDs off.
class OffMode : public Mode {
 public:
  const char *id() const override { return "off"; }
  const char *name() const override { return "Spento"; }
  void start() override {
    display.clear();
    display.render();
  }
  void update(uint32_t) override {}
  bool hasSpeed() const override { return false; }
};
