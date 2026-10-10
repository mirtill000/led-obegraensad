// "Bonsai": a little tree in a pot, on the balcony. It drinks - more on hot
// days, none when it rains (the real weather waters it) -, grows a little
// every day it is well, leans its leaves towards the sun (from the left in
// the morning, from the right in the afternoon), blossoms in spring, turns
// sparse and drops leaves in autumn, rests in winter. Left alone it sends
// out stray shoots that want pruning, wilts when thirsty and rots with too
// much water; fertiliser makes it grow twice as fast for a day, but more
// than once in three days burns its roots. It never dies.
//   L annaffia, R pota, U concima
#include <math.h>

#include "modes/creature.h"
#include "sound.h"
#include "timekeeping.h"
#include "weather.h"

namespace {

struct BonsaiLife {
  CreatureClock clock;
  float water = 70, health = 85;
  float growth = 1.5f;      // 0..GROWN
  float shoots = 0;         // stray shoots, 0..MAX_SHOOTS
  uint32_t ageMin = 0;
  uint32_t fedAgoMin = 99999;  // since the last fertiliser
  uint16_t fedLeftMin = 0;     // fertiliser still working
  uint8_t sorrow = 0;          // 1 burnt by fertiliser, 2 drowned (for the mood, until cared for)
};

const float GROWN = 14;
const float MAX_SHOOTS = 5;

float hash01(int a, int b) {
  uint32_t h = (uint32_t)a * 0x8da6b343u ^ (uint32_t)b * 0xd8163841u;
  h ^= h >> 15;
  h *= 0x2c1b3c6du;
  h ^= h >> 12;
  return (h >> 8) / 16777216.0f;
}

bool raining() {
  const Weather w = weatherNow();
  return w.valid && ((w.code >= 51 && w.code <= 67) || (w.code >= 80 && w.code <= 82) || w.code >= 95);
}

enum Season : uint8_t { SPRING, SUMMER, AUTUMN, WINTER };
Season seasonOf(int month) {  // month 0-11
  if (month >= 2 && month <= 4) return SPRING;
  if (month >= 5 && month <= 8) return SUMMER;
  if (month >= 9 && month <= 10) return AUTUMN;
  return WINTER;
}

enum Anim : uint8_t { NONE, WATER, PRUNE, FEED, REFUSE };
const uint32_t ANIM_MS[] = {0, 2200, 1600, 1600, 600};

class BonsaiMode : public Creature {
 public:
  const char *id() const override { return "bonsai"; }
  const char *name() const override { return "Bonsai"; }
  const char *actionName() const override { return "Annaffia"; }
  void action() override { input('L'); }
  const GameControls *keys() const override {
    static const GameControls c = {"LRU", {"Annaffia", "Pota", "Concima", nullptr, nullptr}, false,
                                   "Tastiera: ← annaffia, → pota, ↑ concima."};
    return &c;
  }
  const char *resetName() const override { return "Nuova piantina"; }
  const char *resetQuestion() const override { return "Il bonsai lascerà il posto a una piantina nuova. Continuare?"; }

  void start() override {
    load();
    anim_ = NONE;
    display.beginTransition();
  }

  void update(uint32_t now) override {
    if (now - lastDraw_ < 40) return;
    lastDraw_ = now;
    draw(now);
  }

  Info info() override {
    load();
    Info i;
    const uint32_t days = life_.ageMin / (24 * 60);
    i.title = String("Bonsai · ") + (days == 1 ? "1 giorno" : String(days) + " giorni");
    i.add("Acqua", life_.water, 20);
    i.add("Salute", life_.health, 40);
    i.add("Crescita", life_.growth / GROWN * 100, 0);
    struct tm t;
    const bool clock = localTime(t);
    const Season season = clock ? seasonOf(t.tm_mon) : SUMMER;
    if (life_.sorrow == 1) i.mood = "Troppo concime: le radici soffrono";
    else if (life_.sorrow == 2 || life_.water > 95) i.mood = "Troppa acqua: aspetta che asciughi";
    else if (life_.water < 15) i.mood = "Ha sete: annaffialo";
    else if (life_.health < 40) i.mood = "Sta male: acqua giusta e pazienza";
    else if (raining()) i.mood = "Beve la pioggia";
    else if (life_.shoots >= 3) i.mood = "Ha rami ribelli: va potato";
    else if (clock && (t.tm_hour < 7 || t.tm_hour >= 20)) i.mood = "Riposa per la notte";
    else if (season == SPRING && life_.health > 60) i.mood = "È in fiore";
    else if (season == WINTER) i.mood = "Riposa per l'inverno";
    else if (season == AUTUMN) i.mood = "Perde qualche foglia";
    else if (life_.growth >= GROWN) i.mood = "È un bonsai adulto, rigoglioso";
    else i.mood = "Cresce bene";
    return i;
  }

