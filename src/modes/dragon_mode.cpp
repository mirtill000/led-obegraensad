// "Draghetto": a dragon hatching from an egg, a cub for its first day, a
// youngster until its third, then grown - into the dragon you raised it to
// be. Three mini-games train it: Riflessi (an arrow flashes, press it in
// time) for agility, Memoria (repeat the arrows it shows, longer and
// longer) for the mind, Forza (press as fast as you can) for strength.
// Grown, it takes the shape of what it is best at - a fire dragon with
// spikes, a sky dragon with great wings, a wise dragon with long horns -,
// or stays a round, lazy dragon if it was hardly trained. Training fades
// slowly, so a grown dragon can still change. It eats, gets bored, sleeps
// at night (22-7); it never dies.
//   L pappa, R riflessi, U memoria, D forza, A coccole
#include <math.h>

#include "modes/creature.h"
#include "sprite_atlas.h"
#include "timekeeping.h"

namespace {

enum Form : uint8_t { NO_FORM, FIRE, SKY, WISE, LAZY };

struct DragonLife {
  CreatureClock clock;
  float food = 80, joy = 80;
  float strength = 0, agility = 0, mind = 0;  // 0-100, from the mini-games
  uint32_t ageMin = 0;
  uint8_t form = NO_FORM;  // once grown
  char name[16] = "Brace";
};

const uint32_t HATCH_MIN = 20, YOUNG_MIN = 24 * 60, GROWN_MIN = 3 * 24 * 60;
enum Stage : uint8_t { EGG, BABY, YOUNG, ADULT };

enum Game : uint8_t { NO_GAME, REFLEX, MEMORY, MIGHT };
enum Anim : uint8_t { NONE, FEED, LOVE, REFUSE, CHEER };
const uint32_t ANIM_MS[] = {0, 2200, 1800, 800, 2000};

const char ARROWS[] = "LRUD";

// An arrow (7x7) pointing `dir` ('L', 'R', 'U', 'D'), centred at (cx, cy).
void arrow(Canvas &c, int cx, int cy, char dir, float v) {
  static const char *const UP[] = {"...#...", "..###..", ".#####.", "#######", "..###..", "..###..", "..###.."};
  for (int r = 0; r < 7; r++) {
    for (int k = 0; k < 7; k++) {
      if (UP[r][k] != '#') continue;
      int dx = k - 3, dy = r - 3;
      switch (dir) {
        case 'D': dy = -dy; break;
        case 'L': { const int t = dx; dx = dy; dy = t; break; }
        case 'R': { const int t = dx; dx = -dy; dy = -t; break; }
      }
      c.set(cx + dx, cy + dy, v);
    }
  }
}

float eyeMark(char, void *ctx) { return *(float *)ctx; }

class DragonMode : public Creature {
 public:
  const char *id() const override { return "dragon"; }
  const char *name() const override { return "Draghetto"; }
  const char *actionName() const override { return "Dai la pappa"; }
  void action() override { input('L'); }
  const GameControls *keys() const override {
    static const GameControls c = {"LRUDA", {"Pappa", "Riflessi", "Memoria", "Forza", "Coccole"}, false,
                                   "Tastiera: ← pappa, → riflessi, ↑ memoria, ↓ forza, spazio coccole. Nei giochi: frecce."};
    return &c;
  }
  const char *resetName() const override { return "Nuovo uovo"; }
  const char *resetQuestion() const override { return "Il draghetto lascerà il posto a un nuovo uovo. Continuare?"; }
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
    game_ = NO_GAME;
    anim_ = NONE;
    x_ = target_ = 3;
    display.beginTransition();
  }

  void update(uint32_t now) override {
    if (now - lastDraw_ < 40) return;
    lastDraw_ = now;
    if (stage() != shownStage_) {
      if (shownStage_ != 255) display.beginPageTransition(900);  // it hatches, it grows
      shownStage_ = stage();
    }
    if (game_ != NO_GAME) {
      play(now);
      return;
    }
    wander(now);
    draw(now);
  }

