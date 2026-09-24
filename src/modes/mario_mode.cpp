#include "modes/mario_mode.h"

#include "display.h"
#include "settings.h"

static const uint32_t FRAME_MS = 75;
static const uint32_t JUMP_BUFFER_MS = 200;  // jump pressed this long before landing still works

static const int GROUND = 14;   // top row of the ground (rows 14-15)
static const int NONE = ROWS;   // column with no ground: a pit
static const int MARIO_X = 2;   // Mario's screen column (left edge)
static const int MARIO_W = 5, MARIO_H = 7;  // sprite size
static const float GRAVITY = 0.3f;
static const float JUMP_SPEED = -2.3f;  // ~9 px high, ~15 px long
static const float MAX_FALL = 3.0f;
static const float GOOMBA_SPEED = 0.25f;

// Sprites: bit 15 = leftmost column.
// Mario, 5x7, facing right, as brightness levels: cap and brim brightest,
// face mid, hair/eye/moustache faint, overalls bright, shoes dim.
static const char *const MARIO_RUN[2][MARIO_H] = {
    {".CCC.", "CCCCC", "MSSMS", "SSMMM", ".OOO.", "OO.OO", "B...B"},
    {".CCC.", "CCCCC", "MSSMS", "SSMMM", ".OOO.", ".OOO.", ".BB.."},
};
static const char *const MARIO_JUMP[MARIO_H] = {"SCCC.", "CCCCC", "MSSMS", "SSMMM", "OOOOS", "OO.OO", "B..B."};

static uint8_t spriteLevel(char c) {
  switch (c) {
    case 'C': return 255;  // cap
    case 'O': return 200;  // overalls
    case 'S': return 120;  // skin
    case 'B': return 90;   // shoes
    case 'M': return 35;   // hair, eye, moustache
    default: return 0;
  }
}

static void drawMario(int x, int y, const char *const *rows) {
  for (int r = 0; r < MARIO_H; r++) {
    for (int c = 0; c < MARIO_W; c++) {
      const uint8_t l = spriteLevel(rows[r][c]);
      if (l) display.setLevel(x + c, y + r, l);
    }
  }
}

static const uint16_t GOOMBA[2][2] = {
    {0xE000, 0x8000},
    {0xE000, 0x2000},
};

static int32_t randomInt(int32_t lo, int32_t hi) { return lo + (int32_t)(esp_random() % (uint32_t)(hi - lo + 1)); }

static uint8_t topAt(const MarioMode::State &s, int32_t x) {
  if (x < 0 || x >= s.generatedTo) return GROUND;
  return s.top[x % MarioMode::RING];
}

// Advances the world by one frame; returns false if Mario died.
static bool step(MarioMode::State &s) {
  s.cam++;
  const int32_t px = s.cam + MARIO_X;

  for (MarioMode::Goomba &g : s.goombas) {
    if (!g.alive) continue;
    // Walk, turning round at pits and pipes.
    const float next = g.x + g.dir * GOOMBA_SPEED;
    const int32_t left = (int32_t)floorf(next);
    if (topAt(s, left) != GROUND || topAt(s, left + 2) != GROUND) {
      g.dir = -g.dir;
    } else {
      g.x = next;
    }
  }

  // Vertical movement, landing on the highest surface under Mario.
  const float prevBottom = s.y + MARIO_H;
  const int prevBottomRow = (int)floorf(s.y + 0.01f) + MARIO_H - 1;
  s.vy = fminf(s.vy + GRAVITY, MAX_FALL);
  s.y += s.vy;
  s.onGround = false;
  if (s.vy >= 0) {
    int landing = NONE;
    for (int c = 0; c < MARIO_W; c++) {
      const int t = topAt(s, px + c);
      if (t < NONE && s.y + MARIO_H >= t && prevBottom <= t + 0.01f && t < landing) landing = t;
    }
    if (landing < NONE) {
      s.y = landing - MARIO_H;
      s.vy = 0;
      s.onGround = true;
    }
  }
  if (s.y > ROWS) return false;  // fell into a pit

  // Running into the side of a pipe.
  const int bottomRow = (int)floorf(s.y + 0.01f) + MARIO_H - 1;
  for (int c = 0; c < MARIO_W; c++) {
    if (bottomRow >= topAt(s, px + c)) return false;
  }

  // Goombas: stomp them from above, anything else hurts.
  for (MarioMode::Goomba &g : s.goombas) {
    if (!g.alive) continue;
    const int32_t gx = (int32_t)lroundf(g.x);
    if (px > gx + 2 || gx > px + MARIO_W - 1 || bottomRow < GROUND - 2) continue;
    if (s.vy > 0 && prevBottomRow < GROUND - 2) {  // feet were above it
      g.alive = false;
      s.vy = -1.8f;
      s.onGround = false;
      s.score++;
    } else {
      return false;
    }
  }

  const int topRow = (int)floorf(s.y);
  for (MarioMode::Coin &c : s.coins) {
    if (c.taken) continue;
    if (c.x >= px && c.x <= px + MARIO_W - 1 && c.y + 1 >= topRow && c.y <= bottomRow) {
      c.taken = true;
      s.score++;
    }
  }
  return true;
}