 protected:
  uint8_t *state() override { return (uint8_t *)&life_; }
  size_t stateSize() const override { return sizeof(life_); }
  uint8_t version() const override { return 1; }
  void fresh() override { life_ = BonsaiLife(); }

  void liveMinute(const struct tm *t) override {
    life_.ageMin++;
    life_.fedAgoMin++;
    const int hour = t ? t->tm_hour : 12;
    const bool day = hour >= 7 && hour < 20;
    const Season season = t ? seasonOf(t->tm_mon) : SUMMER;
    const Weather w = weatherNow();
    const bool hot = w.valid && w.temperature > 26, cold = w.valid && w.temperature < 3;
    // Drinking: about half the pot a day, more in the heat; rain fills it.
    life_.water -= day ? (hot ? 0.09f : 0.05f) : 0.02f;
    if (season == WINTER) life_.water += 0.015f;  // it drinks little in winter
    if (raining()) life_.water += 0.3f;
    life_.water = constrain(life_.water, 0.0f, 100.0f);
    // Health: thirst and soaking hurt, the right water heals.
    if (life_.water < 15) life_.health -= 0.05f;
    else if (life_.water > 95) life_.health -= 0.02f;
    else if (life_.water > 25 && life_.water < 85) life_.health += 0.03f;
    life_.health = constrain(life_.health, 0.0f, 100.0f);
    if (life_.sorrow && life_.health > 60) life_.sorrow = 0;
    if (life_.sorrow == 2 && life_.water < 85) life_.sorrow = 0;
    // Growth: in daylight, when well, not in the frost; fertiliser doubles it.
    if (day && life_.health > 50 && !cold && season != WINTER && life_.growth < GROWN) {
      const float g = (life_.fedLeftMin ? 2.0f : 1.0f) / 600;
      life_.growth = min(GROWN, life_.growth + g);
      life_.shoots = min(MAX_SHOOTS, life_.shoots + g * 0.7f);
    } else if (day && life_.health > 50 && life_.growth >= GROWN) {
      life_.shoots = min(MAX_SHOOTS, life_.shoots + 0.6f / 600);  // grown: only shoots
    }
    if (life_.fedLeftMin) life_.fedLeftMin--;
  }

  bool care(char key) override {
    const uint32_t ms = millis();
    if (anim_ != NONE && ms - animStart_ < ANIM_MS[anim_]) return false;
    Anim next = REFUSE;
    switch (key) {
      case 'L':
        if (life_.water > 85) {
          life_.water = 100;
          life_.health = max(0.0f, life_.health - 6);
          life_.sorrow = 2;
        } else {
          life_.water = min(100.0f, life_.water + 45);
        }
        next = WATER;
        break;
      case 'R':
        if (life_.shoots >= 1) {
          pruned_ = (int)life_.shoots;
          life_.shoots = 0;
          life_.health = min(100.0f, life_.health + 3);
          next = PRUNE;
        }
        break;
      case 'U':
        if (life_.fedAgoMin < 3 * 24 * 60) {
          life_.health = max(0.0f, life_.health - 12);
          life_.sorrow = 1;
        } else {
          life_.fedLeftMin = 24 * 60;
        }
        life_.fedAgoMin = 0;
        next = FEED;
        break;
    }
    anim_ = next;
    animStart_ = ms;
    static const sound::Id SOUND[] = {sound::CLICK, sound::WATER, sound::SNIP, sound::SPARKLE, sound::NO};
    sound::play(SOUND[next]);
    return next != REFUSE;
  }

