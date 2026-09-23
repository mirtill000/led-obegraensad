#include "modes/text_mode.h"

#include "settings.h"

void TextMode::start() { scroller_.start(settings.text); }

void TextMode::update(uint32_t now) { scroller_.update(now, settings.speedMs); }
