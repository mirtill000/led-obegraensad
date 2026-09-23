#include "modes/web_mode.h"

#include "settings.h"
#include "webinfo.h"

void WebMode::start() {
  slot_ = 0;
  showNext();
}

// The next available line, going round word -> history -> calendar.
void WebMode::showNext() {
  const WebInfo info = webInfoNow();
  String line;
  for (int tries = 0; tries < 3 && line.length() == 0; tries++) {
    switch (slot_++ % 3) {
      case 0: line = info.word; break;
      case 1:
        if (info.historyCount) line = info.history[history_++ % info.historyCount];
        break;
      case 2: line = info.event; break;
    }
  }
  if (line.length() == 0) {
    line = settings.infoWord || settings.infoHistory || settings.infoCalendar ? "In attesa di dati dal web..."
                                                                              : "Scegli cosa mostrare nella pagina";
  }
  scroller_.start(line);
  row_ = Scroller::rowFor(settings.webPosition, row_);
  scroller_.setRow(row_);
}

void WebMode::update(uint32_t now) {
  if (scroller_.update(now, interval(SCROLL_DELAY_MS))) showNext();
}
