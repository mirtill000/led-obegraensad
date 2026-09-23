// Alternative clock faces: analog, binary and in words (Italian).
#include <math.h>

#include "animation.h"
#include "display.h"
#include "gfx.h"
#include "scroller.h"
#include "timekeeping.h"

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

// Shown by every clock face until the time has synced.
static void drawNoTime() {
  display.clear();
  for (int i = 0; i < 3; i++) display.setLevel(5 + i * 3, 8, 120);  // "..."
}

// ---------------------------------------------------------------------------
class AnalogClockAnimation : public Animation {
 public:
  const char *id() const override { return "analog"; }
  const char *name() const override { return "Orologio analogico"; }
  const char *group() const override { return "Orologi"; }
  uint16_t frameMs() const override { return 50; }
  bool needsTime() const override { return true; }

  void frame(uint32_t) override {
    struct tm t;
    if (!localTime(t)) return drawNoTime();
    display.clear();
    const float c = 7.5f;
    // Hour marks: brighter at 12, 3, 6 and 9.
    for (int h = 0; h < 12; h++) {
      const float a = h * PI / 6;
      gfx::plot((int)roundf(c + 7.2f * sinf(a)), (int)roundf(c - 7.2f * cosf(a)), h % 3 ? 0.18f : 0.5f);
    }
    const float sec = smoothSeconds(t);
    const float minute = t.tm_min + sec / 60;
    const float hour = (t.tm_hour % 12) + minute / 60;
    hand(hour * PI / 6, 3.8f, 1.0f);
    hand(minute * PI / 30, 5.8f, 1.0f);
    hand(sec * PI / 30, 6.5f, 0.35f);
  }

 private:
  static void hand(float angle, float length, float v) {
    gfx::line(7.5f, 7.5f, 7.5f + length * sinf(angle), 7.5f - length * cosf(angle), v);
  }
};

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

static AnalogClockAnimation analogClock;
extern Animation *const analogClockAnimation = &analogClock;
static BinaryClockAnimation binaryClock;
extern Animation *const binaryClockAnimation = &binaryClock;
static WordClockAnimation wordClock;
extern Animation *const wordClockAnimation = &wordClock;
