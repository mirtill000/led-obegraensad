#pragma once

#include "formula.h"
#include "modes.h"

// "Formule": the panel drawn by a formula of t, i, x, y typed on the page
// (settings.formula, see formula.h), tixy.land style: brighter where the
// value is higher, faint where it is negative. A new formula takes effect
// at once, no new firmware needed.
class FormulaMode : public Mode {
 public:
  const char *id() const override { return "formula"; }
  const char *name() const override { return "Formule"; }
  void start() override;
  void update(uint32_t now) override;
  bool hasSpeed() const override { return false; }
  const char *actionName() const override { return "Da capo"; }
  void action() override { start(); }

  // Checks and installs a new formula; false with the reason if it doesn't
  // parse (the old one stays).
  static bool setFormula(const String &text, String &error);
};
