// "Gatto": a cat living on the lamp. By day it strolls about, sits and
// licks its paw, and now and then sits right in front of the clock,
// hiding a digit; at night (22-7, or while the lamp is in its night
// mode) it sleeps curled up. When a notification has come, it wakes up and
// bats it about for a while. It gets hungry; it wants attention, and if it
// gets too little it sulks, its back to you, ignoring laser and yarn until
// you stroke it back into a good mood. It never dies.
//   L pappa, R laser, U gomitolo, A carezza
#include <math.h>

#include "modes/creature.h"
#include "sound.h"
#include "modes/notify_mode.h"
#include "sprite_atlas.h"
#include "timekeeping.h"

namespace {

struct CatLife {
  CreatureClock clock;
  float food = 70, love = 70;
  uint32_t ageMin = 0;
  bool sulking = false;
  char name[16] = "Micio";
};

// Digits 3x5 for the clock it sits in front of.
const uint16_t DIGITS[10] = {0x7B6F, 0x2C97, 0x73E7, 0x73CF, 0x5BC9, 0x79CF, 0x79EF, 0x7249, 0x7BEF, 0x7BCF};

void digit(Canvas &c, int x, int y, int d, float v) {
  for (int r = 0; r < 5; r++) {
    for (int k = 0; k < 3; k++) {
      if (DIGITS[d] >> (14 - r * 3 - k) & 1) c.set(x + k, y + r, v);
    }
  }
}

enum Act : uint8_t { WANDER, SIT, GROOM, CLOCK, SLEEP, SULK, EAT, LASER, YARN, PURR, NOTE, REFUSE };

const int FLOOR = ROWS - 1;  // the cat stands on row 15

bool nightNow() {
  struct tm t;
  if (localTime(t) && (t.tm_hour >= 22 || t.tm_hour < 7)) return true;
  return isNight();
}

float eyeMark(char m, void *ctx) { return *(float *)ctx; }

class CatMode : public Creature {
 public:
  const char *id() const override { return "cat"; }
  const char *name() const override { return "Gatto"; }
  const char *actionName() const override { return "Dai la pappa"; }
  void action() override { input('L'); }
  const GameControls *keys() const override {
    static const GameControls c = {"LRUA", {"Pappa", "Laser", "Gomitolo", nullptr, "Carezza"}, false,
                                   "Tastiera: ← pappa, → laser, ↑ gomitolo, spazio carezza."};
    return &c;
  }
  const char *resetName() const override { return "Nuovo gattino"; }
  const char *resetQuestion() const override { return "Arriverà un gattino nuovo al posto di questo. Continuare?"; }
  String petName() override {
    load();
    return life_.name;
  }
  void rename(const String &name) override {
    load();
    String n = name;
    n.trim();
    if (!n.length()) return;
    strncpy(life_.name, n.c_str(), sizeof(life_.name) - 1);
    life_.name[sizeof(life_.name) - 1] = 0;
    save();
  }

  void start() override {
    load();
    x_ = 4;
    pick(millis());
    display.beginTransition();
  }

  void update(uint32_t now) override {
    if (now - lastDraw_ < 40) return;
    const float dt = lastDraw_ ? min(0.2f, (now - lastDraw_) / 1000.0f) : 0.04f;
    lastDraw_ = now;
    // A notification shown while it was away: it wakes up to play with it.
    if (sawNote_ && act_ != NOTE && act_ != EAT && act_ != PURR) {
      sawNote_ = false;
      begin(NOTE, now, 6000);
      sound::play(sound::MEOW);
    }
    if (now >= until_) pick(now);
    move(now, dt);
    draw(now);
  }

  Info info() override {
    load();
    Info i;
    const uint32_t days = life_.ageMin / (24 * 60);
    i.title = String(life_.name) + " · " + (days == 1 ? "1 giorno" : String(days) + " giorni");
    i.add("Pancia", life_.food, 25);
    i.add("Affetto", life_.love, 25);
    if (life_.sulking) i.mood = "Ti tiene il muso: fagli una carezza";
    else if (life_.food < 25) i.mood = "Ha fame e miagola";
    else if (nightNow()) i.mood = "Dorme acciambellato";
    else if (life_.love < 40) i.mood = "Si sente un po' trascurato";
    else if (life_.love > 75) i.mood = "Fa le fusa";
    else i.mood = "Gironzola tranquillo";
    return i;
  }


 protected:
  uint8_t *state() override { return (uint8_t *)&life_; }
  size_t stateSize() const override { return sizeof(life_); }
  uint8_t version() const override { return 1; }
  void fresh() override { life_ = CatLife(); }

  // A notification came and went while it was on the lamp or not.
  void watch() override {
    const bool on = NotifyMode::pending() > 0;
    if (noteWas_ && !on) sawNote_ = true;
    noteWas_ = on;
  }

