#pragma once

#include "constants.h"
#include "modes.h"

// Slow ambient animations. settings.ambient picks one, or "auto": a new one
// every few minutes during the day and quiet stars at night.
class AmbientMode : public Mode {
 public:
  const char *id() const override { return "ambient"; }
  const char *name() const override { return "Animazioni"; }
  void start() override;
  void update(uint32_t now) override;
  const char *actionName() const override { return "Prossima animazione"; }
  void action() override;

  struct Animation {
    const char *id;
    const char *name;
  };
  static const Animation ANIMATIONS[];
  static const uint8_t ANIMATION_COUNT;

 private:
  void begin(uint8_t animation);
  uint8_t pickAuto() const;

  void rain(uint32_t now);
  void fire();
  void stars();
  void waves(uint32_t now);
  void breath(uint32_t now);

  uint8_t animation_ = 0;
  uint8_t autoIndex_ = 0;
  uint32_t animationStart_ = 0;
  uint32_t lastFrame_ = 0;

  // Per-animation state.
  struct Drop {
    float y;
    float speed;
    uint8_t length;
  } drops_[COLS];
  uint8_t heat_[ROWS][COLS];
  uint8_t starLife_[ROWS][COLS];  // frames left
  uint8_t starSpan_[ROWS][COLS];  // total frames, to fade in and out
};
