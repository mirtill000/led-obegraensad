#pragma once

#include "modes.h"
#include "pager.h"

// A different quote every hour, shown as still pages in the font chosen in
// Display (see Pager), again and again; the quote only changes at the end
// of a pass, so it is never cut off. The list is the built-in quotes
// (generated from content/frasi_dell_ora.txt) followed by those added on the
// web page (settings.quotes, one per line).
class QuotesMode : public Mode {
 public:
  const char *id() const override { return "quotes"; }
  const char *name() const override { return "Frase dell'ora"; }
  void start() override;
  void update(uint32_t now) override;
  const char *actionName() const override { return "Prossima frase"; }
  void action() override;

  // How many quotes are in rotation (built-in plus added).
  static uint16_t count();
  static uint16_t builtInCount();  // the quotes from content/frasi_dell_ora.txt
  // Quote number `index` (wrapping round) of the list in use, UTF-8.
  static String quoteAt(uint16_t index);

 private:
  // Index of the quote for the current hour (plus any skips).
  uint16_t currentIndex(uint16_t count) const;
  Pager pager_;
  String quote_;       // the quote being shown (UTF-8)
  uint16_t shown_ = 0;
  uint16_t skip_ = 0;  // "next quote" presses
};
