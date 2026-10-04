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


static bool available(const WorldInfo &w, int card) { return card == 0 ? w.airOk : w.issOk; }

String WorldMode::status() const {
  const WorldInfo w = worldInfoNow();
  String s;
  if (w.airOk) s = String("Aria ") + w.aqi + " " + aqiBand(w.aqi);
  if (w.issOk) {
    s += String(s.length() ? " · " : "") + "ISS a " +
         String((long)distanceKm(settings.latitude, settings.longitude, w.issLat, w.issLon)) + " km";
  }
  return s;
}

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
  if (fade && card_ != before) display.beginPageTransition(FADE_MS);
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
    // The shared card: "Aria", the European index, its band.
    ui::card("Aria", String(w.aqi), aqiBand(w.aqi), t);
  } else {
    for (int y = 0; y < 8; y++) {
      for (int x = 0; x < COLS; x++) {
        if (MAP[y][x] == '#') display.setLevel(x, mapTop() + y, ui::LEVEL_FAINT);
      }
    }
    for (int i = w.trailCount - 1; i >= 0; i--) {
      display.setLevel(mapX(w.trailLon[i]), mapY(w.trailLat[i]), 60 + 60 * (w.trailCount - i) / w.trailCount);
    }
    display.setLevel(mapX(settings.longitude), mapY(settings.latitude), 140);
    if ((now / 300) % 2) display.setLevel(mapX(w.issLon), mapY(w.issLat), 255);
    if (settings.vertical) ui::label("ISS", 0, ui::LEVEL_DIM, t);
  }
  display.render();
}
