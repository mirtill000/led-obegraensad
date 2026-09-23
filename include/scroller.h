#pragma once

#include <Arduino.h>

#include "display.h"

// Non-blocking scrolling text: call update() from a mode's update(). Holds
// its own copy of the text; '|' splits it into two lines (see Display).
class Scroller {
 public:
  // `text` is UTF-8; accented letters are converted for the font.
  void start(const String &text) {
    text_ = Display::fontText(text);
    width_ = Display::scrollWidth(text_.c_str());
    offset_ = -COLS;
    lastStep_ = 0;
  }

  // Advances one pixel every `stepMs`; returns true when a full pass has
  // finished (the text has left the panel and will start over).
  bool update(uint32_t now, uint16_t stepMs) {
    if (now - lastStep_ < stepMs) return false;
    lastStep_ = now;
    display.drawScrollFrame(text_.c_str(), offset_, row_);
    if (++offset_ < width_) return false;
    offset_ = -COLS;
    return true;
  }

  const String &text() const { return text_; }

  // Top row of the text (-1 = centred); takes effect on the next step.
  void setRow(int row) { row_ = row; }

  // Row for a position setting: "top", "middle", "bottom", or "random" - a
  // different height from `previous` each time (at least 2 rows away).
  static int rowFor(const String &position, int previous) { return Display::textRow(position, previous); }

 private:
  String text_;
  int width_ = 0;
  int offset_ = 0;
  int row_ = -1;
  uint32_t lastStep_ = 0;
};
