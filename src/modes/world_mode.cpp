#include "modes/world_mode.h"

#include <math.h>

#include "bigdigits.h"
#include "display.h"
#include "settings.h"
#include "ui.h"
#include "world.h"

static const uint32_t PICTURE_MS = 6000;

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
static const int MAP_TOP = 4;

static int mapX(float lon) { return constrain((int)floorf((lon + 180) / 22.5f), 0, 15); }
static int mapY(float lat) { return MAP_TOP + constrain((int)floorf((90 - lat) / 22.5f), 0, 7); }

// A 5x10 rocket, nose up.
static const char *const ROCKET[10] = {"..#..", ".###.", ".###.", ".#.#.", ".###.", ".###.", ".###.", "#####", "##.##", "#...#"};

static bool available(const WorldInfo &w, int scene) {
  return scene == 0 ? w.airOk : scene == 1 ? w.issOk : w.launchOk;
}

static String sceneText(const WorldInfo &w, int scene) {
  switch (scene) {
    case 0: {
      String s = String("Aria ") + aqiBand(w.aqi) + " - indice europeo " + w.aqi;
      if (w.pm25 >= 0) s += ", PM2.5 " + String((int)lroundf(w.pm25));
      return s;
    }
    case 1: {
      float bearing;
      const float km = distanceKm(settings.latitude, settings.longitude, w.issLat, w.issLon, &bearing);
      const String dist = String((long)lroundf(km / 10) * 10);
      if (km < 1500) return "La Stazione spaziale passa sopra di te! A " + dist + " km, verso " + compassName(bearing);
      return "Stazione spaziale a " + dist + " km da qui, verso " + compassName(bearing);
    }
    default:
      return "Prossimo lancio: " + w.launchName + " " + countdownText((long)(w.launchTime - time(nullptr)));
  }
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
    if (now - sceneStart_ >= PICTURE_MS) {
      picture_ = false;
      scroller_.start(sceneText(w, scene_));
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
    case AIR: {
      // The index in big digits, and a gauge filling up to it: one pixel
      // per 6.25 points, the bands marked below.
      drawBigNumber(min(w.aqi, 99), 1, 230);
      const float filled = min(1.0f, t / 1200.0f) * min(w.aqi, 100) / 100.0f * COLS;
      for (int x = 0; x < COLS; x++) {
        const float f = filled - x;
        const uint8_t l = f >= 1 ? 220 : f > 0 ? (uint8_t)(220 * f) : 25;
        for (int y = 10; y <= 12; y++) display.setLevel(x, y, l);
      }
      for (int band = 1; band < 5; band++) display.setLevel(band * COLS / 5, 14, 90);
      break;
    }
    case ISS: {
      for (int y = 0; y < 8; y++) {
        for (int x = 0; x < COLS; x++) {
          if (MAP[y][x] == '#') display.setLevel(x, MAP_TOP + y, 35);
        }
      }
      for (int i = w.trailCount - 1; i >= 0; i--) {
        display.setLevel(mapX(w.trailLon[i]), mapY(w.trailLat[i]), 60 + 60 * (w.trailCount - i) / w.trailCount);
      }
      display.setLevel(mapX(settings.longitude), mapY(settings.latitude), 140);
      if ((now / 300) % 2) display.setLevel(mapX(w.issLon), mapY(w.issLat), 255);
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
