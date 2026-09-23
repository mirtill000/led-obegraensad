#pragma once

#include "constants.h"
#include "modes.h"
#include "scroller.h"

// Side-scrolling platformer in the style of Super Mario: the level scrolls
// by with pipes, pits, goombas and coins. Plays by itself (an autopilot
// jumps when needed); the "Salta" button on the web page takes over for a
// while, until nobody has pressed it for MANUAL_MS.
class MarioMode : public Mode {
 public:
  const char *id() const override { return "mario"; }
  const char *name() const override { return "Super Mario"; }
  void start() override;
  void update(uint32_t now) override;
  const char *actionName() const override { return "Salta"; }
  void action() override;

  // World column heights live in a ring buffer this long; it must cover
  // the screen plus the autopilot's look-ahead.
  static const int RING = 64;
  static const int MAX_GOOMBAS = 6;
  static const int MAX_COINS = 12;

  struct Goomba {
    float x;
    float dir;
    bool alive;
  };
  struct Coin {
    int32_t x;
    int8_t y;
    bool taken;
  };
  // Everything the physics touches, so the autopilot can copy it and
  // simulate ahead.
  struct State {
    uint8_t top[RING];  // highest solid row per world column; 16 = pit
    int32_t generatedTo;
    int32_t cam;  // world x of the leftmost screen column
    float y, vy;  // Mario's top row and vertical speed
    bool onGround;
    uint16_t score;
    Goomba goombas[MAX_GOOMBAS];
    Coin coins[MAX_COINS];
  };

 private:
  enum Phase { PLAYING, DYING, SCORE };

  void extendWorld();
  bool shouldJump() const;
  void draw(uint32_t now);

  State s_;
  Phase phase_ = PLAYING;
  int flatLeft_ = 0;
  uint32_t lastFrame_ = 0;
  uint32_t manualUntil_ = 0;
  uint32_t frame_ = 0;
  float deathY_ = 0, deathVy_ = 0;
  Scroller score_;
};
