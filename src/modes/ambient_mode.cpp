#include "modes/ambient_mode.h"

#include "display.h"
#include "settings.h"
#include "timekeeping.h"

static const uint32_t AUTO_SWITCH_MS = 5 * 60 * 1000;

// Next animation in the "auto" rotation, skipping clocks until the time is
// known.
Animation *AmbientMode::pickAuto() {
  struct tm t;
  const bool haveTime = localTime(t);
  for (uint8_t tries = 0; tries < ANIMATION_COUNT; tries++) {
    Animation *a = ANIMATIONS[autoIndex_ % ANIMATION_COUNT];
    if (!a->needsTime() || haveTime) return a;
    autoIndex_++;
  }
  return ANIMATIONS[0];
}

void AmbientMode::play(Animation *animation) {
  animation_ = animation;
  since_ = millis();
  lastFrame_ = 0;
  display.beginTransition();
  display.clear();
  animation_->start();
}

void AmbientMode::start() {
  Animation *chosen = override_ ? findAnimation(override_) : findAnimation(settings.ambient);
  play(chosen ? chosen : pickAuto());
}

bool AmbientMode::setOverride(const char *animationId) {
  const bool changed = (override_ == nullptr) != (animationId == nullptr) ||
                       (override_ && animationId && strcmp(override_, animationId) != 0);
  override_ = animationId;
  return changed;
}

void AmbientMode::action() {
  if (override_) return;  // the night schedule decides
  if (!findAnimation(settings.ambient)) {
    autoIndex_++;
    play(pickAuto());
    return;
  }
  // A fixed animation was chosen: move the choice on to the next one.
  uint8_t i = 0;
  while (i < ANIMATION_COUNT && ANIMATIONS[i] != animation_) i++;
  Animation *next = ANIMATIONS[(i + 1) % ANIMATION_COUNT];
  settings.ambient = next->id();
  saveSettings();
  play(next);
}

bool AmbientMode::demoForced() const { return override_ || !findAnimation(settings.ambient); }

bool AmbientMode::input(char key) {
  if (!animation_ || !animation_->isGame() || demoForced() || demoMode(animation_->id())) return false;
  animation_->input(key);
  return true;
}

void AmbientMode::update(uint32_t now) {
  if (!animation_) start();
  if (!override_ && !findAnimation(settings.ambient) && now - since_ >= AUTO_SWITCH_MS) {
    autoIndex_++;
    play(pickAuto());
  }
  if (now - lastFrame_ < interval(animation_->frameMs())) return;
  lastFrame_ = now;
  if (animation_->isGame()) animation_->setDemo(demoForced() || demoMode(animation_->id()));
  animation_->frame(now);
  display.render();
}
