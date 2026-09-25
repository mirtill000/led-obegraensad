#pragma once

#include "modes.h"
#include "pager.h"

// A different quote every hour, shown as still pages of three lines in the
// 4-row Tiny font (see Pager), again and again; the quote only changes at
// the end of a pass, so it is never cut off. The list is edited
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
  Pager pager_;
  String quote_;       // the quote being shown (UTF-8)
  uint16_t shown_ = 0;
  uint16_t skip_ = 0;  // "next quote" presses
};