  void liveMinute(const struct tm *t) override {
    life_.ageMin++;
    const bool asleep = t && (t->tm_hour >= 22 || t->tm_hour < 7);
    life_.food = max(0.0f, life_.food - (asleep ? 2.0f : 6.0f) / 60);
    life_.love = max(0.0f, life_.love - (asleep ? 0.5f : 3.0f) / 60 - (life_.food < 20 ? 2.0f / 60 : 0));
    if (life_.love < 20) life_.sulking = true;
  }

  bool care(char key) override {
    const uint32_t now = millis();
    if ((act_ == EAT || act_ == LASER || act_ == YARN || act_ == PURR) && now < until_) return false;
    const bool asleep = nightNow() && act_ == SLEEP;
    switch (key) {
      case 'L':
        if (life_.food > 90) break;  // not hungry: it sniffs and walks off
        life_.food = min(100.0f, life_.food + 40);
        life_.love = min(100.0f, life_.love + 3);
        begin(EAT, now, 3500);
        sound::play(sound::MEOW);
        return true;
      case 'R':
      case 'U':
        if (life_.sulking || asleep) break;  // it ignores you
        life_.love = min(100.0f, life_.love + (key == 'R' ? 10 : 8));
        life_.food = max(0.0f, life_.food - 3);
        if (key == 'R') {
          laser_ = x_ + 6;
          begin(LASER, now, 5000);
        } else {
          ball_ = -2;
          ballV_ = 5;
          begin(YARN, now, 5000);
        }
        sound::play(sound::CLICK);
        return true;
      case 'A':
        life_.love = min(100.0f, life_.love + 12);
        if (life_.sulking && life_.love >= 40) life_.sulking = false;
        begin(life_.sulking ? SULK : PURR, now, 2500);
        sound::play(life_.sulking ? sound::NO : sound::PURR);
        return true;
    }
    begin(REFUSE, now, 900);
    sound::play(sound::NO);
    return false;
  }

 private:
  CatLife life_;
  bool sawNote_ = false, noteWas_ = false;
  Act act_ = SIT;
  uint32_t since_ = 0, until_ = 0, lastDraw_ = 0;
  float x_ = 4, target_ = 4;
  bool right_ = true;
  float laser_ = 0, ball_ = 0, ballV_ = 0;

  void begin(Act a, uint32_t now, uint32_t ms) {
    act_ = a;
    since_ = now;
    until_ = now + ms;
  }

  // What it does next, by itself.
  void pick(uint32_t now) {
    if (life_.sulking) return begin(SULK, now, 4000);
    if (nightNow()) return begin(SLEEP, now, 5000);
    const uint32_t r = esp_random() % 10;
    if (r < 4) {
      target_ = esp_random() % (COLS - 8);
      begin(WANDER, now, 6000);
    } else if (r < 6) {
      begin(SIT, now, 4000 + esp_random() % 3000);
    } else if (r < 8) {
      begin(GROOM, now, 3000);
    } else {
      target_ = esp_random() % 8;  // in front of a digit or two
      begin(CLOCK, now, 9000);
    }
  }

  void move(uint32_t now, float dt) {
    float goal = x_, speed = 3;
    switch (act_) {
      case WANDER:
      case CLOCK: goal = target_; break;
      case EAT: goal = COLS - 12; break;
      case LASER: {
        // The dot darts about; the cat runs after it.
        const float t = (now - since_) / 1000.0f;
        laser_ = 6.5f + 6 * sinf(t * 1.7f) + 2 * sinf(t * 4.3f);
        goal = laser_ - 6;
        speed = 9;
        break;
      }
      case YARN:
        // The ball rolls in, the cat bats it back.
        ball_ += ballV_ * dt;
        if (ball_ > COLS - 2) ballV_ = -fabsf(ballV_);
        if (ball_ < x_ + 9 && ballV_ < 0) ballV_ = 4 + (esp_random() % 3);
        goal = min(ball_ - 9, (float)COLS - 9);
        speed = 6;
        break;
      default: return;
    }
    goal = constrain(goal, 0.0f, (float)COLS - 9);
    if (fabsf(goal - x_) > 0.2f) {
      right_ = goal > x_;
      x_ += (goal > x_ ? 1 : -1) * min(speed * dt, fabsf(goal - x_));
    }
  }

  bool walking() const {
    switch (act_) {
      case WANDER:
      case CLOCK: return fabsf(target_ - x_) > 0.2f;
      case LASER:
      case YARN: return true;
      case EAT: return x_ < COLS - 12.2f;
      default: return false;
    }
  }

