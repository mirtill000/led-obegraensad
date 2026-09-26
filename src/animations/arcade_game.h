#pragma once

#include "animation.h"
#include "scroller.h"

// Common plumbing for the arcade games (arcade.cpp, runner.cpp, kong.cpp):
// the "Giochi" group, demo mode and, at game over, the score scrolling by
// before the game starts again. Subclasses implement tick() and call
// gameOver(points).

// "Punti 12" scrolling once after a game over.
class ScoreScreen {
 public:
  void show(const String &text) {
    scroller_.start(text);
    active_ = true;
  }
  bool active() const { return active_; }
  // True when it has finished (the game should restart).
  bool update(uint32_t now) {
    if (!active_ || !scroller_.update(now, 70)) return false;
    active_ = false;
    return true;
  }

 private:
  Scroller scroller_;
  bool active_ = false;
};

// Base class of the arcade games.
class ArcadeGame : public Animation {
 public:
  const char *group() const override { return "Giochi"; }
  bool isGame() const override { return true; }
  void setDemo(bool demo) override { demo_ = demo; }

  void frame(uint32_t now) override {
    if (score_.active()) {
      if (score_.update(now)) start();
      return;
    }
    tick(now);
  }

 protected:
  virtual void tick(uint32_t now) = 0;
  void gameOver(int points) { score_.show(String("Punti ") + points); }
  bool demo_ = true;

 private:
  ScoreScreen score_;
};
