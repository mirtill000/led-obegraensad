#include "modes/world_mode.h"

#include <math.h>

#include "display.h"
#include "settings.h"
#include "ui.h"
#include "world.h"

// How long each card stays, and the cross-fade between them.
static const uint32_t AIR_MS = 8000, ISS_MS = 10000;
static const uint16_t FADE_MS = 900;

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

static bool available(const WorldInfo &w, int card) { return card == 0 ? w.airOk : w.issOk; }

void WorldMode::start() {
  worldWanted();
  card_ = ISS;  // next() moves on to the air first
  next(millis(), false);
}

void WorldMode::next(uint32_t now, bool fade) {
  const WorldInfo w = worldInfoNow();
  const uint8_t before = card_;
  for (int tries = 0; tries < CARDS; tries++) {
    card_ = (card_ + 1) % CARDS;
    if (available(w, card_)) break;
  }
  if (fade && card_ != before) display.beginFade(FADE_MS);
  cardStart_ = now;
}

void WorldMode::update(uint32_t now) {
  worldWanted();
  if (now - lastDraw_ < 50) return;
  lastDraw_ = now;
  const WorldInfo w = worldInfoNow();
  display.clear();
  if (!w.airOk && !w.issOk) {
    ui::waiting(now);  // nothing downloaded yet
    display.render();
    return;
  }
  if (!available(w, card_) || now - cardStart_ >= (card_ == AIR ? AIR_MS : ISS_MS)) next(now, true);
  const uint32_t t = now - cardStart_;
  if (card_ == AIR) {
    // "Aria" above, the European index in the middle, its band below
    // (scrolling when the name is long).
    // The value in the text fonts' own 3x6 digits (as "Media" draws
    // them), centred in rows 5-10 between two blank rows: the three lines
    // read as one family. The words are dimmer so the number leads.
    word("Aria", 0, 110, t);
    const String value(w.aqi);
    const int width = Display::textWidthIn(TextFont::Short, value.c_str(), 0, value.length());
    display.drawTextIn(TextFont::Short, (COLS + 1 - width) / 2, 5, value.c_str(), 0, value.length());
    word(aqiBand(w.aqi), 12, 170, t);
  } else {
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
  }
  display.render();
}
