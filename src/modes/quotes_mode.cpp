#include "modes/quotes_mode.h"

#include "settings.h"
#include "timekeeping.h"

// Built-in list, generated from content/frasi_dell_ora.txt at build time.
#include "quotes_builtin.h"

const char *QuotesMode::defaultQuotes() { return BUILTIN_QUOTES; }

static const String &activeList() {
  static String builtIn = BUILTIN_QUOTES;
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

static const uint32_t PAGE_MS = 2500;  // each page (scaled by the speed setting)

void QuotesMode::start() {
  const String &list = activeList();
  const uint16_t count = quoteLine(list, 0xFFFF, nullptr);
  shown_ = currentIndex(count);
  quote_ = "";
  quoteLine(list, shown_, &quote_);
  pager_.start(quote_);
}

void QuotesMode::update(uint32_t now) {
  if (!pager_.update(now, interval(PAGE_MS))) return;
  // The quote is over: the next one if the hour has changed, else again.
  if (currentIndex(quoteLine(activeList(), 0xFFFF, nullptr)) != shown_) {
    start();
  } else {
    pager_.start(quote_);
  }
}

uint16_t QuotesMode::count() { return quoteLine(activeList(), UINT16_MAX, nullptr); }

String QuotesMode::quoteAt(uint16_t index) {
  String quote;
  const uint16_t n = count();
  if (n) quoteLine(activeList(), index % n, &quote);
  return quote;
}

void QuotesMode::action() {
  skip_++;
  start();
}
