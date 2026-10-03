#pragma once

#include "constants.h"
#include "modes.h"

// Conway's Game of Life on a 16x16 torus (edges wrap around), with a
// drawing board (board.h) for the first generation.
//
// Left alone, each game starts from an empty board with just a handful of
// cells in the middle - a small "methuselah" pattern that grows into a lot
// of activity - and starts over with another one when the board dies out,
// freezes or loops. As soon as someone draws (the page, the Cardputer's
// cursor, the "w" commands) the lamp shows the drawing instead; "Fai
// vivere" (the mode's button then) sets it going as generation zero.
class LifeMode : public Mode {
 public:
  const char *id() const override { return "life"; }
  const char *name() const override { return "Gioco della vita"; }
  void start() override;
  void update(uint32_t now) override;
  const char *actionName() const override { return drawing_ ? "Fai vivere" : "Ricomincia"; }
  String status() const override {
    return drawing_ ? String("Disegno: X lo fa vivere") : "Generazione " + String(generation_);
  }
  void action() override;
  bool input(char key) override;
  const GameControls *controls() const override;

  // Starts a game from the board (the next update, on whichever instance
  // is shown).
  static void fromBoard();

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
  bool drawing_ = false;       // showing the board, not evolving
  uint32_t seenVersion_ = 0;   // board::version() last looked at
  uint32_t lastBoardDraw_ = 0;
};
