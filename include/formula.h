#pragma once

#include <Arduino.h>

#include <vector>

// Per-pixel formulas in the style of tixy.land: an expression of
//   t  seconds since start     i  pixel index 0-255
//   x  column 0-15              y  row 0-15
// written like JavaScript: numbers, + - * / % ** , comparisons, && || !,
// bitwise & | ^ ~ << >>, the ternary ?:, PI, and the functions sin cos tan
// asin acos atan atan2 abs sqrt floor ceil round sign min max pow hypot
// log exp random (a "Math." in front is accepted). The value lights the
// pixel: 1 or more is full, 0 off; negative values show faintly (tixy
// draws them red). true/false count as 1/0.
//
// compile() turns the text into a small stack program once; eval() runs it
// for each pixel of each frame, so a new formula needs no new firmware.
class Formula {
 public:
  // False with `error` (in Italian, with the position) if it doesn't parse.
  bool compile(const String &text, String &error);
  float eval(float t, float i, float x, float y) const;
  bool ok() const { return !code_.empty(); }

 private:
  struct Op {
    uint8_t code;
    uint8_t arg;  // function id / argument count
    int16_t jump;
    float value;
  };
  std::vector<Op> code_;
  friend class FormulaParser;
};
