#include "modes/text_mode.h"

#include "display.h"
#include "settings.h"

void TextMode::start() {
  offset_ = -COLS;
  width_ = Display::scrollWidth(settings.text.c_str());
  lastStep_ = 0;
}

void TextMode::update(uint32_t now) {
  if (now - lastStep_ < settings.speedMs) return;
  lastStep_ = now;

  display.drawScrollFrame(settings.text.c_str(), offset_);
  if (++offset_ >= width_) offset_ = -COLS;
}