  Info info() override {
    load();
    Info i;
    static const char *const STAGES[] = {"uovo", "cucciolo", "giovane", "adulto"};
    static const char *const FORMS[] = {"", "drago di fuoco", "drago del cielo", "drago saggio", "drago pigro"};
    const uint32_t days = life_.ageMin / (24 * 60);
    i.title = String(life_.name) + " · " + (stage() == ADULT ? FORMS[form()] : STAGES[stage()]);
    if (days) i.title += String(", ") + (days == 1 ? "1 giorno" : String(days) + " giorni");
    i.add("Pancia", life_.food, 25);
    i.add("Allegria", life_.joy, 25);
    i.add("Forza", life_.strength, 0);
    i.add("Agilità", life_.agility, 0);
    i.add("Mente", life_.mind, 0);
    if (stage() == EGG) i.mood = "Sta per nascere";
    else if (asleep()) i.mood = "Dorme";
    else if (life_.food < 25) i.mood = "Ha fame";
    else if (life_.joy < 25) i.mood = "Si annoia: gioca con lui";
    else if (stage() != ADULT) {
      const Form f = leaning();
      i.mood = f == FIRE ? "Diventerà forte come il fuoco" : f == SKY ? "Diventerà agile come il vento"
               : f == WISE ? "Diventerà saggio" : "Allenalo, o crescerà pigro";
    } else if (life_.joy > 70) i.mood = "È felice";
    else i.mood = "Sta bene";
    return i;
  }

 protected:
  uint8_t *state() override { return (uint8_t *)&life_; }
  size_t stateSize() const override { return sizeof(life_); }
  uint8_t version() const override { return 1; }
  void fresh() override { life_ = DragonLife(); }

  void liveMinute(const struct tm *t) override {
    life_.ageMin++;
    if (stage() == EGG) return;
    const bool night = t && (t->tm_hour >= 22 || t->tm_hour < 7);
    life_.food = max(0.0f, life_.food - (night ? 2.0f : 7.0f) / 60);
    life_.joy = max(0.0f, life_.joy - (night ? 0.5f : 4.0f) / 60 - (life_.food < 20 ? 2.0f / 60 : 0));
    // Training fades: a point a day.
    const float fade = 1.0f / (24 * 60);
    life_.strength = max(0.0f, life_.strength - fade);
    life_.agility = max(0.0f, life_.agility - fade);
    life_.mind = max(0.0f, life_.mind - fade);
    if (stage() == ADULT) {
      // Its shape follows what it is best at (with a margin, so it doesn't
      // flicker between two).
      const Form best = leaning();
      if (life_.form == NO_FORM || (best != life_.form && score(best) > score((Form)life_.form) + 10)) life_.form = best;
    }
  }

  bool care(char key) override {
    const uint32_t now = millis();
    if (game_ != NO_GAME) {
      gameKey(key, now);
      return false;  // saved when the game ends
    }
    if (anim_ != NONE && now - animStart_ < ANIM_MS[anim_]) return false;
    Anim next = REFUSE;
    if (stage() != EGG && !asleep()) {
      switch (key) {
        case 'L':
          if (life_.food <= 90) {
            life_.food = min(100.0f, life_.food + 30);
            next = FEED;
          }
          break;
        case 'A':
          life_.joy = min(100.0f, life_.joy + 10);
          if (stage() == ADULT && form() == FIRE) x_ = target_ = 0;  // room for its flame
          next = LOVE;
          break;
        case 'R':
        case 'U':
        case 'D':
          if (life_.food >= 15) {
            begin(key == 'R' ? REFLEX : key == 'U' ? MEMORY : MIGHT, now);
            return false;
          }
          break;
      }
    }
    anim_ = next;
    animStart_ = now;
    return next != REFUSE;
  }

 private:
  DragonLife life_;
  Anim anim_ = NONE;
  uint32_t animStart_ = 0, lastDraw_ = 0, lastStep_ = 0;
  uint8_t shownStage_ = 255;
  int x_ = 3, target_ = 3;

