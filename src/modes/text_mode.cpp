#include "modes/text_mode.h"

#include "settings.h"

void TextMode::start() {
  // Always one line through the middle; texts saved by older versions may
  // still contain the two-line '|' separator.
  String text = settings.text;
  text.replace('|', ' ');
  scroller_.start(text);
  row_ = Scroller::rowFor(settings.textPosition, -1);
  scroller_.setRow(row_);
}

void TextMode::update(uint32_t now) {
  // A new height (if set to vary) every time the text has gone by.
  if (scroller_.update(now, interval(SCROLL_DELAY_MS))) {
    row_ = Scroller::rowFor(settings.textPosition, row_);
    scroller_.setRow(row_);
  }
}
