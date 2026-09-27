#pragma once

#include "constants.h"
#include "modes.h"

// Conway's Game of Life on a 16x16 torus (edges wrap around). Each game
// starts from an empty board with just a handful of cells in the middle - a
// small "methuselah" pattern that grows into a lot of activity - and starts
// over with another one when the board dies out, freezes or loops. A game
// can also start from a pattern drawn by hand on the Lavagna.
class LifeMode : public Mode {
 public:
  const char *id() const override { return "life"; }
  const char *name() const override { return "Gioco della vita"; }
  void start() override;
  void update(uint32_t now) override;
  const char *actionName() const override { return "Ricomincia"; }
  void action() override { start(); }
  // The next start() begins from these cells instead of a random pattern
  // (the Lavagna's drawing).
  static void seedWith(const bool cells[ROWS][COLS]);

 private:
  void seed();
  void step();
  void draw();
  uint32_t hash() const;

  bool cells_[ROWS][COLS];
  bool previous_[ROWS][COLS];  // last generation: cells that just died glow faintly
  uint32_t history_[4];  // hashes of recent generations, to spot loops
  uint16_t generation_ = 0;
  uint32_t lastStep_ = 0;
};
