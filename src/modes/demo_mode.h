#pragma once

#include <vector>

#include "modes.h"

// "Demo": the hourly quotes shown three ways, to compare how a long text
// reads on 16x16 LEDs (settings.demoStyle, or "auto" for each in turn):
//  rows3 - split into three lines of about the same length that scroll
//          together, in the 4-row Tiny font (4+1+4+1+4 rows): a third of
//          the scrolling;
//  pages - still screens of three lines (up to 16 pixels each, words kept
//          whole where they fit), in the Tiny font, one after another;
//  rows2 - two lines scrolling together in the 5-row mini font.
class DemoMode : public Mode {
 public:
  const char *id() const override { return "demo"; }
  const char *name() const override { return "Demo"; }
  void start() override;
  void update(uint32_t now) override;
  const char *actionName() const override { return "Prossima frase"; }
  void action() override { next(); }

 private:
  enum Style : uint8_t { ROWS3, PAGES, ROWS2, STYLES };

  void next();       // the next quote (and, in "auto", the next style)
  void layout();     // splits quote_ into lines_ / pages_ for style_
  void drawScroll();
  void drawPage();

  String quote_;           // in the font's single-byte characters
  uint16_t index_ = 0;     // quote number
  Style style_ = ROWS3;
  uint8_t autoStyle_ = 0;
  std::vector<String> lines_;  // rows3 / rows2: the lines; pages: 3 per page
  int offset_ = 0;             // scrolling position
  int width_ = 0;              // widest line, pixels
  size_t page_ = 0;
  uint32_t lastStep_ = 0;
  bool started_ = false;
};
