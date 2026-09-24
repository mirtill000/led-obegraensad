#pragma once

#include <stdint.h>

#include "display.h"

// 5x6 digits for the big clock (bit 15 = leftmost column): the same rounded
// ("Morbido") shapes as the fonts' digits, drawn 5 columns wide.
static const uint16_t BIG_DIGITS[10][6] = {
    {0x7000, 0x8800, 0x8800, 0x8800, 0x8800, 0x7000},  // 0
    {0x2000, 0x6000, 0x2000, 0x2000, 0x2000, 0x2000},  // 1
    {0xF000, 0x0800, 0x3000, 0x4000, 0x8000, 0xF800},  // 2
    {0xF000, 0x0800, 0x3000, 0x0800, 0x0800, 0xF000},  // 3
    {0x8800, 0x8800, 0xF800, 0x0800, 0x0800, 0x0800},  // 4
    {0xF800, 0x8000, 0xF000, 0x0800, 0x0800, 0xF000},  // 5
    {0x7800, 0x8000, 0xF000, 0x8800, 0x8800, 0x7000},  // 6
    {0xF800, 0x0800, 0x1000, 0x2000, 0x2000, 0x2000},  // 7
    {0x7000, 0x8800, 0x7000, 0x8800, 0x8800, 0x7000},  // 8
    {0x7000, 0x8800, 0x7800, 0x0800, 0x0800, 0xF000},  // 9
};

// Draws 0-99 in big digits, horizontally centred, top at row `y`.
inline void drawBigNumber(int value, int y, uint8_t level = 255) {
  const bool two = value >= 10;
  const int x = two ? 2 : 5;  // 11 or 5 pixels wide
  auto digit = [&](int d, int dx) {
    for (int row = 0; row < 6; row++) {
      for (int col = 0; col < 5; col++) {
        if (BIG_DIGITS[d][row] & (0x8000 >> col)) display.setLevel(dx + col, y + row, level);
      }
    }
  };
  if (two) digit(value / 10 % 10, x);
  digit(value % 10, two ? x + 6 : x);
}