// --- Autopilot -------------------------------------------------------------
// It plans by simulating copies of the state: for each moment it could jump
// (now or in a few frames), how long would Mario survive, allowing up to two
// more jumps later? It jumps now only if that is the best choice.

static const int PLAN_FRAMES = 60;  // "survives this long" counts as safe
static const int JUMP_WINDOW = 12;  // moments considered for the next jump

static int bestFrom(const MarioMode::State &s, int jumps, int budget);

// Frames survived (up to `budget`) just walking on.
static int walkFrames(MarioMode::State s, int budget) {
  for (int i = 0; i < budget; i++) {
    if (!step(s)) return i;
  }
  return budget;
}

// Frames survived (up to `budget`) jumping now, then playing on as well as
// possible with `jumps - 1` more jumps.
static int jumpFrames(MarioMode::State s, int jumps, int budget) {
  s.vy = JUMP_SPEED;
  s.onGround = false;
  for (int i = 0; i < budget; i++) {
    if (!step(s)) return i;
    if (s.onGround) return i + 1 + bestFrom(s, jumps - 1, budget - i - 1);
  }
  return budget;
}

// Frames survived (up to `budget`) with the best choice of when to jump,
// using at most `jumps` jumps.
static int bestFrom(const MarioMode::State &start, int jumps, int budget) {
  int best = walkFrames(start, budget);
  if (jumps == 0 || best == budget) return best;
  MarioMode::State s = start;
  for (int wait = 0; wait < JUMP_WINDOW && wait < budget; wait++) {
    if (s.onGround) {
      best = max(best, wait + jumpFrames(s, jumps, budget - wait));
      if (best == budget) return best;
    }
    if (!step(s)) break;
  }
  return best;
}

void MarioMode::start() {
  memset(&s_, 0, sizeof(s_));
  s_.y = GROUND - MARIO_H;
  s_.onGround = true;
  flatLeft_ = 20;
  extendWorld();
  phase_ = PLAYING;
  lastFrame_ = 0;
}

// Generates level columns up to the end of the ring buffer: flat stretches
// separated by a pipe, a pit, a goomba or a row of coins.
void MarioMode::extendWorld() {
  auto put = [this](uint8_t top) { s_.top[s_.generatedTo++ % RING] = top; };

  for (Goomba &g : s_.goombas) {
    if (g.alive && g.x < s_.cam - 4) g.alive = false;
  }
  for (Coin &c : s_.coins) {
    if (!c.taken && c.x < s_.cam) c.taken = true;
  }

  while (s_.generatedTo < s_.cam + RING - 4) {
    if (flatLeft_ > 0) {
      put(GROUND);
      flatLeft_--;
      continue;
    }
    const int r = randomInt(0, 99);
    if (r < 30) {
      const uint8_t top = GROUND - randomInt(2, 4);
      put(top);
      put(top);
      flatLeft_ = randomInt(8, 13);
    } else if (r < 55) {
      for (int i = randomInt(2, 3); i > 0; i--) put(NONE);
      flatLeft_ = randomInt(8, 13);
    } else if (r < 75) {
      for (Goomba &g : s_.goombas) {
        if (!g.alive) {
          g = {(float)(s_.generatedTo + 4), -1, true};
          break;
        }
      }
      flatLeft_ = randomInt(9, 13);
    } else {
      const int8_t heights[3] = {3, 2, 3};  // above Mario's head: jump for them
      int placed = 0;
      for (Coin &c : s_.coins) {
        if (c.taken && placed < 3) {
          c = {s_.generatedTo + 2 + placed, heights[placed], false};
          placed++;
        }
      }
      flatLeft_ = randomInt(6, 10);
    }
  }
}

