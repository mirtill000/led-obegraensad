#include "modes/text_mode.h"

#include "settings.h"

void TextMode::start() {
  // Always one line through the middle; texts saved by older versions may
  // still contain the two-line '|' separator.
  String text = settings.text;
  text.replace('|', ' ');
  scroller_.start(text);
}

void TextMode::update(uint32_t now) { scroller_.update(now, interval(SCROLL_DELAY_MS)); }
