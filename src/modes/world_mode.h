#pragma once

#include "modes.h"
#include "scroller.h"

// "Mondo": the air where you are, the Space Station, the next rocket (see
// world.h), one after the other: "Aria" with the European index and its
// band; the world map with the station and its trail (you are the steady
// dot; "ISS" above it with the lamp vertical); a rocket lifting off, then
// the launch's name and countdown scrolling by.
class WorldMode : public Mode {
 public:
  const char *id() const override { return "world"; }
  const char *name() const override { return "Mondo"; }
  void start() override;
  void update(uint32_t now) override;
  const char *actionName() const override { return "Prossima"; }
  void action() override { next(millis()); }

 private:
  enum Scene : uint8_t { AIR, ISS, LAUNCH, SCENES };
  void next(uint32_t now);
  void drawPicture(uint32_t now);

  Scroller scroller_;
  uint8_t scene_ = SCENES;
  bool picture_ = true;  // picture first, then the text
  uint32_t sceneStart_ = 0, lastDraw_ = 0;
  int row_ = -1;
};
