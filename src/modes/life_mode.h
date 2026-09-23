#pragma once

#include "constants.h"
#include "modes.h"

// Conway's Game of Life on a 16x16 torus (edges wrap around). Starts from a
// random soup and reseeds when the pattern dies out, freezes or falls into
// a short loop.
class LifeMode : public Mode {
 public:
  const char *id() const override { return "life"; }
  const char *name() const override { return "Gioco della vita"; }
  void start() override;
  void update(uint32_t now) override;
  const char *actionName() const override { return "Ricomincia"; }
  void action() override { start(); }

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
