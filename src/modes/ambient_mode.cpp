#include "modes/ambient_mode.h"

#include <math.h>

#include "display.h"
#include "settings.h"
#include "timekeeping.h"

const AmbientMode::Animation AmbientMode::ANIMATIONS[] = {
    {"rain", "Pioggia digitale"},
    {"fire", "Fuoco"},
    {"stars", "Stelle"},
    {"waves", "Onde"},
    {"breath", "Respiro"},
};
const uint8_t AmbientMode::ANIMATION_COUNT = sizeof(ANIMATIONS) / sizeof(ANIMATIONS[0]);

enum { RAIN, FIRE, STARS, WAVES, BREATH };

static const uint16_t FRAME_MS[] = {60, 70, 100, 60, 40};
static const uint32_t AUTO_SWITCH_MS = 5 * 60 * 1000;

static float random01() { return (esp_random() & 0xFFFF) / 65535.0f; }

void AmbientMode::start() {
  autoIndex_ = 0;
  begin(pickAuto());
}

// Index of the animation to show: the chosen one, or in "auto" mode stars
// at night (22:00-07:00) and the rotation during the day.
uint8_t AmbientMode::pickAuto() const {
  for (uint8_t i = 0; i < ANIMATION_COUNT; i++) {
    if (settings.ambient == ANIMATIONS[i].id) return i;
  }
  struct tm t;
  if (localTime(t) && (t.tm_hour >= 22 || t.tm_hour < 7)) return STARS;
  return autoIndex_ % ANIMATION_COUNT;
}

void AmbientMode::begin(uint8_t animation) {
  animation_ = animation;
  animationStart_ = millis();
  lastFrame_ = 0;
  memset(heat_, 0, sizeof(heat_));
  memset(starLife_, 0, sizeof(starLife_));
  for (int x = 0; x < COLS; x++) {
    drops_[x] = {-random01() * 20, 0.25f + random01() * 0.5f, (uint8_t)(3 + esp_random() % 5)};
  }
}

void AmbientMode::action() {
  if (settings.ambient == "auto") {
    autoIndex_++;
    begin(autoIndex_ % ANIMATION_COUNT);
    return;
  }
  // A fixed animation was chosen: move the choice on to the next one.
  const uint8_t next = (pickAuto() + 1) % ANIMATION_COUNT;
  settings.ambient = ANIMATIONS[next].id;
  saveSettings();
  begin(next);
}

void AmbientMode::update(uint32_t now) {
  if (settings.ambient == "auto" && now - animationStart_ >= AUTO_SWITCH_MS) {
    autoIndex_++;
    begin(pickAuto());
  }
  if (now - lastFrame_ < FRAME_MS[animation_]) return;
  lastFrame_ = now;

  switch (animation_) {
    case RAIN: rain(now); break;
    case FIRE: fire(); break;
    case STARS: stars(); break;
    case WAVES: waves(now); break;
    case BREATH: breath(now); break;
  }
  display.render();
}

// "Matrix" rain: a falling drop with a tail in each column.
void AmbientMode::rain(uint32_t) {
  display.clear();
  for (int x = 0; x < COLS; x++) {
    Drop &d = drops_[x];
    d.y += d.speed;
    if (d.y - d.length > ROWS) {
      d = {-random01() * 10, 0.25f + random01() * 0.5f, (uint8_t)(3 + esp_random() % 5)};
    }
    const int head = (int)d.y;
    for (int i = 0; i < d.length; i++) display.setPixel(x, head - i, true);
  }
}

// Classic "doom fire": heat rises from the bottom row and cools on the way
// up; a pixel is lit where it is hot enough.
void AmbientMode::fire() {
  for (int x = 0; x < COLS; x++) heat_[ROWS - 1][x] = 150 + esp_random() % 106;
  for (int y = 0; y < ROWS - 1; y++) {
    for (int x = 0; x < COLS; x++) {
      const int from = x + (int)(esp_random() % 3) - 1;
      const int below = heat_[y + 1][(from + COLS) % COLS];
      const int cooled = below - (int)(esp_random() % 32);
      heat_[y][x] = cooled > 0 ? cooled : 0;
    }
  }
  display.clear();
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) display.setPixel(x, y, heat_[y][x] > 110);
  }
}

// Stars that appear, shine for a while and fade out.
void AmbientMode::stars() {
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      if (starLife_[y][x] > 0) starLife_[y][x]--;
    }
  }
  if (esp_random() % 3 == 0) {
    starLife_[esp_random() % ROWS][esp_random() % COLS] = 10 + esp_random() % 40;
  }
  display.clear();
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      // A short twinkle while a star is appearing or about to go out.
      const uint8_t life = starLife_[y][x];
      display.setPixel(x, y, life > 3 || (life > 0 && (esp_random() & 1)));
    }
  }
}

// Soft moving blobs from overlapping sine waves.
void AmbientMode::waves(uint32_t now) {
  const float t = now / 1000.0f;
  display.clear();
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      const float v = sinf(x * 0.45f + t) + sinf(y * 0.35f - t * 1.3f) + sinf((x + y) * 0.25f + t * 0.7f);
      display.setPixel(x, y, v > 0.6f);
    }
  }
}

// A circle that grows (breathe in, 4 s) and shrinks (breathe out, 6 s).
void AmbientMode::breath(uint32_t now) {
  const float phase = (now - animationStart_) % 10000 / 1000.0f;
  const float eased = phase < 4 ? (1 - cosf(phase / 4 * PI)) / 2 : (1 + cosf((phase - 4) / 6 * PI)) / 2;
  const float radius = 1 + eased * 8;
  display.clear();
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      const float dx = x - 7.5f, dy = y - 7.5f;
      display.setPixel(x, y, dx * dx + dy * dy <= radius * radius);
    }
  }
}
