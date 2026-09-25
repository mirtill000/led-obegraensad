#pragma once

#include "modes.h"
#include "scroller.h"

// A different quote every hour, scrolling on one line. The quote only
// changes at the end of a pass, so it is never cut off. The list is edited
// on the web page (settings.quotes, one per line); when that is empty the
// built-in list is used, generated from content/frasi_dell_ora.txt.
class QuotesMode : public Mode {
 public:
  const char *id() const override { return "quotes"; }
  const char *name() const override { return "Frase dell'ora"; }
  void start() override;
  void update(uint32_t now) override;
  const char *actionName() const override { return "Prossima frase"; }
  void action() override;

  // The built-in list, one quote per line (UTF-8).
  static const char *defaultQuotes();
  // How many quotes the list in use has.
  static uint16_t count();
  // Quote number `index` (wrapping round) of the list in use, UTF-8.
  static String quoteAt(uint16_t index);

 private:
  // Index of the quote for the current hour (plus any skips).
  uint16_t currentIndex(uint16_t count) const;
  void nextRow();

  Scroller scroller_;
  uint16_t shown_ = 0;
  uint16_t skip_ = 0;  // "next quote" presses
  int row_ = -1;       // current height
};
