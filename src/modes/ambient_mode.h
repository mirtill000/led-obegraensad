#pragma once

#include "animation.h"
#include "modes.h"

// Plays one of the animations in src/animations/. settings.ambient picks
// one, or "auto": a different one every few minutes. The night schedule can
// force one (stars) through setOverride().
class AmbientMode : public Mode {
 public:
  const char *id() const override { return "ambient"; }
  const char *name() const override { return "Animazioni"; }
  void start() override;
  void update(uint32_t now) override;
  const char *actionName() const override { return "Prossima animazione"; }
  void action() override;

  // Forces an animation by id (nullptr = back to the setting); returns true
  // if that changed anything.
  bool setOverride(const char *animationId);
  // The animation being shown, or nullptr.
  const Animation *playing() const { return animation_; }

 private:
  void play(Animation *animation);
  Animation *pickAuto();

  Animation *animation_ = nullptr;
  const char *override_ = nullptr;
  uint8_t autoIndex_ = 0;
  uint32_t since_ = 0;
  uint32_t lastFrame_ = 0;
};