  // The mini-game being played.
  Game game_ = NO_GAME;
  uint32_t gameStart_ = 0, phaseStart_ = 0;
  uint8_t round_ = 0, hits_ = 0, misses_ = 0, presses_ = 0;
  char cue_ = 0;            // Riflessi: the arrow to press; 0 = the pause between
  char seq_[8] = {};        // Memoria: the arrows to repeat
  uint8_t len_ = 0, typed_ = 0;
  bool showing_ = false;    // Memoria: showing the sequence (not your turn)
  char last_ = 0;           // the key you just pressed, echoed
  uint32_t lastAt_ = 0;
  bool over_ = false;       // showing the result
  float gain_ = 0;

  Stage stage() const {
    return life_.ageMin < HATCH_MIN ? EGG : life_.ageMin < YOUNG_MIN ? BABY : life_.ageMin < GROWN_MIN ? YOUNG : ADULT;
  }
  bool asleep() const {
    struct tm t;
    return stage() != EGG && localTime(t) && (t.tm_hour >= 22 || t.tm_hour < 7);
  }
  float score(Form f) const { return f == FIRE ? life_.strength : f == SKY ? life_.agility : f == WISE ? life_.mind : 25; }
  Form leaning() const {
    Form best = LAZY;
    for (Form f : {FIRE, SKY, WISE}) {
      if (score(f) >= 25 && score(f) > score(best)) best = f;
    }
    return best;
  }
  Form form() const { return life_.form != NO_FORM ? (Form)life_.form : leaning(); }
  const Sprite &sprite() const {
    switch (stage()) {
      case EGG: return spr::DRAGON_EGG;
      case BABY: return spr::DRAGON_BABY;
      case YOUNG: return spr::DRAGON_YOUNG;
      default: break;
    }
    switch (form()) {
      case FIRE: return spr::DRAGON_FIRE;
      case SKY: return spr::DRAGON_SKY;
      case WISE: return spr::DRAGON_WISE;
      default: return spr::DRAGON_LAZY;
    }
  }

  // ---- the mini-games ----
  void begin(Game g, uint32_t now) {
    game_ = g;
    gameStart_ = phaseStart_ = now;
    round_ = hits_ = misses_ = presses_ = typed_ = 0;
    cue_ = last_ = 0;
    over_ = false;
    if (g == MEMORY) {
      len_ = 3;
      for (char &k : seq_) k = ARROWS[esp_random() % 4];
      showing_ = true;
    }
    display.beginTransition();
  }

  void finish(uint32_t now) {
    switch (game_) {
      case REFLEX: gain_ = hits_ * 2.5f; life_.agility = min(100.0f, life_.agility + gain_); break;
      case MEMORY: gain_ = (len_ - 3) * 4.0f + typed_; life_.mind = min(100.0f, life_.mind + gain_); break;
      case MIGHT: gain_ = min(15.0f, presses_ * 0.6f); life_.strength = min(100.0f, life_.strength + gain_); break;
      default: break;
    }
    life_.joy = min(100.0f, life_.joy + 6);
    life_.food = max(0.0f, life_.food - 5);
    save();
    over_ = true;
    phaseStart_ = now;
  }

  void gameKey(char key, uint32_t now) {
    if (over_) return;
    last_ = key;
    lastAt_ = now;
    switch (game_) {
      case REFLEX:
        if (!cue_) return;
        if (key == cue_) hits_++;
        else misses_++;
        cue_ = 0;
        phaseStart_ = now;
        break;
      case MEMORY:
        if (showing_ || !strchr(ARROWS, key)) return;
        if (key != seq_[typed_]) {
          finish(now);  // wrong: the game ends here
          return;
        }
        if (++typed_ == len_) {
          if (len_ == sizeof(seq_)) {
            finish(now);
            return;
          }
          len_++;
          typed_ = 0;
          showing_ = true;
          phaseStart_ = now + 500;
        }
        break;
      case MIGHT: presses_++; break;
      default: break;
    }
  }

