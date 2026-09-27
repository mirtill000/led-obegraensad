#include "modes/world_mode.h"

#include <math.h>

#include "bigdigits.h"
#include "display.h"
#include "settings.h"
#include "ui.h"
#include "world.h"

// How long each picture stays. The air and the Station are only pictures;
// the launch is followed by its line of text.
static const uint32_t AIR_MS = 8000, ISS_MS = 10000, LAUNCH_MS = 6000;

// The world, 16 x 8 (22.5 degrees a cell), from 180 W and from the pole.
static const char *const MAP[8] = {
    "....#.#.....#...",  // 79 N
    ".####...#######.",  // 56 N
    "..###..#######..",  // 34 N
    "....#..###.##...",  // 11 N
    "....##..##...##.",  // 11 S
    "....##..#....##.",  // 34 S
    ".....#..........",  // 56 S
    "################",  // Antarctica
};
// Centred; with the lamp vertical a row lower, under "ISS".
static int mapTop() { return settings.vertical ? 5 : 4; }

static int mapX(float lon) { return constrain((int)floorf((lon + 180) / 22.5f), 0, 15); }
static int mapY(float lat) { return mapTop() + constrain((int)floorf((90 - lat) / 22.5f), 0, 7); }

// A word in the 4-pixel font (the rows below `y` must be empty), centred;
// scrolling round when it is wider than the panel.
static void word(const char *text, int y, uint8_t level, uint32_t t) {
  const String plain = Display::fontText(text);
  String s;
  for (unsigned k = 0; k < plain.length(); k++) s += (char)toupper((uint8_t)plain[k]);  // capitals only
  const int w = Display::textWidthIn(TextFont::Tiny, s.c_str(), 0, s.length());
  if (w <= COLS + 1) {
    display.drawTextIn(TextFont::Tiny, (COLS + 1 - w) / 2, y, s.c_str(), 0, s.length());
  } else {
    const int period = w + 6;  // a gap before it comes round again
    const int offset = (int)(t / 90) % period;
    display.drawTextIn(TextFont::Tiny, -offset, y, s.c_str(), 0, s.length());
    display.drawTextIn(TextFont::Tiny, period - offset, y, s.c_str(), 0, s.length());
  }
  // The font draws at full brightness: bring its rows down to `level`.
  for (int r = y; r < y + Display::fontHeightOf(TextFont::Tiny); r++) {
    for (int x = 0; x < COLS; x++) {
      if (display.getLevel(x, r)) display.setLevel(x, r, level);
    }
  }
}

// A 5x10 rocket, nose up.
static const char *const ROCKET[10] = {"..#..", ".###.", ".###.", ".#.#.", ".###.", ".###.", ".###.", "#####", "##.##", "#...#"};

static bool available(const WorldInfo &w, int scene) {
  return scene == 0 ? w.airOk : scene == 1 ? w.issOk : w.launchOk;
}

static String launchText(const WorldInfo &w) {
  return "Prossimo lancio: " + w.launchName + " " + countdownText((long)(w.launchTime - time(nullptr)));
}

void WorldMode::start() {
  worldWanted();
  scene_ = LAUNCH;  // next() moves on to the air first
  next(millis());
}

void WorldMode::next(uint32_t now) {
  const WorldInfo w = worldInfoNow();
  for (int tries = 0; tries < SCENES; tries++) {
    scene_ = (scene_ + 1) % SCENES;
    if (available(w, scene_)) break;
  }
  picture_ = true;
  sceneStart_ = now;
}

void WorldMode::update(uint32_t now) {
  worldWanted();
  const WorldInfo w = worldInfoNow();
  if (!w.airOk && !w.issOk && !w.launchOk) {
    // Nothing downloaded yet.
    if (now - lastDraw_ < 50) return;
    lastDraw_ = now;
    display.clear();
    ui::waiting(now);
    display.render();
    return;
  }
  if (scene_ >= SCENES || !available(w, scene_)) return next(now);
  if (picture_) {
    const uint32_t shown = scene_ == AIR ? AIR_MS : scene_ == ISS ? ISS_MS : LAUNCH_MS;
    if (now - sceneStart_ >= shown) {
      if (scene_ != LAUNCH) return next(now);
      picture_ = false;
      scroller_.start(launchText(w));
      row_ = Scroller::rowFor(settings.webPosition == "pages" ? String("middle") : settings.webPosition, row_);
      scroller_.setRow(row_);
      return;
    }
    if (now - lastDraw_ < 50) return;
    lastDraw_ = now;
    drawPicture(now);
    return;
  }
  if (scroller_.update(now, interval(SCROLL_DELAY_MS))) next(now);
}

void WorldMode::drawPicture(uint32_t now) {
  const WorldInfo w = worldInfoNow();
  const uint32_t t = now - sceneStart_;
  display.clear();
  switch (scene_) {
    case AIR:
      // "Aria" above, the European index in the middle, its band below
      // (scrolling when the name is long).
      word("Aria", 0, 150, t);
      drawBigNumber(min(w.aqi, 99), 5, 255);
      word(aqiBand(w.aqi), 12, 200, t);
      break;
    case ISS: {
      for (int y = 0; y < 8; y++) {
        for (int x = 0; x < COLS; x++) {
          if (MAP[y][x] == '#') display.setLevel(x, mapTop() + y, 35);
        }
      }
      for (int i = w.trailCount - 1; i >= 0; i--) {
        display.setLevel(mapX(w.trailLon[i]), mapY(w.trailLat[i]), 60 + 60 * (w.trailCount - i) / w.trailCount);
      }
      display.setLevel(mapX(settings.longitude), mapY(settings.latitude), 140);
      if ((now / 300) % 2) display.setLevel(mapX(w.issLon), mapY(w.issLat), 255);
      if (settings.vertical) word("ISS", 0, 150, t);
      break;
    }
    default: {
      // Lift-off: the rocket rises with a flickering flame and smoke.
      const float rise = t < 1500 ? 0 : (t - 1500) / 1000.0f * (t - 1500) / 1000.0f * 3;
      const int top = 5 - (int)rise;
      for (int r = 0; r < 10; r++) {
        for (int c = 0; c < 5; c++) {
          if (ROCKET[r][c] == '#') display.setLevel(6 + c, top + r, 200);
        }
      }
      for (int k = 0; k < 3; k++) {
        const int fy = top + 10 + k;
        const uint8_t l = (uint8_t)((255 - k * 70) * (0.6f + 0.4f * ((now / 60 + k) % 3) / 2));
        display.setLevel(8, fy, l);
        if (k < 2) display.setLevel(7 + (now / 90 + k) % 3, fy, l / 2);
      }
      // Smoke on the pad, spreading after ignition.
      const int spread = t < 1000 ? 0 : min(7, (int)((t - 1000) / 250));
      for (int dx = -spread; dx <= spread; dx++) display.setLevel(8 + dx, 15, 60 + (dx * 37 + now / 100) % 40);
      break;
    }
  }
  display.render();
}
