#pragma once

#include "modes.h"

// "Mondo": two cards taking turns, cross-fading into each other (see
// world.h for the data): "Aria" with the European index and its band, and
// the world map with the Space Station and its trail (you are the steady
// dot; "ISS" above it with the lamp vertical).
class WorldMode : public Mode {
 public:
  const char *id() const override { return "world"; }
  const char *name() const override { return "Mondo"; }
  void start() override;
  void update(uint32_t now) override;
  bool hasSpeed() const override { return false; }
  const char *actionName() const override { return "Prossima"; }
  String status() const override;
  void action() override { next(millis(), true); }

 private:
  enum Card : uint8_t { AIR, ISS, CARDS };
  void next(uint32_t now, bool fade);

  uint8_t card_ = AIR;
  uint32_t cardStart_ = 0, lastDraw_ = 0;
};
