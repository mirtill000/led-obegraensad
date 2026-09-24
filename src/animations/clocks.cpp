// Alternative clock faces: binary and in words (Italian).
#include <math.h>

#include "animation.h"
#include "display.h"
#include "gfx.h"
#include "scroller.h"
#include "timekeeping.h"
#include "ui.h"

// Seconds with a fractional part, for hands that move smoothly: counts
// milliseconds since the whole second last changed.
static float smoothSeconds(const struct tm &t) {
  static int lastSec = -1;
  static uint32_t secStart = 0;
  if (t.tm_sec != lastSec) {
    lastSec = t.tm_sec;
    secStart = millis();
  }
  return t.tm_sec + min(999u, (unsigned)(millis() - secStart)) / 1000.0f;
}

// Shown by every clock face until the time has synced: the shared
// waiting sign (see ui.h).
static void drawNoTime() {
  display.clear();
  ui::waiting(millis());
}

// ---------------------------------------------------------------------------
// Binary-coded decimal: one column per digit of HH:MM, bits from the bottom
// (1, 2, 4, 8), unlit bits faintly visible; a bar at the bottom fills up
// with the seconds.
class BinaryClockAnimation : public Animation {
 public:
  const char *id() const override { return "binary"; }
  const char *name() const override { return "Orologio binario"; }
  const char *group() const override { return "Orologi"; }
  uint16_t frameMs() const override { return 100; }
  bool needsTime() const override { return true; }

  void frame(uint32_t) override {
    struct tm t;
    if (!localTime(t)) return drawNoTime();
    display.clear();
    const int digits[4] = {t.tm_hour / 10, t.tm_hour % 10, t.tm_min / 10, t.tm_min % 10};
    const int bits[4] = {2, 4, 3, 4};  // bits each digit can need
    const int columnX[4] = {2, 5, 9, 12};
    for (int d = 0; d < 4; d++) {
      for (int b = 0; b < bits[d]; b++) {
        const uint8_t l = (digits[d] >> b) & 1 ? 255 : 22;
        const int y = 11 - b * 3;  // bit 0 at rows 11-12, bit 3 at rows 2-3
        for (int dy = 0; dy < 2; dy++) {
          for (int dx = 0; dx < 2; dx++) display.setLevel(columnX[d] + dx, y + dy, l);
        }
      }
    }
    const float filled = smoothSeconds(t) / 60 * COLS;
    for (int x = 0; x < COLS; x++) {
      const float f = filled - x;
      display.setLevel(x, 15, gfx::level(f >= 1 ? 0.45f : f > 0 ? f * 0.45f : 0.04f));
    }
  }
};

// ---------------------------------------------------------------------------
// The time in Italian words, rounded to five minutes, scrolling by.
class WordClockAnimation : public Animation {
 public:
  const char *id() const override { return "words"; }
  const char *name() const override { return "Orologio a parole"; }
  const char *group() const override { return "Orologi"; }
  uint16_t frameMs() const override { return 80; }
  bool needsTime() const override { return true; }

  void start() override { scroller_.start(phrase()); }

  void frame(uint32_t now) override {
    // One pixel per frame; a fresh phrase at the start of every pass.
    if (scroller_.update(now, 0)) scroller_.start(phrase());
  }

  // "sono le tre e un quarto", "è mezzanotte", "è l'una meno cinque"...
  static String phrase() {
    struct tm t;
    if (!localTime(t)) return "in attesa dell'ora";
    static const char *const HOURS[] = {"dodici", "una",  "due",  "tre",   "quattro", "cinque",
                                        "sei",    "sette", "otto", "nove", "dieci",   "undici"};
    static const char *const PAST[] = {"in punto",       "e cinque", "e dieci",        "e un quarto",
                                       "e venti",        "e venticinque", "e mezza", "e trentacinque",
                                       "meno venti",     "meno un quarto", "meno dieci", "meno cinque"};
    int m5 = (t.tm_min + 2) / 5;  // nearest five minutes, 0-12
    int hour = t.tm_hour;
    if (m5 == 12) {
      m5 = 0;
      hour++;
    }
    if (m5 >= 8) hour++;  // "meno ..." refers to the next hour
    hour %= 24;

    String s;
    if (hour == 0) {
      s = "è mezzanotte";
    } else if (hour == 12) {
      s = "è mezzogiorno";
    } else if (hour % 12 == 1) {
      s = "è l'una";
    } else {
      s = String("sono le ") + HOURS[hour % 12];
    }
    return s + " " + PAST[m5];
  }

 private:
  Scroller scroller_;
};

static BinaryClockAnimation binaryClock;
extern Animation *const binaryClockAnimation = &binaryClock;
static WordClockAnimation wordClock;
extern Animation *const wordClockAnimation = &wordClock;
