#pragma once

#include "modes.h"
#include "pager.h"
#include "scroller.h"

// Scrolls settings.text across the panel on one line, looping forever, at
// the height set in settings.textPosition - or, with "pages", shows it as
// still pages of three lines (see Pager), over and over.
class TextMode : public Mode {
 public:
  const char *id() const override { return "text"; }
  const char *name() const override { return "Testo scorrevole"; }
  void start() override;
  void update(uint32_t now) override;

 private:
  Scroller scroller_;
  Pager pager_;
  bool pages_ = false;
  int row_ = -1;
};