  void play(uint32_t now) {
    Canvas c;
    c.fill(0);
    if (over_) {
      // The result: the dragon cheering, the points gained as dots.
      if (now - phaseStart_ > 2500) {
        game_ = NO_GAME;
        anim_ = CHEER;
        animStart_ = now;
        return;
      }
      const Sprite &s = sprite();
      const float eye = 0;
      c.sprite(s, (COLS - s.w) / 2, ROWS - 1 - s.h - ((now / 200) % 2), 0, 0.75f, false, eyeMark, (void *)&eye);
      for (int k = 0; k < (int)gain_ && k < 16; k++) c.set(k, 0, 0.9f);
      c.show();
      return;
    }
    const uint32_t t = now - phaseStart_;
    switch (game_) {
      case REFLEX: {
        // Six arrows, each shown a little shorter; a pause between.
        const uint32_t window = 1500 - round_ * 150;
        if (!cue_ && t > 500) {
          if (round_ == 6) {
            finish(now);
            return;
          }
          cue_ = ARROWS[esp_random() % 4];
          round_++;
          phaseStart_ = now;
        } else if (cue_ && t > window) {
          misses_++;  // too slow
          cue_ = 0;
          phaseStart_ = now;
        }
        if (cue_) arrow(c, 8, 8, cue_, 1);
        // The score along the top: hits bright, misses dim.
        for (int k = 0; k < hits_; k++) c.set(k * 2, 0, 0.9f);
        for (int k = 0; k < misses_; k++) c.set(COLS - 1 - k * 2, 0, 0.25f);
        // The time left, shrinking along the bottom.
        if (cue_) {
          const int left = COLS - (int)(t * COLS / window);
          for (int k = 0; k < left; k++) c.set(k, ROWS - 1, 0.3f);
        }
        break;
      }
      case MEMORY: {
        // The sequence, one arrow at a time; then your turn ("?" dots blink).
        if (showing_) {
          if (now < phaseStart_) break;
          const int i = t / 800;
          if (i >= len_) {
            showing_ = false;
            phaseStart_ = now;
          } else if (t % 800 < 600) {
            arrow(c, 8, 8, seq_[i], 0.9f);
          }
          for (int k = 0; k < len_; k++) c.set(4 + k, 0, k <= i ? 0.6f : 0.2f);
        } else {
          if (now - lastAt_ < 300 && last_) arrow(c, 8, 8, last_, 0.6f);
          else if ((now / 400) % 2) c.set(8, 8, 0.5f);
          for (int k = 0; k < len_; k++) c.set(4 + k, 0, k < typed_ ? 0.9f : 0.2f);
          if (t > 8000) finish(now);  // gave up
        }
        break;
      }
      case MIGHT: {
        // Press, press, press: the flame grows with every press, 4 seconds.
        if (t > 4000) {
          finish(now);
          return;
        }
        const Sprite &s = sprite();
        const float eye = 0;
        c.sprite(s, 0, ROWS - 1 - s.h, 0, 0.6f, false, eyeMark, (void *)&eye);
        const float len = min(15.0f, presses_ * 0.6f);
        for (int k = 0; k < (int)len; k++) {
          const int fx = s.w + k, fy = ROWS - 1 - s.h + 2;
          c.set(fx, fy + ((k + now / 80) % 3 == 0 ? 1 : 0), 0.4f + 0.6f * (1 - k / 16.0f));
          if (k > 2) c.set(fx, fy + 1, 0.3f);
        }
        const int left = COLS - (int)(t * COLS / 4000);
        for (int k = 0; k < left; k++) c.set(k, 0, 0.3f);
        break;
      }
      default: break;
    }
    c.show();
  }

  // ---- everyday life ----
  void wander(uint32_t now) {
    if (now - lastStep_ < 450) return;
    lastStep_ = now;
    if (anim_ != NONE || asleep() || stage() == EGG) return;
    const int maxX = COLS - sprite().w;
    if (x_ == target_) {
      if (esp_random() % 4 == 0) target_ = esp_random() % (maxX + 1);
      return;
    }
    x_ += target_ > x_ ? 1 : -1;
  }

