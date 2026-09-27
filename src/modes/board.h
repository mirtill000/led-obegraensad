#pragma once

#include <Arduino.h>

#include "constants.h"

// The drawing board of the Gioco della vita: a 16x16 picture everyone can
// paint on - from the page (each open page sees the others' strokes within
// a second) or with the Cardputer's cursor - which becomes the first
// generation when "Fai vivere" is pressed. Kept across restarts (NVS blob
// "canvas", so it is in the settings backup).
namespace board {

void paint(int x, int y, uint8_t level);
void clear();
const uint8_t *pixels();  // 256 levels, row by row
uint32_t version();       // changes with every stroke
// The cursor, for keys: arrows move it (wrapping), A lights or clears the
// pixel under it. False for keys it doesn't use.
bool input(char key);
// Draws the board, with the cursor blinking for a while after a key.
void draw(uint32_t now);
void saveIfChanged();     // called now and then (debounced writes)

}  // namespace board
