// Alternative clock faces: binary, in words (Italian and English), a pile
// of sand growing by the minute.
#include <math.h>
#include <string.h>

#include "animation.h"
#include "bigdigits.h"
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

// ---------------------------------------------------------------------------
// The time in English words, like the Italian one: "it's quarter past
// three", "it's twenty to four", "it's midnight".
class EnglishWordClockAnimation : public Animation {
 public:
  const char *id() const override { return "wordsen"; }
  const char *name() const override { return "Orologio a parole (inglese)"; }
  const char *group() const override { return "Orologi"; }
  uint16_t frameMs() const override { return 80; }
  bool needsTime() const override { return true; }

  void start() override { scroller_.start(phrase()); }
  void frame(uint32_t now) override {
    if (scroller_.update(now, 0)) scroller_.start(phrase());
  }

  static String phrase() {
    struct tm t;
    if (!localTime(t)) return "waiting for the time";
    static const char *const HOURS[] = {"twelve", "one", "two", "three", "four", "five",
                                        "six",    "seven", "eight", "nine", "ten", "eleven"};
    static const char *const PAST[] = {"",           "five past", "ten past",    "quarter past", "twenty past",
                                       "twenty-five past", "half past", "twenty-five to", "twenty to",
                                       "quarter to", "ten to",    "five to"};
    int m5 = (t.tm_min + 2) / 5;
    int hour = t.tm_hour;
    if (m5 == 12) {
      m5 = 0;
      hour++;
    }
    if (m5 >= 7) hour++;  // "... to" the next hour
    hour %= 24;
    String h = hour == 0 ? "midnight" : hour == 12 ? "noon" : HOURS[hour % 12];
    if (m5 == 0) return hour % 12 ? "it's " + h + " o'clock" : "it's " + h;
    return String("it's ") + PAST[m5] + " " + h;
  }

 private:
  Scroller scroller_;
};

// ---------------------------------------------------------------------------
// Sand clock: the hour in big digits at the top; below, a grain of sand
// falls for every minute and piles up with falling-sand physics (it
// slides down the slopes). At the new hour the floor opens and the pile
// drains away. On start the pile rains down at once.
class SandClockAnimation : public Animation {
 public:
  const char *id() const override { return "sandclock"; }
  const char *name() const override { return "Orologio di sabbia"; }
  const char *group() const override { return "Orologi"; }
  uint16_t frameMs() const override { return 50; }
  bool needsTime() const override { return true; }

  void start() override {
    memset(sand_, 0, sizeof(sand_));
    grains_ = 0;
    draining_ = false;
  }

  void frame(uint32_t) override {
    struct tm t;
    if (!localTime(t)) return drawNoTime();
    if (t.tm_min < grains_ && !draining_) draining_ = true;  // a new hour
    if (draining_) {
      // The floor is open: grains fall out through the bottom row.
      for (int x = 0; x < COLS; x++) sand_[ROWS - 1][x] = false;
      if (!any()) {
        draining_ = false;
        grains_ = 0;
      }
    } else if (grains_ < t.tm_min && !sand_[TOP][7] && !sand_[TOP][8]) {
      sand_[TOP][(esp_random() & 1) ? 7 : 8] = true;  // the next grain
      grains_++;
    }
    fall();
    display.clear();
    drawBigNumber(t.tm_hour, 0, 200);
    // The spout the grains come from.
    display.setLevel(7, TOP - 1, 50);
    display.setLevel(8, TOP - 1, 50);
    for (int y = TOP; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        if (sand_[y][x]) display.setLevel(x, y, 150 + ((x * 7 + y * 3) % 5) * 20);  // a grainy texture
      }
    }
  }

 private:
  static const int TOP = 7;  // the sand's space: rows 7-15
  bool sand_[ROWS][COLS];
  int grains_ = 0;
  bool draining_ = false;

  bool any() const {
    for (int y = TOP; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        if (sand_[y][x]) return true;
      }
    }
    return false;
  }
  bool freeCell(int x, int y) const { return x >= 0 && x < COLS && (y >= ROWS ? draining_ : !sand_[y][x]); }
  void fall() {
    for (int y = ROWS - 1; y >= TOP; y--) {
      const bool leftFirst = esp_random() & 1;
      for (int i = 0; i < COLS; i++) {
        const int x = leftFirst ? i : COLS - 1 - i;
        if (!sand_[y][x]) continue;
        int nx = -1;
        if (freeCell(x, y + 1)) {
          nx = x;
        } else {
          const int a = (esp_random() & 1) ? -1 : 1;
          if (freeCell(x + a, y + 1)) nx = x + a;
          else if (freeCell(x - a, y + 1)) nx = x - a;
        }
        if (nx < 0) continue;
        sand_[y][x] = false;
        if (y + 1 < ROWS) sand_[y + 1][nx] = true;  // else it drained away
      }
    }
  }
};

static BinaryClockAnimation binaryClock;
extern Animation *const binaryClockAnimation = &binaryClock;
static WordClockAnimation wordClock;
extern Animation *const wordClockAnimation = &wordClock;
static EnglishWordClockAnimation englishWordClock;
extern Animation *const englishWordClockAnimation = &englishWordClock;
static SandClockAnimation sandClock;
extern Animation *const sandClockAnimation = &sandClock;
