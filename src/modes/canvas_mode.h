#pragma once

#include "animation.h"
#include "constants.h"
#include "modes.h"

// "Lavagna": a shared live drawing. Whoever has the page open paints on it
// (every open page sees the others' strokes within a second), and the
// Cardputer draws with a blinking cursor: arrows move it, space lights or
// clears the pixel under it. The drawing is kept across restarts (NVS blob
// "canvas") and can become the first generation of the Game of Life
// ("Fai vivere", also the mode's button) or be saved among the drawings.
class CanvasMode : public Mode {
 public:
  const char *id() const override { return "canvas"; }
  const char *name() const override { return "Lavagna"; }
  void start() override;
  void update(uint32_t now) override;
  const char *actionName() const override { return "Fai vivere"; }
  void action() override { toLife(); }
  bool hasSpeed() const override { return false; }
  bool input(char key) override;
  const GameControls *controls() const override;

  static void paint(int x, int y, uint8_t level);
  static void clear();
  static const uint8_t *pixels();  // 256 levels, row by row
  static uint32_t version();       // changes with every stroke
  // Seeds the Game of Life with the lit pixels and shows it.
  static void toLife();
  static void saveIfChanged();     // called now and then (debounced writes)
};
