#include "modes/notify_mode.h"

#include <math.h>

#include "display.h"

namespace {

struct Icon {
  const char *id;
  uint8_t rows;
  const char *const *art;  // '#' lit, top row first
  enum Motion : uint8_t { PULSE, SWING, SHAKE, BEAT } motion;
};

const char *const BELL[] = {"....#....", "...###...", "..#####..", "..#####..", "..#####..",
                            ".#######.", ".#######.", "#########", ".........", "...###..."};
const char *const MAIL[] = {"############", "##........##", "#.#......#.#", "#..#....#..#",
                            "#...#..#...#", "#....##....#", "#..........#", "############"};
const char *const CHECK[] = {"..........#", ".........##", "........##.", "#......##..",
                             "##....##...", ".##..##....", "..####.....", "...##......"};
const char *const ALERT[] = {".....#.....", "....###....", "....#.#....", "...##.##...", "...##.##...",
                             "..###.###..", "..#######..", ".####.####.", "###########"};
const char *const HEART[] = {".##...##.", "####.####", "#########", "#########",
                             ".#######.", "..#####..", "...###...", "....#...."};
const char *const PHONE[] = {"#######", "#.....#", "#.....#", "#.....#", "#.....#", "#.....#",
                             "#.....#", "#.....#", "#######", "#..#..#", "#######"};
const char *const HOME[] = {".....#.....", "....#.#....", "...#...#...", "..#.....#..", ".#.......#.",
                            "###########", ".#.......#.", ".#..###..#.", ".#..#.#..#.", ".####.####."};
const char *const STAR[] = {".....#.....", ".....#.....", "....###....", "###########", ".#########.",
                            "..#######..", "..#######..", ".####.####.", ".###...###.", ".#.......#."};

const Icon ICONS[] = {
    {"bell", 10, BELL, Icon::SWING},   {"mail", 8, MAIL, Icon::PULSE},    {"check", 8, CHECK, Icon::PULSE},
    {"alert", 9, ALERT, Icon::SHAKE},  {"heart", 8, HEART, Icon::BEAT},   {"phone", 11, PHONE, Icon::SHAKE},
    {"home", 10, HOME, Icon::PULSE},   {"star", 10, STAR, Icon::PULSE},
};

const Icon *findIcon(const String &id) {
  for (const Icon &i : ICONS) {
    if (id == i.id) return &i;
  }
  return nullptr;
}

const uint32_t DROP_MS = 450, ICON_MS = 2200, ICON_ONLY_MS = 3500;

struct Note {
  String text;
  const Icon *icon;
};
Note queue[NotifyMode::QUEUE];
int queued = 0;

}  // namespace

bool NotifyMode::push(const String &text, const String &icon) {
  const Icon *i = icon.length() ? findIcon(icon) : nullptr;
  if (icon.length() && !i) return false;
  String t = text;
  t.trim();
  if (!t.length() && !i) return false;
  if (queued == QUEUE) {  // full: drop the oldest still waiting (not the one on show)
    for (int k = 1; k < QUEUE - 1; k++) queue[k] = queue[k + 1];
    queued--;
  }
  queue[queued++] = {t.substring(0, 200), i};
  return true;
}

int NotifyMode::pending() { return queued; }

void NotifyMode::clear() {
  queued = 0;
  for (Note &n : queue) n.text = "";
}

void NotifyMode::start() { begin(millis()); }

void NotifyMode::begin(uint32_t now) {
  since_ = now;
  if (!queued) {
    phase_ = DONE;
    return;
  }
  if (queue[0].icon) {
    phase_ = ICON;
    drawIcon(0);
  } else {
    phase_ = TEXT;
    pager_.start(queue[0].text);
  }
}

// The icon drops in from the top with a little bounce, then moves in its
// own way: the bell swings, the phone and the warning shake, the heart
// beats, the others glow.
void NotifyMode::drawIcon(uint32_t t) {
  const Icon &icon = *queue[0].icon;
  const int w = strlen(icon.art[0]);
  int x = (COLS - w) / 2, y = (ROWS - icon.rows) / 2;
  uint8_t level = 255;
  if (t < DROP_MS) {
    // Ease in from above, overshooting a pixel before settling.
    const float p = t / (float)DROP_MS;
    const float e = 1 - powf(1 - p, 3);
    y = (int)lroundf(-icon.rows + (y + icon.rows) * e + sinf(p * (float)M_PI) * 1.5f);
  } else {
    const uint32_t m = t - DROP_MS;
    switch (icon.motion) {
      case Icon::SWING: {
        static const int8_t SWING[4] = {0, 1, 0, -1};
        if (m < 1200) x += SWING[(m / 110) % 4];
        break;
      }
      case Icon::SHAKE:
        if (m < 900) x += (m / 60) % 2 ? 1 : -1;
        break;
      case Icon::BEAT: {
        static const uint8_t BEAT[6] = {255, 120, 255, 120, 120, 120};
        level = BEAT[(m / 130) % 6];
        break;
      }
      case Icon::PULSE:
        level = 150 + (uint8_t)(105 * (0.5f + 0.5f * cosf(m * 2 * (float)M_PI / 700)));
        break;
    }
  }
  display.clear();
  for (int r = 0; r < icon.rows; r++) {
    for (int c = 0; c < w; c++) {
      if (icon.art[r][c] == '#') display.setLevel(x + c, y + r, level);
    }
  }
  display.render();
}

void NotifyMode::update(uint32_t now) {
  if (phase_ == DONE) return;  // modes.cpp is switching back
  if (phase_ == ICON) {
    const bool iconOnly = queue[0].text.length() == 0;
    if (now - since_ < (iconOnly ? ICON_ONLY_MS : ICON_MS)) return drawIcon(now - since_);
    if (!iconOnly) {
      phase_ = TEXT;
      pager_.start(queue[0].text);
      return;
    }
  } else if (!pager_.update(now, interval(PAGE_MS))) {
    return;
  }
  // This one is over: the next, or back to what was on.
  for (int k = 0; k < queued - 1; k++) queue[k] = queue[k + 1];
  queue[--queued].text = "";
  if (queued) {
    display.beginTransition();
    begin(now);
  } else {
    phase_ = DONE;
  }
}
