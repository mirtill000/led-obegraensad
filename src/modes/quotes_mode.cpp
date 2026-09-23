#include "modes/quotes_mode.h"

#include "settings.h"
#include "timekeeping.h"

static const char DEFAULT_QUOTES[] =
    "Dare mighty things\n"
    "Stay hungry, stay foolish\n"
    "Chi va piano va sano e va lontano\n"
    "Fatti non foste a viver come bruti\n"
    "Per aspera ad astra\n"
    "La semplicità è la suprema sofisticazione\n"
    "Less but better\n"
    "One step at a time\n"
    "Done is better than perfect\n"
    "Carpe diem\n"
    "Think big, start small\n"
    "Nulla dies sine linea\n"
    "The best is yet to come\n"
    "Stay curious\n"
    "Work hard, be kind\n"
    "Chi si ferma è perduto\n"
    "Ad maiora\n"
    "Small steps every day\n"
    "Never stop learning\n"
    "What we do echoes in eternity\n"
    "Hic et nunc\n"
    "Slow down, breathe\n"
    "Enjoy the little things\n"
    "Make it simple\n"
    "Believe you can\n"
    "Keep going\n"
    "The journey is the reward\n"
    "Sogna in grande, parti in piccolo\n"
    "Fortes fortuna adiuvat\n"
    "Be here now";

const char *QuotesMode::defaultQuotes() { return DEFAULT_QUOTES; }

static const String &activeList() {
  static String builtIn = DEFAULT_QUOTES;
  return settings.quotes.length() ? settings.quotes : builtIn;
}

// Splits the list into non-empty lines; returns how many there are and, if
// `want` is in range, that line in `out`.
static uint16_t quoteLine(const String &list, uint16_t want, String *out) {
  uint16_t n = 0;
  int start = 0;
  while (start <= (int)list.length()) {
    int end = list.indexOf('\n', start);
    if (end < 0) end = list.length();
    String line = list.substring(start, end);
    line.trim();
    if (line.length()) {
      if (n == want && out) *out = line;
      n++;
    }
    start = end + 1;
  }
  return n;
}

uint16_t QuotesMode::currentIndex(uint16_t count) const {
  uint32_t hour;
  struct tm t;
  if (localTime(t)) {
    hour = (t.tm_year * 366u + t.tm_yday) * 24u + t.tm_hour;
  } else {
    hour = millis() / 3600000u;  // no clock yet: hours since power-on
  }
  return count ? (hour + skip_) % count : 0;
}

void QuotesMode::start() {
  const String &list = activeList();
  const uint16_t count = quoteLine(list, 0xFFFF, nullptr);
  shown_ = currentIndex(count);
  String quote;
  quoteLine(list, shown_, &quote);
  scroller_.start(quote);
}

void QuotesMode::update(uint32_t now) {
  if (!scroller_.update(now, interval(SCROLL_DELAY_MS))) return;
  if (currentIndex(quoteLine(activeList(), 0xFFFF, nullptr)) != shown_) start();
}

void QuotesMode::action() {
  skip_++;
  start();
}
