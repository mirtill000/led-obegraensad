#include "modes/quotes_mode.h"

#include "settings.h"
#include "timekeeping.h"

// The font has no accented letters, so Italian quotes are picked to avoid
// them.
static const char *const QUOTES[] = {
    "dare mighty things",
    "stay hungry stay foolish",
    "chi va piano va sano e va lontano",
    "fatti non foste a viver come bruti",
    "per aspera ad astra",
    "less but better",
    "one step at a time",
    "done is better than perfect",
    "carpe diem",
    "think big start small",
    "nulla dies sine linea",
    "the best is yet to come",
    "stay curious",
    "work hard be kind",
    "ad maiora",
    "small steps every day",
    "never stop learning",
    "what we do echoes in eternity",
    "hic et nunc",
    "slow down breathe",
    "enjoy the little things",
    "make it simple",
    "believe you can",
    "keep going",
    "the journey is the reward",
    "sogna in grande parti in piccolo",
    "fortes fortuna adiuvat",
    "be here now",
};
static const uint16_t QUOTE_COUNT = sizeof(QUOTES) / sizeof(QUOTES[0]);

uint16_t QuotesMode::currentIndex() const {
  uint32_t hour;
  struct tm t;
  if (localTime(t)) {
    hour = (t.tm_year * 366u + t.tm_yday) * 24u + t.tm_hour;
  } else {
    hour = millis() / 3600000u;  // no clock yet: hours since power-on
  }
  return (hour + skip_) % QUOTE_COUNT;
}

void QuotesMode::start() {
  shown_ = currentIndex();
  scroller_.start(QUOTES[shown_]);
}

void QuotesMode::update(uint32_t now) {
  if (!scroller_.update(now, settings.speedMs)) return;
  if (currentIndex() != shown_) start();
}

void QuotesMode::action() {
  skip_++;
  start();
}