  void draw(uint32_t now) {
    const uint32_t at = anim_ != NONE ? now - animStart_ : 0;
    if (anim_ != NONE && at >= ANIM_MS[anim_]) anim_ = NONE;
    Canvas c;
    c.fill(0);
    for (int x = 0; x < COLS; x++) c.set(x, ROWS - 1, 0.06f);
    const Sprite &s = sprite();
    x_ = min(x_, COLS - (int)s.w);
    int x = x_, y = ROWS - 1 - s.h;
    if (stage() == EGG) {
      const uint32_t period = life_.ageMin + 2 >= HATCH_MIN ? 150 : 900;
      x = (COLS - s.w) / 2 + ((now / period) % 4 == 1 ? 1 : (now / period) % 4 == 3 ? -1 : 0);
      c.sprite(s, x, y, 0, 0.7f);
      c.show();
      return;
    }
    const bool sleeping = asleep();
    // The sky dragon hovers; the others hop when happy or cheering.
    if (stage() == ADULT && form() == SKY && !sleeping) y -= 1 + (int)(now / 300 % 2);
    else if ((anim_ == CHEER && (at / 200) % 2) || (anim_ == NONE && !sleeping && life_.joy > 70 && (now / 700) % 6 == 0)) y -= 1;
    if (anim_ == REFUSE) x += (at / 100) % 2 ? 1 : -1;
    float eye = sleeping || (now % 4000) < 150 || anim_ == LOVE ? 0.6f : 0;
    c.sprite(s, x, y, 0, sleeping ? 0.3f : 0.65f, false, eyeMark, &eye);
    // Its mouth: just right of the eye.
    int mx = x + s.w, my = y + 2;
    for (int r = 0; r < s.h; r++) {
      for (int k = 0; k < s.w; k++) {
        if (sprites::at(s, 0, k, r) == 'e') mx = x + s.w, my = y + r + 1;
      }
    }

    switch (anim_) {
      case FEED: {
        // A piece of meat drops in front of it and is gobbled up.
        const int fx = min(mx + 1, COLS - 3);
        const int fy = at < 800 ? -3 + (int)((ROWS - 4) * at / 800) : ROWS - 4;
        if (at < 1800) {
          c.set(fx, fy, 0.8f);
          c.set(fx + 1, fy, 0.8f);
          c.set(fx, fy + 1, 0.8f);
          c.set(fx + 1, fy + 1, 0.8f);
          c.set(fx + 2, fy + 2, 0.5f);  // the bone
        }
        break;
      }
      case LOVE:
      case CHEER: {
        const float ph = at / (float)ANIM_MS[anim_];
        if (stage() == ADULT && form() == FIRE) {
          // A puff of flame, happily.
          for (int k = 0; k < 5; k++) c.put(mx + k, my + sinf(ph * 20 + k) * 0.5f, (1 - k / 5.0f) * (1 - ph));
        } else {
          c.put(x + s.w / 2.0f, y - 2 - ph * 6, 0.8f * (1 - ph));
          c.put(x + s.w / 2.0f + 1, y - 2 - ph * 6, 0.8f * (1 - ph));
        }
        break;
      }
      default: break;
    }
    if (sleeping) {
      const int ph = (now / 600) % 4;
      c.set(min(x + s.w, COLS - 2), max(0, y - 2 - ph), 0.35f);
      c.set(min(x + s.w, COLS - 2) + 1, max(0, y - 2 - ph), 0.35f);
    } else if (anim_ == NONE && (now / 1000) % 2) {
      // What it needs, blinking in the corner.
      if (life_.food < 25) {
        c.set(1, 0, 0.6f);
        c.set(0, 1, 0.6f);
        c.set(1, 1, 0.6f);
        c.set(2, 2, 0.4f);
      } else if (life_.joy < 25) {
        c.set(1, 0, 0.6f);
        c.set(1, 1, 0.6f);
        c.set(1, 3, 0.6f);
      }
    }
    c.show();
  }
};

DragonMode dragon;

}  // namespace

Creature *const dragonCreature = &dragon;
