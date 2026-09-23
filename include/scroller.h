#pragma once

#include <Arduino.h>

#include "display.h"

// Non-blocking scrolling text: call update() from a mode's update(). Holds
// its own copy of the text; '|' splits it into two lines (see Display).
class Scroller {
 public:
  void start(const String &text) {
    text_ = text;
    width_ = Display::scrollWidth(text_.c_str());
    offset_ = -COLS;
    lastStep_ = 0;
  }

  // Advances one pixel every `stepMs`; returns true when a full pass has
  // finished (the text has left the panel and will start over).
  bool update(uint32_t now, uint16_t stepMs) {
    if (now - lastStep_ < stepMs) return false;
    lastStep_ = now;
    display.drawScrollFrame(text_.c_str(), offset_);
    if (++offset_ < width_) return false;
    offset_ = -COLS;
    return true;
  }

  const String &text() const { return text_; }

 private:
  String text_;
  int width_ = 0;
  int offset_ = 0;
  uint32_t lastStep_ = 0;
};