 private:
  BonsaiLife life_;
  Anim anim_ = NONE;
  uint32_t animStart_ = 0, lastDraw_ = 0;
  int pruned_ = 0;

  void draw(uint32_t now) {
    const float t = now / 1000.0f;
    const uint32_t at = anim_ != NONE ? now - animStart_ : 0;
    if (anim_ != NONE && at >= ANIM_MS[anim_]) anim_ = NONE;
    struct tm tm;
    const bool clock = localTime(tm);
    const int hour = clock ? tm.tm_hour : 12;
    const bool night = hour < 7 || hour >= 20;
    const Season season = clock ? seasonOf(tm.tm_mon) : SUMMER;
    const float side = hour < 13 ? -1 : 1;  // where the sun comes from
    const float dim = night ? 0.45f : 1;
    Canvas c;
    c.fill(0);
    // Rain falling past.
    if (raining()) {
      for (int i = 0; i < 6; i++) {
        const float x = hash01(i, 1) * 16, y = fmodf(t * 9 + hash01(i, 2) * 16, 16);
        c.put(x, y, 0.2f);
      }
    }

    const int shake = anim_ == REFUSE ? ((at / 80) % 2 ? 1 : -1) : 0;
    // Pot and soil.
    for (int x = 3; x <= 12; x++) c.set(x, 13, 0.2f * dim);
    for (int x = 4; x <= 11; x++) c.set(x, 14, 0.12f * dim);
    for (int x = 5; x <= 10; x++) c.set(x, 15, 0.1f * dim);
    c.set(5, 15, 0);
    c.set(10, 15, 0);
    for (int x = 4; x <= 11; x++) c.set(x, 12, life_.water > 60 && (x * 7 + now / 400) % 9 == 0 ? 0.2f * dim : 0);  // wet soil glints

    // The tree grows from the pot: trunk, branches, three pads of leaves.
    const float s = 0.35f + 0.65f * life_.growth / GROWN;
    const float bx = 7.5f + shake, by = 12;
    static const float TRUNK[][2] = {{0, 0}, {-1, -2.5f}, {1, -5}, {0, -8}};
    for (int k = 0; k < 3; k++) {
      for (float u = 0; u < 1; u += 0.1f) {
        const float x = bx + s * (TRUNK[k][0] + (TRUNK[k + 1][0] - TRUNK[k][0]) * u);
        const float y = by + s * (TRUNK[k][1] + (TRUNK[k + 1][1] - TRUNK[k][1]) * u);
        c.lift((int)lroundf(x - 0.5f), (int)y, 0.16f * dim);
        if (k == 0 && u < 0.5f) c.lift((int)lroundf(x + 0.5f), (int)y, 0.12f * dim);  // a thicker base
      }
    }
    const bool droop = life_.water < 15 || life_.health < 30;
    struct Pad {
      float x, y, rx, ry;
    } pads[3] = {{-3.5f, -4.5f, 2.6f, 1.3f}, {3.5f, -6.3f, 2.8f, 1.4f}, {0, -9.2f, 3.2f, 1.7f}};
    const float vigor = (0.45f + 0.55f * life_.health / 100) * dim;
    const float density = season == WINTER ? 0.55f : season == AUTUMN ? 0.7f : 0.92f;
    for (int i = 0; i < 3; i++) {
      Pad &p = pads[i];
      p.x = bx + p.x * s;
      p.y = by + p.y * s + (droop ? 1 : 0);
      p.rx *= 0.5f + 0.5f * s;
      p.ry *= 0.55f + 0.45f * s;
      // The branch out to it.
      for (float u = 0; u < 1; u += 0.15f) {
        const float x = bx + s * TRUNK[2][0] * 0.6f + (p.x - bx) * u, y = by + s * TRUNK[1][1] + (p.y - by - s * TRUNK[1][1]) * u;
        c.lift((int)x, (int)y, 0.13f * dim);
      }
    }
    for (int i = 0; i < 3; i++) {
      const Pad &p = pads[i];
      for (int y = (int)(p.y - p.ry - 1); y <= (int)(p.y + p.ry + 1); y++) {
        for (int x = (int)(p.x - p.rx - 1); x <= (int)(p.x + p.rx + 1); x++) {
          const float dx = (x + 0.5f - p.x) / p.rx, dy = (y + 0.5f - p.y) / p.ry;
          if (dx * dx + dy * dy > 1 || y >= 12) continue;
          const float r = hash01(x * 3 + i, y);
          if (r > density) continue;
          // Lit on the sun's side and on top; a slow shimmer in the breeze.
          const float light = 0.55f + 0.25f * dx * side * (night ? 0 : 1) - 0.2f * dy;
          const float shimmer = 0.9f + 0.1f * sinf(t * 1.3f + x * 0.9f + y * 1.7f);
          c.set(x, y, constrain(light, 0.25f, 1.0f) * shimmer * vigor * (season == WINTER ? 0.7f : 1));
          // Blossoms in spring.
          if (season == SPRING && life_.health > 60 && r < 0.13f) c.set(x, y, (0.75f + 0.25f * sinf(t * 2 + r * 50)) * dim);
        }
      }
    }
    // Stray shoots, sticking out of the pads.
    const int shoots = anim_ == PRUNE ? (at < 500 ? pruned_ : 0) : (int)life_.shoots;
    static const int8_t SHOOT[][3] = {{0, -1, -1}, {1, 1, -1}, {2, 0, -1}, {0, -1, 0}, {1, 1, 0}};  // pad, dx, dy
    for (int k = 0; k < shoots; k++) {
      const Pad &p = pads[SHOOT[k][0]];
      const float dx = SHOOT[k][1], dy = SHOOT[k][2];
      const float x0 = p.x + dx * p.rx, y0 = p.y + (dy ? -p.ry : 0);
      for (int j = 1; j <= 3; j++) c.lift((int)(x0 + dx * j * 0.8f), (int)(y0 + (dy ? -j : -j * 0.4f)), (j == 3 ? 0.45f : 0.2f) * dim);
    }

    // Autumn leaves, and the ones the scissors cut, falling.
    if (season == AUTUMN && !night) {
      for (int i = 0; i < 3; i++) {
        const float ph = fmodf(t * 0.25f + i * 0.33f, 1);
        c.put(pads[i].x + sinf(ph * 9 + i) * 1.5f, pads[i].y + ph * 9, 0.3f * (1 - ph));
      }
    }

    switch (anim_) {
      case WATER:
        // The watering can, top left, pouring into the pot.
        for (int x = 0; x < 3; x++) {
          c.set(x, 1, 0.45f);
          c.set(x, 2, 0.45f);
        }
        c.set(3, 1, 0.3f);
        c.set(4, 0, 0.3f);  // the spout
        for (int i = 0; i < 10; i++) {
          const float ph = fmodf(at / 450.0f + i / 10.0f, 1);
          c.put(4.5f - ph * 0.5f, 1 + ph * 11, 0.7f);
        }
        break;
      case PRUNE:
        for (int i = 0; i < pruned_ * 2; i++) {
          const float ph = at / (float)ANIM_MS[PRUNE];
          const Pad &p = pads[SHOOT[i / 2][0]];
          c.put(p.x + (i % 2 ? 1.5f : -1.5f) + sinf(ph * 8 + i), p.y + ph * 8, 0.5f * (1 - ph));
        }
        break;
      case FEED:
        // Sparkles rising from the soil (red ones when it was too much).
        for (int i = 0; i < 6; i++) {
          const float ph = fmodf(at / 900.0f + i / 6.0f, 1);
          c.put(4.5f + hash01(i, 7) * 7, 12 - ph * 8, (life_.sorrow == 1 ? 0.3f : 0.8f) * (1 - ph));
        }
        break;
      default: break;
    }

    // What it needs, blinking in the corner: a drop when thirsty.
    if (anim_ == NONE && life_.water < 15 && (now / 700) % 2) {
      c.set(1, 0, 0.8f);
      c.set(0, 1, 0.8f);
      c.set(1, 1, 0.8f);
      c.set(2, 1, 0.8f);
      c.set(1, 2, 0.8f);
    }
    c.show();
  }
};

BonsaiMode bonsai;

}  // namespace

Creature *const bonsaiCreature = &bonsai;