// Autopilot decision for this frame: jump when an obstacle is coming and
// jumping now is (one of) the best moments, or to grab a coin just ahead
// when that is safe.
bool MarioMode::shouldJump() const {
  if (!s_.onGround) return false;

  const int32_t px = s_.cam + MARIO_X;
  bool coinAhead = false;
  for (const Coin &c : s_.coins) {
    if (!c.taken && c.x > px + MARIO_W - 1 && c.x <= px + MARIO_W + 3) coinAhead = true;
  }
  const bool obstacle = walkFrames(s_, 14) < 14;
  if (!obstacle && !coinAhead) return false;

  const int now = jumpFrames(s_, 3, PLAN_FRAMES);
  if (now == PLAN_FRAMES) return true;  // jumping now is safe
  if (!obstacle) return false;          // coin not worth a risk

  State later = s_;
  if (!step(later)) return true;
  return now > 1 + bestFrom(later, 3, PLAN_FRAMES - 1);
}

bool MarioMode::input(char key) {
  if (demoMode("mario")) return false;  // the autopilot is playing
  if (key != 'U' && key != 'A') return true;
  if (phase_ != PLAYING) return true;
  if (s_.onGround) {
    s_.vy = JUMP_SPEED;
    s_.onGround = false;
  } else {
    jumpQueuedUntil_ = millis() + JUMP_BUFFER_MS;
  }
  return true;
}

void MarioMode::update(uint32_t now) {
  if (phase_ == SCORE) {
    if (score_.update(now, SCROLL_DELAY_MS)) start();
    return;
  }
  if (now - lastFrame_ < interval(FRAME_MS)) return;
  lastFrame_ = now;
  frame_++;

  if (phase_ == DYING) {
    // Classic death hop: up a little, then down off the screen.
    deathVy_ += GRAVITY;
    deathY_ += deathVy_;
    draw(now);
    if (deathY_ > ROWS + 10) {
      phase_ = SCORE;
      score_.start(String("punti ") + s_.score);
    }
    return;
  }

  const bool jump = demoMode("mario") ? shouldJump() : (s_.onGround && (int32_t)(jumpQueuedUntil_ - now) > 0);
  if (jump) {
    s_.vy = JUMP_SPEED;
    s_.onGround = false;
    jumpQueuedUntil_ = now;
  }
  if (!step(s_)) {
    phase_ = DYING;
    deathY_ = s_.y;
    deathVy_ = -2.5f;
  }
  extendWorld();
  draw(now);
}

void MarioMode::draw(uint32_t now) {
  display.clear();
  // Faint clouds in the background, drifting at half speed.
  static const uint16_t CLOUD[2] = {0x6000, 0xF000};
  for (int i = 0; i < 2; i++) {
    const int period = 24;
    const int x = ((i * 13 - s_.cam / 2) % period + period) % period - 4;
    for (int row = 0; row < 2; row++) {
      for (int col = 0; col < 4; col++) {
        if (CLOUD[row] & (0x8000 >> col)) display.setLevel(x + col, 2 + i * 3 + row, 35);
      }
    }
  }
  for (int sx = 0; sx < COLS; sx++) {
    const int32_t wx = s_.cam + sx;
    const int top = topAt(s_, wx);
    if (top == NONE) continue;
    display.setPixel(sx, GROUND, true);
    display.setLevel(sx, GROUND + 1, wx % 4 != 3 ? 110 : 0);  // bricks show the scrolling
    for (int y = top; y < GROUND; y++) display.setPixel(sx, y, true);
  }
  for (const Coin &c : s_.coins) {
    if (c.taken) continue;
    const int sx = c.x - s_.cam;
    // Spinning glint: the two halves swap brightness.
    const bool phase = (now / 200) % 2;
    display.setLevel(sx, c.y, phase ? 255 : 120);
    display.setLevel(sx, c.y + 1, phase ? 120 : 255);
  }
  for (const Goomba &g : s_.goombas) {
    if (g.alive) display.drawBitmap((int)lroundf(g.x) - s_.cam, GROUND - 2, GOOMBA[(frame_ / 3) % 2], 3, 2);
  }

  if (phase_ == DYING) {
    drawMario(MARIO_X, (int)lroundf(deathY_), MARIO_JUMP);
  } else {
    drawMario(MARIO_X, (int)lroundf(s_.y), s_.onGround ? MARIO_RUN[(frame_ / 2) % 2] : MARIO_JUMP);
  }
  display.render();
}
