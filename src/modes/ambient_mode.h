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
  bool mine(const Animation *a) const { return a && a->isGame() == games_; }
  String &choice() const { return games_ ? settings.game : settings.ambient; }
  bool autoRotation() const;

  const bool games_;
  mutable String checked_;
  mutable bool isAuto_ = true;

  Animation *animation_ = nullptr;
  const char *override_ = nullptr;
  uint8_t autoIndex_ = 0;
  uint32_t since_ = 0;
  uint32_t lastFrame_ = 0;
};
