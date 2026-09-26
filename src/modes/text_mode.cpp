#include "modes/text_mode.h"

#include "settings.h"

void TextMode::start() {
  // Always one line through the middle; texts saved by older versions may
  // still contain the two-line '|' separator.
  String text = settings.text;
  text.replace('|', ' ');
  pages_ = settings.textPosition == "pages";
  if (pages_) {
    pager_.start(text);
    return;
  }
  scroller_.start(text);
  row_ = Scroller::rowFor(settings.textPosition, -1);
  scroller_.setRow(row_);
}

void TextMode::update(uint32_t now) {
  if (pages_) {
    if (pager_.update(now, interval(PAGE_MS))) start();  // again from the first page
    return;
  }
  // A new height (if set to vary) every time the text has gone by.
  if (scroller_.update(now, interval(SCROLL_DELAY_MS))) {
    row_ = Scroller::rowFor(settings.textPosition, row_);
    scroller_.setRow(row_);
  }
}
