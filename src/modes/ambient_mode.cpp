#include "modes/ambient_mode.h"

#include "display.h"
#include "settings.h"
#include "timekeeping.h"

static const uint32_t AUTO_SWITCH_MS = 5 * 60 * 1000;

// Next animation in the "auto" rotation (of this instance's kind),
// skipping clocks until the time is known.
Animation *AmbientMode::pickAuto() {
  struct tm t;
  const bool haveTime = localTime(t);
  for (uint8_t tries = 0; tries < ANIMATION_COUNT; tries++) {
    Animation *a = ANIMATIONS[autoIndex_ % ANIMATION_COUNT];
    if (mine(a) && (!a->needsTime() || haveTime)) return a;
    autoIndex_++;
  }
  for (uint8_t i = 0; i < ANIMATION_COUNT; i++) {
    if (mine(ANIMATIONS[i])) return ANIMATIONS[i];
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

// "auto" (or an id that no longer exists, or of the other kind): they take
// turns. Asked on every loop(), so the lookup is redone only when the
// setting changes.
bool AmbientMode::autoRotation() const {
  if (checked_ != choice()) {
    checked_ = choice();
    isAuto_ = !mine(findAnimation(checked_));
  }
  return isAuto_;
}

void AmbientMode::start() {
  Animation *chosen = override_ ? findAnimation(override_) : autoRotation() ? nullptr : findAnimation(choice());
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
  if (autoRotation()) {
    autoIndex_++;
    play(pickAuto());
    return;
  }
  // A fixed one was chosen: move the choice on to the next of its kind.
  uint8_t i = 0;
  while (i < ANIMATION_COUNT && ANIMATIONS[i] != animation_) i++;
  Animation *next = animation_;
  for (uint8_t k = 1; k <= ANIMATION_COUNT; k++) {
    next = ANIMATIONS[(i + k) % ANIMATION_COUNT];
    if (mine(next)) break;
  }
  choice() = next->id();
  saveSettings();
  play(next);
}

bool AmbientMode::demoForced() const { return override_ || autoRotation(); }

bool AmbientMode::input(char key) {
  if (!animation_ || !animation_->isGame() || demoForced() || demoMode(animation_->id())) return false;
  animation_->input(key);
  return true;
}

void AmbientMode::update(uint32_t now) {
  if (!animation_) start();
  if (!override_ && autoRotation() && now - since_ >= AUTO_SWITCH_MS) {
    autoIndex_++;
    play(pickAuto());
  }
  if (now - lastFrame_ < interval(animation_->frameMs())) return;
  lastFrame_ = now;
  if (animation_->isGame()) animation_->setDemo(demoForced() || demoMode(animation_->id()));
  animation_->frame(now);
  display.render();
}