  void draw(uint32_t now) {
    const uint32_t at = now - since_;
    Canvas c;
    c.fill(0);
    for (int x = 0; x < COLS; x++) c.set(x, FLOOR, 0.06f);
    const float body = act_ == SLEEP ? 0.4f : 0.75f;
    int x = (int)lroundf(x_);

    // The clock behind it, when it comes to sit in front of it.
    if (act_ == CLOCK) {
      struct tm t;
      if (localTime(t)) {
        digit(c, 1, 7, t.tm_hour / 10, 0.3f);
        digit(c, 5, 7, t.tm_hour % 10, 0.3f);
        digit(c, 9, 7, t.tm_min / 10, 0.3f);
        digit(c, 13, 7, t.tm_min % 10, 0.3f);
      }
    }
    if (act_ == EAT) {
      // The bowl, on the right, emptying.
      for (int k = 0; k < 4; k++) c.set(COLS - 4 + k, FLOOR - 1, 0.3f);
      c.set(COLS - 4, FLOOR - 2, 0.3f);
      c.set(COLS - 1, FLOOR - 2, 0.3f);
      if (at < 2500) {
        c.set(COLS - 3, FLOOR - 2, 0.6f);
        c.set(COLS - 2, FLOOR - 2, 0.6f);
      }
    }

    float eye = 0;  // open
    const bool blink = (now % 3500) < 140;
    if (act_ == PURR || blink) eye = body;
    if (act_ == SLEEP) {
      c.sprite(spr::CAT_SLEEP, 4, FLOOR - 4, 0, body);
      // A "z" floating up.
      const int ph = (now / 600) % 4;
      c.set(11, 9 - ph, 0.3f);
      c.set(12, 9 - ph, 0.3f);
      c.set(12, 10 - ph, 0.3f);
      c.set(11, 11 - ph, 0.3f);
      c.set(12, 11 - ph, 0.3f);
    } else if (act_ == SULK) {
      c.sprite(spr::CAT_BACK, x + 2, FLOOR - 7, 0, body);
      // Its tail flicking, annoyed.
      const int flick = (now / 250) % 2;
      c.set(x + 1 + flick * 6, FLOOR - 1, body);
      c.set(x + flick * 8, FLOOR - 2, body * 0.6f);
    } else if (walking()) {
      c.sprite(spr::CAT_WALK, x, FLOOR - 6, (now / 160) % 2, body, !right_, eyeMark, &eye);
    } else {
      const int sx = right_ ? x + 2 : x;
      c.sprite(spr::CAT_SIT, sx, FLOOR - 7, 0, body, !right_, eyeMark, &eye);
      // Its tail swishing slowly.
      const float sw = sinf(now / 400.0f);
      c.put(right_ ? sx - 1.0f + sw * 0.5f : sx + 7.0f - sw * 0.5f, FLOOR - 2.5f, body * 0.6f);
      if (act_ == GROOM && (at / 300) % 2) c.set(right_ ? sx + 5 : sx + 1, FLOOR - 4, body);  // the paw up to its face
    }

    switch (act_) {
      case LASER: c.put(laser_, FLOOR - 0.5f, 1); break;
      case YARN:
        c.put(ball_, FLOOR - 1.5f, 0.5f);
        c.put(ball_ + 1, FLOOR - 1.5f, 0.5f);
        break;
      case PURR: {
        const float ph = at / 2500.0f;
        c.put(x + 4.5f + sinf(ph * 9) * 0.5f, FLOOR - 8 - ph * 6, 0.8f * (1 - ph));  // a little heart rising
        c.put(x + 5.5f + sinf(ph * 9) * 0.5f, FLOOR - 8 - ph * 6, 0.8f * (1 - ph));
        break;
      }
      case NOTE: {
        // The notification as an envelope, flicked about.
        const float ph = at / 1000.0f;
        const float ex = 9 + 3 * sinf(ph * 2.3f), ey = 6 + 2.5f * fabsf(sinf(ph * 3.1f));
        for (int k = 0; k < 4; k++) {
          c.set((int)ex + k, (int)ey, 0.7f);
          c.set((int)ex + k, (int)ey + 2, 0.7f);
        }
        c.set((int)ex, (int)ey + 1, 0.7f);
        c.set((int)ex + 3, (int)ey + 1, 0.7f);
        c.set((int)ex + 1 + ((int)(ph * 4) % 2), (int)ey + 1, 0.4f);
        break;
      }
      case REFUSE:
        // A little "?" over it.
        c.set(x + 5, FLOOR - 11, 0.5f);
        c.set(x + 6, FLOOR - 12, 0.5f);
        c.set(x + 7, FLOOR - 11, 0.5f);
        c.set(x + 6, FLOOR - 10, 0.5f);
        c.set(x + 6, FLOOR - 8, 0.5f);
        break;
      default: break;
    }
    // What it needs, blinking in the corner: hungry.
    if (act_ != EAT && life_.food < 25 && (now / 700) % 2) {
      c.set(0, 1, 0.6f);
      c.set(3, 1, 0.6f);
      for (int k = 0; k < 4; k++) c.set(k, 2, 0.6f);
    }
    c.show();
  }
};

CatMode cat;

}  // namespace

Creature *const catCreature = &cat;
