#include "modes/formula_mode.h"

#include <math.h>

#include "display.h"
#include "settings.h"

static Formula formula;
static String compiled;  // the text `formula` was built from
static uint32_t startMs = 0, lastDraw = 0;

static void ensureCompiled() {
  if (compiled == settings.formula && formula.ok()) return;
  String error;
  if (!formula.compile(settings.formula, error)) formula.compile("0", error);  // a bad saved one: dark
  compiled = settings.formula;
}

bool FormulaMode::setFormula(const String &text, String &error) {
  Formula test;
  if (!test.compile(text, error)) return false;
  settings.formula = text;
  formula = test;
  compiled = text;
  startMs = millis();
  return true;
}

void FormulaMode::start() {
  ensureCompiled();
  startMs = millis();
  display.beginTransition();
}

void FormulaMode::update(uint32_t now) {
  if (now - lastDraw < 33) return;
  lastDraw = now;
  ensureCompiled();
  const float t = (now - startMs) / 1000.0f;
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      const float v = formula.eval(t, y * COLS + x, x, y);
      uint8_t level = 0;
      if (v > 0) level = v >= 1 ? 255 : (uint8_t)(v * 255);
      else if (v < 0) level = v <= -1 ? 60 : (uint8_t)(-v * 60);  // negative: faint (red in tixy)
      display.setLevel(x, y, level);  // NaN fails both tests: off
    }
  }
  display.render();
}
