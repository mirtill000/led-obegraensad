#pragma once

#include "animation.h"
#include "modes.h"

// Plays the animations in src/animations/, in two instances: "Animazioni"
// (everything but the games, picked by settings.ambient) and "Giochi" (only
// the games, picked by settings.game). The setting names one, or "auto": a
// different one every few minutes. The night schedule can force one (stars)
// on the Animazioni instance through setOverride().
class AmbientMode : public Mode {
 public:
  explicit AmbientMode(bool games) : games_(games) {}
  const char *id() const override { return games_ ? "games" : "ambient"; }
  const char *name() const override { return games_ ? "Giochi" : "Animazioni"; }
  void start() override;
  void update(uint32_t now) override;
  bool setPick(const String &id) override;
  const char *actionName() const override { return games_ ? "Prossimo gioco" : "Prossima animazione"; }
  void action() override;

  // Forces an animation by id (nullptr = back to the setting); returns true
  // if that changed anything.
  bool setOverride(const char *animationId);
  // The animation being shown, or nullptr.
  const Animation *playing() const { return animation_; }

  bool input(char key) override;
  const char *gameId() const override { return animation_ && animation_->isGame() ? animation_->id() : nullptr; }
  // Games shown by "auto" or by the night schedule always run as demos.
  bool demoForced() const;

 private:
  void play(Animation *animation);
  Animation *pickAuto();
  // Whether `a` belongs to this instance (games or not).
  bool mine(const Animation *a) const { return a && !a->isClockFace() && a->isGame() == games_ && a->available(); }
  String &choice() const { return games_ ? settings.game : settings.ambient; }
  bool autoRotation() const;

  const bool games_;
  mutable String checked_;
  mutable bool isAuto_ = true;

  Animation *animation_ = nullptr;
  const char *override_ = nullptr;
  String pick_;  // from the playlist (setPick), "" none
  uint8_t autoIndex_ = 0;
  uint32_t since_ = 0;
  uint32_t lastFrame_ = 0;
  // The animation's clock (ms, run by the speed setting) and, for
  // fixedStep() ones, how far its steps have got.
  uint32_t clock_ = 0, stepClock_ = 0;
  float clockCarry_ = 0;
};
