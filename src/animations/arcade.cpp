// Arcade games: Pong, Breakout, Flappy Bird, Space Invaders and Pac-Man. In
// demo mode each is played by a simple computer player; otherwise it takes
// the page's controls. At game over they scroll the score and start again.
#include <math.h>
#include <string.h>

#include "animation.h"
#include "display.h"
#include "scroller.h"

static float random01() { return (esp_random() & 0xFFFF) / 65535.0f; }

// All the games in this file draw LEDs only fully on or off: in-between
// levels are made by switching the LEDs rapidly, which can show as flicker.

// A dot at a fractional position, on the nearest pixel.
static void dot(float x, float y) { display.setPixel((int)lroundf(x), (int)lroundf(y), true); }

// "Punti 12" scrolling once after a game over.
class ScoreScreen {
 public:
  void show(const String &text) {
    scroller_.start(text);
    active_ = true;
  }
  bool active() const { return active_; }
  // True when it has finished (the game should restart).
  bool update(uint32_t now) {
    if (!active_ || !scroller_.update(now, 70)) return false;
    active_ = false;
    return true;
  }

 private:
  Scroller scroller_;
  bool active_ = false;
};

// Common plumbing for the games in this file.
class ArcadeGame : public Animation {
 public:
  const char *group() const override { return "Giochi"; }
  bool isGame() const override { return true; }
  void setDemo(bool demo) override { demo_ = demo; }

  void frame(uint32_t now) override {
    if (score_.active()) {
      if (score_.update(now)) start();
      return;
    }
    tick(now);
  }

 protected:
  virtual void tick(uint32_t now) = 0;
  void gameOver(int points) { score_.show(String("Punti ") + points); }
  bool demo_ = true;

 private:
  ScoreScreen score_;
};

// ---------------------------------------------------------------------------
// Pong: you (left paddle) against the computer (right), first to 5.
class PongGame : public ArcadeGame {
 public:
  const char *id() const override { return "pong"; }
  const char *name() const override { return "Pong"; }
  uint16_t frameMs() const override { return 30; }

  void start() override {
    left_ = right_ = 6;
    scoreL_ = scoreR_ = 0;
    serve(esp_random() & 1);
  }

  void input(char key) override {
    if (key == 'U') left_ = fmaxf(0, left_ - 1);
    if (key == 'D') left_ = fminf(ROWS - PADDLE, left_ + 1);
  }

 protected:
  void tick(uint32_t) override {
    if (pause_ > 0) {
      pause_--;
      drawScore();
      return;
    }
    // Paddles: the computer on the right; also on the left in demo mode.
    right_ = track(right_, 0.22f, vx_ > 0);
    if (demo_) left_ = track(left_, 0.3f, vx_ < 0);

    x_ += vx_;
    y_ += vy_;
    if (y_ < 0) {
      y_ = -y_;
      vy_ = -vy_;
    }
    if (y_ > ROWS - 1) {
      y_ = 2 * (ROWS - 1) - y_;
      vy_ = -vy_;
    }
    if (x_ <= 1 && vx_ < 0) bounce(left_, 1);
    if (x_ >= COLS - 2 && vx_ > 0) bounce(right_, COLS - 2);
    if (x_ < -1 || x_ > COLS) {
      (x_ < 0 ? scoreR_ : scoreL_)++;
      if (scoreL_ == 5 || scoreR_ == 5) {
        gameOver(scoreL_);
        return;
      }
      serve(x_ < 0);
      pause_ = 25;  // show the score for a moment
      return;
    }
    draw();
  }

 private:
  static const int PADDLE = 4;

  void serve(bool towardsLeft) {
    x_ = 7.5f;
    y_ = 3 + random01() * 9;
    vx_ = towardsLeft ? -0.3f : 0.3f;
    vy_ = (random01() - 0.5f) * 0.4f;
  }

  // Moves a paddle towards where the ball is heading, with limited speed
  // (so the computer can be beaten).
  float track(float paddle, float speed, bool incoming) const {
    const float target = (incoming ? y_ : 7.5f) - PADDLE / 2.0f + 0.5f;
    const float step = fmaxf(-speed, fminf(speed, target - paddle));
    return fmaxf(0, fminf(ROWS - PADDLE, paddle + step));
  }

  void bounce(float paddle, float edgeX) {
    if (y_ < paddle - 0.5f || y_ > paddle + PADDLE - 0.5f) return;  // missed
    x_ = edgeX;
    vx_ = -vx_ * 1.05f;  // a little faster each hit
    if (fabsf(vx_) > 0.7f) vx_ = vx_ > 0 ? 0.7f : -0.7f;
    vy_ += (y_ - (paddle + PADDLE / 2.0f - 0.5f)) * 0.12f;  // edges deflect
  }

  void draw() {
    display.clear();
    for (int i = 0; i < PADDLE; i++) {
      display.setPixel(0, (int)roundf(left_) + i, true);
      display.setPixel(COLS - 1, (int)roundf(right_) + i, true);
    }
    dot(x_, y_);
  }

  void drawScore() {
    display.clear();
    const String s = String(scoreL_) + "-" + scoreR_;
    const int w = Display::textWidth(s.c_str(), 0, s.length()) - 1;
    display.drawText((COLS - w) / 2, 5, s.c_str(), 0, s.length());
  }

  float left_ = 6, right_ = 6, x_ = 7.5f, y_ = 7.5f, vx_ = 0.3f, vy_ = 0;
  int scoreL_ = 0, scoreR_ = 0, pause_ = 0;
};

// ---------------------------------------------------------------------------
// Breakout: clear the bricks with the ball; 3 lives.
class BreakoutGame : public ArcadeGame {
 public:
  const char *id() const override { return "breakout"; }
  const char *name() const override { return "Breakout"; }
  uint16_t frameMs() const override { return 30; }

  void start() override {
    lives_ = 3;
    points_ = 0;
    speed_ = 0.28f;
    fillBricks();
    newBall();
  }

  void input(char key) override {
    if (key == 'L') paddle_ = fmaxf(0, paddle_ - 1);
    if (key == 'R') paddle_ = fminf(COLS - PADDLE, paddle_ + 1);
  }

 protected:
  void tick(uint32_t) override {
    if (demo_) {  // follow the ball, aiming a little off-centre for angles
      const float target = x_ - PADDLE / 2.0f + 0.5f + aim_;
      paddle_ += fmaxf(-0.45f, fminf(0.45f, target - paddle_));
      paddle_ = fmaxf(0, fminf(COLS - PADDLE, paddle_));
    }
    if (wait_ > 0) {  // ball resting on the paddle before launch
      wait_--;
      x_ = paddle_ + PADDLE / 2.0f - 0.5f;
      y_ = ROWS - 2;
    } else {
      // Small sub-steps so the ball never skips a brick.
      for (int s = 0; s < 3; s++) {
        if (!move(vx_ / 3, vy_ / 3)) return;
      }
    }
    draw();
  }

 private:
  static const int PADDLE = 4, BRICK_ROWS = 4, BRICK_W = 2, TOP = 2;

  void fillBricks() {
    for (int r = 0; r < BRICK_ROWS; r++) {
      for (int c = 0; c < COLS / BRICK_W; c++) bricks_[r][c] = true;
    }
    left_ = BRICK_ROWS * COLS / BRICK_W;
  }

  void newBall() {
    wait_ = 30;
    const float angle = (random01() - 0.5f) * 1.2f;
    vx_ = sinf(angle) * speed_;
    vy_ = -cosf(angle) * speed_;
  }

  // Moves the ball a little; false if the game ended.
  bool move(float dx, float dy) {
    x_ += dx;
    y_ += dy;
    if (x_ < 0) {
      x_ = -x_;
      vx_ = fabsf(vx_);
    }
    if (x_ > COLS - 1) {
      x_ = 2 * (COLS - 1) - x_;
      vx_ = -fabsf(vx_);
    }
    if (y_ < 0) {
      y_ = -y_;
      vy_ = fabsf(vy_);
    }
    const int bx = (int)roundf(x_), by = (int)roundf(y_);
    const int row = by - TOP, col = bx / BRICK_W;
    if (row >= 0 && row < BRICK_ROWS && col >= 0 && col < COLS / BRICK_W && bricks_[row][col]) {
      bricks_[row][col] = false;
      vy_ = -vy_;
      points_ += BRICK_ROWS - row;  // higher rows are worth more
      if (--left_ == 0) {  // level cleared: faster next time
        speed_ = fminf(0.5f, speed_ + 0.05f);
        fillBricks();
        newBall();
        return false;
      }
    }
    if (vy_ > 0 && y_ >= ROWS - 2 && y_ < ROWS - 1 && x_ > paddle_ - 1 && x_ < paddle_ + PADDLE) {
      // Paddle: the angle depends on where it hits.
      const float hit = (x_ - (paddle_ + PADDLE / 2.0f - 0.5f)) / (PADDLE / 2.0f);
      const float angle = fmaxf(-1.1f, fminf(1.1f, hit * 1.0f));
      vx_ = sinf(angle) * speed_;
      vy_ = -cosf(angle) * speed_;
      y_ = ROWS - 2;
      aim_ = (random01() - 0.5f) * 2.8f;  // the computer varies its angles
    }
    if (y_ > ROWS) {
      if (--lives_ == 0) {
        gameOver(points_);
        return false;
      }
      newBall();
      return false;
    }
    return true;
  }

  void draw() {
    display.clear();
    for (int r = 0; r < BRICK_ROWS; r++) {
      for (int c = 0; c < COLS / BRICK_W; c++) {
        if (!bricks_[r][c]) continue;
        for (int k = 0; k < BRICK_W; k++) display.setPixel(c * BRICK_W + k, TOP + r, true);
      }
    }
    for (int i = 0; i < lives_ - 1; i++) display.setPixel(i * 2, 0, true);  // spare balls
    const int p = (int)roundf(paddle_);
    for (int i = 0; i < PADDLE; i++) display.setPixel(p + i, ROWS - 1, true);
    dot(x_, y_);
  }

  bool bricks_[BRICK_ROWS][COLS / BRICK_W];
  int left_ = 0, lives_ = 3, points_ = 0, wait_ = 0;
  float paddle_ = 6, x_ = 7.5f, y_ = 14, vx_ = 0, vy_ = 0, speed_ = 0.28f;
  float aim_ = 0;  // computer player's offset from the ball
};

// ---------------------------------------------------------------------------
// Flappy Bird: fly through the gaps between the pipes.
class FlappyGame : public ArcadeGame {
 public:
  const char *id() const override { return "flappy"; }
  const char *name() const override { return "Flappy Bird"; }
  uint16_t frameMs() const override { return 40; }

  void start() override {
    y_ = 6;
    vy_ = 0;
    points_ = 0;
    scroll_ = 0;
    for (int i = 0; i < PIPES; i++) {
      pipeX_[i] = COLS + 4 + i * SPACING;
      pipeGap_[i] = randomGap();
    }
    started_ = false;
  }

  void input(char key) override {
    if (key == 'A' || key == 'U') flap();
  }

 protected:
  void tick(uint32_t) override {
    if (demo_ && shouldFlap()) flap();
    if (!started_) {  // hover until the first flap
      y_ = 6 + sinf(scroll_ * 0.2f) * 0.6f;
      scroll_ += 0.3f;
      if (demo_) started_ = true;
      draw();
      return;
    }
    vy_ = fminf(vy_ + GRAVITY, 1.0f);
    y_ += vy_;
    for (int i = 0; i < PIPES; i++) {
      pipeX_[i] -= SPEED;
      if (pipeX_[i] < -2) {  // recycle the pipe behind the last one
        float last = pipeX_[0];
        for (int j = 1; j < PIPES; j++) last = fmaxf(last, pipeX_[j]);
        pipeX_[i] = last + SPACING;
        pipeGap_[i] = randomGap();
      }
      // Passed a pipe: a point.
      if (pipeX_[i] + 2 <= BIRD_X && pipeX_[i] + 2 + SPEED > BIRD_X) points_++;
    }
    if (hits(y_, pipeX_)) {
      gameOver(points_);
      return;
    }
    draw();
  }

 private:
  static const int PIPES = 3, SPACING = 8, GAP = 6, BIRD_X = 3;
  static constexpr float GRAVITY = 0.07f, FLAP = -0.5f, SPEED = 0.3f;

  static int randomGap() { return 2 + esp_random() % (ROWS - GAP - 3); }  // top row of the gap

  void flap() {
    vy_ = FLAP;
    started_ = true;
  }

  // True if the bird (2x2 at BIRD_X, y) touches a pipe or leaves the screen,
  // with the pipes at `pipeX`.
  bool hits(float y, const float *pipeX) const {
    if (y < -0.5f || y > ROWS - 2) return true;
    for (int i = 0; i < PIPES; i++) {
      const int px = (int)floorf(pipeX[i]);
      if (px > BIRD_X + 1 || px + 1 < BIRD_X) continue;
      if (y < pipeGap_[i] - 0.3f || y + 1 > pipeGap_[i] + GAP - 0.7f) return true;
    }
    return false;
  }

  // Frames the bird survives (up to `frames`) if it flaps now or not, and
  // then flaps whenever it drops below the middle of the next gap.
  int survival(bool flapNow, int frames) const {
    float y = y_, vy = flapNow ? FLAP : vy_;
    float pipeX[PIPES];
    memcpy(pipeX, pipeX_, sizeof(pipeX));
    for (int f = 0; f < frames; f++) {
      if (f > 0) {
        int next = 0;
        for (int i = 0; i < PIPES; i++) {
          if (pipeX[i] + 2 > BIRD_X && (pipeX[next] + 2 <= BIRD_X || pipeX[i] < pipeX[next])) next = i;
        }
        if (y > pipeGap_[next] + GAP / 2.0f - 0.5f && vy > 0) vy = FLAP;
      }
      vy = fminf(vy + GRAVITY, 1.0f);
      y += vy;
      for (int i = 0; i < PIPES; i++) pipeX[i] -= SPEED;
      if (hits(y, pipeX)) return f;
    }
    return frames;
  }

  // Computer player: flap only if that keeps the bird alive longer.
  bool shouldFlap() const { return survival(true, 40) > survival(false, 40); }

  void draw() {
    display.clear();
    for (int i = 0; i < PIPES; i++) {
      const int px = (int)floorf(pipeX_[i]);
      for (int y = 0; y < ROWS; y++) {
        if (y >= pipeGap_[i] && y < pipeGap_[i] + GAP) continue;
        display.setPixel(px, y, true);
        display.setPixel(px + 1, y, true);
      }
    }
    // The bird: a 2x2 block.
    const int by = (int)lroundf(y_);
    for (int k = 0; k < 4; k++) display.setPixel(BIRD_X + k % 2, by + k / 2, true);
  }

  float y_ = 6, vy_ = 0, scroll_ = 0;
  float pipeX_[PIPES];
  int pipeGap_[PIPES];
  int points_ = 0;
  bool started_ = false;
};

// ---------------------------------------------------------------------------
// Space Invaders: shoot the descending invaders before they land.
class InvadersGame : public ArcadeGame {
 public:
  const char *id() const override { return "invaders"; }
  const char *name() const override { return "Space Invaders"; }
  uint16_t frameMs() const override { return 50; }

  void start() override {
    lives_ = 3;
    points_ = 0;
    wave_ = 0;
    newWave();
  }

  void input(char key) override {
    if (key == 'L') player_ = max(1, player_ - 1);
    if (key == 'R') player_ = min(COLS - 2, player_ + 1);
    if (key == 'A' || key == 'U') shoot();
  }

 protected:
  void tick(uint32_t) override {
    tick_++;
    if (demo_) autopilot();
    // Formation: side to side, a step down at each edge.
    if (tick_ % max(3, 14 - wave_ - (ALIENS - alive_) / 3) == 0) {
      int minX = COLS, maxX = -1, maxY = 0;
      for (int i = 0; i < ALIENS; i++) {
        if (!aliens_[i]) continue;
        minX = min(minX, alienX(i));
        maxX = max(maxX, alienX(i) + 1);
        maxY = max(maxY, alienY(i));
      }
      if ((dir_ > 0 && maxX >= COLS - 1) || (dir_ < 0 && minX <= 0)) {
        dir_ = -dir_;
        oy_++;
        if (maxY + 1 >= ROWS - 3) {  // they landed
          gameOver(points_);
          return;
        }
      } else {
        ox_ += dir_;
      }
    }
    // Player's shot.
    if (shotY_ >= 0 && --shotY_ >= 0) {
      for (int i = 0; i < ALIENS; i++) {
        if (aliens_[i] && alienY(i) == shotY_ && (shotX_ == alienX(i) || shotX_ == alienX(i) + 1)) {
          aliens_[i] = false;
          alive_--;
          points_ += 3 - i / COLS_A;  // top rows are worth more
          shotY_ = -1;
          break;
        }
      }
    }
    if (alive_ == 0) {
      wave_ = min(wave_ + 1, 5);
      newWave();
    }
    // Bombs: dropped by random invaders, falling every other frame.
    if (bombY_ < 0 && esp_random() % 12 == 0) {
      const int i = esp_random() % ALIENS;
      if (aliens_[i]) {
        bombX_ = alienX(i) + (esp_random() & 1);
        bombY_ = alienY(i) + 1;
      }
    }
    if (bombY_ >= 0 && tick_ % 2 == 0 && ++bombY_ >= ROWS) bombY_ = -1;
    if (bombY_ >= ROWS - 2 && abs(bombX_ - player_) <= (bombY_ == ROWS - 2 ? 0 : 1)) {
      bombY_ = -1;
      if (--lives_ == 0) {
        gameOver(points_);
        return;
      }
    }
    draw();
  }

 private:
  static const int ROWS_A = 3, COLS_A = 4, ALIENS = ROWS_A * COLS_A;

  int alienX(int i) const { return ox_ + (i % COLS_A) * 3; }
  int alienY(int i) const { return oy_ + (i / COLS_A) * 2; }

  void newWave() {
    for (bool &a : aliens_) a = true;
    alive_ = ALIENS;
    ox_ = 2;
    oy_ = 1;
    dir_ = 1;
    shotY_ = bombY_ = -1;
    player_ = 7;
  }

  void shoot() {
    if (shotY_ >= 0) return;  // one shot at a time
    shotX_ = player_;
    shotY_ = ROWS - 2;
  }

  // A bomb low enough that standing at `x` is risky.
  bool danger(int x) const { return bombY_ >= ROWS - 9 && abs(bombX_ - x) <= 1; }

  // Computer player: dodge bombs, line up under the lowest invader, fire.
  void autopilot() {
    if (danger(player_)) {
      const int away = (bombX_ <= player_ && player_ < COLS - 2) || player_ <= 1 ? 1 : -1;
      player_ += away;
      return;
    }
    int target = -1;
    for (int i = ALIENS - 1; i >= 0; i--) {
      if (aliens_[i] && (target < 0 || abs(alienX(i) - player_) < abs(alienX(target) - player_))) target = i;
      if (target >= 0 && i % COLS_A == 0) break;  // stay with the lowest row that has invaders
    }
    if (target < 0) return;
    const int aim = alienX(target) + (tick_ / 20) % 2;
    const int step = aim < player_ ? -1 : aim > player_ ? 1 : 0;
    if (step && !danger(player_ + step)) player_ += step;
    if (aim == player_) shoot();
  }

  void draw() {
    display.clear();
    for (int i = 0; i < ALIENS; i++) {
      if (!aliens_[i]) continue;
      display.setPixel(alienX(i), alienY(i), true);
      display.setPixel(alienX(i) + 1, alienY(i), true);
    }
    if (shotY_ >= 0) display.setPixel(shotX_, shotY_, true);
    if (bombY_ >= 0) display.setPixel(bombX_, bombY_, true);
    display.setPixel(player_, ROWS - 2, true);
    for (int dx = -1; dx <= 1; dx++) display.setPixel(player_ + dx, ROWS - 1, true);
    for (int i = 0; i < lives_ - 1; i++) display.setPixel(COLS - 1 - i * 2, 0, true);  // spare lives
  }

  bool aliens_[ALIENS];
  int alive_ = 0, ox_ = 2, oy_ = 1, dir_ = 1, wave_ = 0;
  int player_ = 7, shotX_ = 0, shotY_ = -1, bombX_ = 0, bombY_ = -1;
  int lives_ = 3, points_ = 0;
  uint32_t tick_ = 0;
};

// ---------------------------------------------------------------------------
// Pac-Man: one pixel per cell on a 16x16 maze with a tunnel on row 6. With
// LEDs only on or off the walls stay dark and the dots trace the corridors;
// Pac-Man is steady, the ghosts blink slowly (and only light up briefly
// while they can be eaten). Eat
// every dot to clear the level; 3 lives. In demo mode Pac-Man heads for the
// nearest dot along paths that keep clear of the ghosts, runs from them
// when they get close and hunts them while they are frightened.
class PacManGame : public ArcadeGame {
 public:
  const char *id() const override { return "pacman"; }
  const char *name() const override { return "Pac-Man"; }
  uint16_t frameMs() const override { return 50; }

  void start() override {
    lives_ = 3;
    points_ = 0;
    level_ = 0;
    newLevel();
  }

  void input(char key) override {
    for (int d = 0; d < 4; d++) {
      if (key == "RDLU"[d]) wanted_ = d;
    }
  }

 protected:
  void tick(uint32_t) override {
    tick_++;
    if (flash_ > 0) {  // level cleared: the maze flashes, then a new one
      if (--flash_ == 0) newLevel();
      draw();
      return;
    }
    if (pause_ > 0) {  // after losing a life
      pause_--;
      draw();
      return;
    }

    // Pac-Man: a cell every 3 frames.
    if (tick_ % 3 == 0) {
      if (demo_) wanted_ = autopilot();
      if (canMove(px_, py_, wanted_)) dir_ = wanted_;
      if (canMove(px_, py_, dir_)) step(px_, py_, dir_);
      eat();
    }
    if (frightened_ > 0) frightened_--;

    // Ghosts: a cell every 4 frames (slower while frightened), a bit faster
    // at each level.
    const int every = frightened_ > 0 ? 7 : max(3, 4 - level_ / 2);
    for (int i = 0; i < GHOSTS; i++) {
      Ghost &g = ghosts_[i];
      if (tick_ < g.release) continue;
      if (tick_ % every == (uint32_t)i % every) moveGhost(i);
    }
    collide();
    draw();
  }

 private:
  static const int GHOSTS = 3;
  static const int FRIGHT_FRAMES = 140;  // 7 s
  struct Ghost {
    int x, y, dir;
    uint32_t release;  // frame it may leave the pen
  };

  // '#' wall, '.' dot, 'o' power pellet, ' ' empty (the ghosts' pen).
  static const char *const MAZE[ROWS];

  static int dx(int d) { return d == 0 ? 1 : d == 2 ? -1 : 0; }  // 0 right, 1 down, 2 left, 3 up
  static int dy(int d) { return d == 1 ? 1 : d == 3 ? -1 : 0; }
  static int wrap(int x) { return (x + COLS) % COLS; }  // the tunnel

  bool wall(int x, int y) const { return y < 0 || y >= ROWS || MAZE[y][wrap(x)] == '#'; }
  bool canMove(int x, int y, int d) const { return d >= 0 && !wall(x + dx(d), y + dy(d)); }
  static void step(int &x, int &y, int d) {
    x = wrap(x + dx(d));
    y += dy(d);
  }

  void newLevel() {
    dotsLeft_ = 0;
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        const char c = MAZE[y][x];
        food_[y][x] = c == '.' ? 1 : c == 'o' ? 2 : 0;
        if (food_[y][x]) dotsLeft_++;
      }
    }
    resetPositions();
  }

  void resetPositions() {
    px_ = 7;
    py_ = 13;
    dir_ = wanted_ = 2;
    frightened_ = 0;
    const int startX[GHOSTS] = {7, 8, 7};
    const int startY[GHOSTS] = {7, 7, 8};
    for (int i = 0; i < GHOSTS; i++) ghosts_[i] = {startX[i], startY[i], 3, tick_ + 20 + i * 60};
    pause_ = 20;
  }

  void eat() {
    uint8_t &f = food_[py_][px_];
    if (!f) return;
    if (f == 2) {
      frightened_ = FRIGHT_FRAMES;
      eatenInFright_ = 0;
      for (Ghost &g : ghosts_) g.dir = (g.dir + 2) % 4;  // they turn round
      points_ += 50;
    } else {
      points_ += 10;
    }
    f = 0;
    if (--dotsLeft_ == 0) {
      level_++;
      flash_ = 30;
    }
  }

  // Ghosts never turn back; at a junction they pick the exit closest to
  // their target (Pac-Man, a spot ahead of him, or a random cell for the
  // third), or a random one while frightened.
  void moveGhost(int i) {
    Ghost &g = ghosts_[i];
    int tx = px_, ty = py_;
    if (i == 1) {
      tx = px_ + 3 * dx(dir_);
      ty = py_ + 3 * dy(dir_);
    } else if (i == 2 && (tick_ / 100) % 2) {
      tx = esp_random() % COLS;
      ty = esp_random() % ROWS;
    }
    int best = -1, bestDist = 1 << 30;
    for (int d = 0; d < 4; d++) {
      if (d == (g.dir + 2) % 4 || !canMove(g.x, g.y, d)) continue;
      int nx = g.x, ny = g.y;
      step(nx, ny, d);
      const int dist = frightened_ > 0 ? (int)(esp_random() % 100) : (nx - tx) * (nx - tx) + (ny - ty) * (ny - ty);
      if (dist < bestDist) {
        bestDist = dist;
        best = d;
      }
    }
    if (best < 0) best = (g.dir + 2) % 4;  // dead end: back
    g.dir = best;
    step(g.x, g.y, best);
  }

  void collide() {
    for (int i = 0; i < GHOSTS; i++) {
      Ghost &g = ghosts_[i];
      if (g.x != px_ || g.y != py_) continue;
      if (frightened_ > 0) {
        points_ += 200 << min(eatenInFright_++, 3);
        g = {7 + i % 2, 7, 3, tick_ + 60};  // back to the pen
        continue;
      }
      if (--lives_ <= 0) {
        gameOver(points_);
        return;
      }
      resetPositions();
      return;
    }
  }

  // Distance from every cell to the nearest ghost (breadth-first), for the
  // autopilot.
  void ghostDistances(uint8_t dist[ROWS][COLS]) const {
    memset(dist, 255, ROWS * COLS);
    uint8_t qx[ROWS * COLS], qy[ROWS * COLS];
    int head = 0, tail = 0;
    for (const Ghost &g : ghosts_) {
      if (tick_ < g.release) continue;
      dist[g.y][g.x] = 0;
      qx[tail] = g.x;
      qy[tail++] = g.y;
    }
    while (head < tail) {
      const int x = qx[head], y = qy[head++];
      for (int d = 0; d < 4; d++) {
        int nx = x, ny = y;
        if (!canMove(x, y, d)) continue;
        step(nx, ny, d);
        if (dist[ny][nx] != 255) continue;
        dist[ny][nx] = dist[y][x] + 1;
        qx[tail] = nx;
        qy[tail++] = ny;
      }
    }
  }

  // Breadth-first search from Pac-Man to the nearest wanted cell (a dot,
  // or a ghost while they are frightened), avoiding cells a ghost could
  // reach first; returns the first step, or -1.
  int autopilot() {
    uint8_t ghostDist[ROWS][COLS];
    ghostDistances(ghostDist);
    const bool hunting = frightened_ > 20;
    int8_t first[ROWS][COLS];
    uint8_t depth[ROWS][COLS];
    memset(first, -1, sizeof(first));
    uint8_t qx[ROWS * COLS], qy[ROWS * COLS];
    int head = 0, tail = 0;
    qx[tail] = px_;
    qy[tail++] = py_;
    first[py_][px_] = 4;
    depth[py_][px_] = 0;
    while (head < tail) {
      const int x = qx[head], y = qy[head++];
      const bool target = hunting ? ghostDist[y][x] == 0 : food_[y][x] != 0;
      if (target && (x != px_ || y != py_)) return first[y][x];
      for (int d = 0; d < 4; d++) {
        int nx = x, ny = y;
        if (!canMove(x, y, d)) continue;
        step(nx, ny, d);
        if (first[ny][nx] != -1) continue;
        const int reach = depth[y][x] + 1;
        // Unsafe: a ghost is as close to that cell as we are (with margin).
        if (!hunting && ghostDist[ny][nx] <= reach + 1) continue;
        first[ny][nx] = first[y][x] == 4 ? d : first[y][x];
        depth[ny][nx] = reach;
        qx[tail] = nx;
        qy[tail++] = ny;
      }
    }
    // Nothing safe to go for: move to the neighbour furthest from the ghosts.
    int best = dir_, bestDist = -1;
    for (int d = 0; d < 4; d++) {
      int nx = px_, ny = py_;
      if (!canMove(px_, py_, d)) continue;
      step(nx, ny, d);
      if (ghostDist[ny][nx] > bestDist) {
        bestDist = ghostDist[ny][nx];
        best = d;
      }
    }
    return best;
  }

  void draw() {
    display.clear();
    if (flash_ > 0) {  // level cleared: the walls blink
      if ((flash_ / 4) % 2) {
        for (int y = 0; y < ROWS; y++) {
          for (int x = 0; x < COLS; x++) display.setPixel(x, y, MAZE[y][x] == '#');
        }
      }
      return;
    }
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < COLS; x++) {
        if (food_[y][x]) display.setPixel(x, y, true);
      }
    }
    // Ghosts: on 300 ms, off 200 ms; while they can be eaten, on only one
    // frame in four. Off means dark, even over a dot, so the blink shows.
    const bool ghostOn = frightened_ > 0 ? tick_ % 4 == 0 : tick_ % 10 < 6;
    for (const Ghost &g : ghosts_) display.setPixel(g.x, g.y, ghostOn);
    if (pause_ == 0 || (tick_ / 5) % 2) display.setPixel(px_, py_, true);
    for (int i = 0; i < lives_ - 1; i++) display.setPixel(COLS - 1 - i, 0, true);  // spare lives, top right
  }

  uint8_t food_[ROWS][COLS];  // 0 none, 1 dot, 2 power pellet
  Ghost ghosts_[GHOSTS];
  int px_ = 7, py_ = 13, dir_ = 2, wanted_ = 2;
  int lives_ = 3, points_ = 0, level_ = 0, dotsLeft_ = 0;
  int frightened_ = 0, eatenInFright_ = 0, flash_ = 0, pause_ = 0;
  uint32_t tick_ = 0;
};

const char *const PacManGame::MAZE[ROWS] = {
    "################",
    "#......##......#",
    "#o##.#....#.##o#",
    "#.##.#.##.#.##.#",
    "#..............#",
    "##.##.####.##.##",
    "...#........#...",
    "##.#.#    #.#.##",
    "##.#.#    #.#.##",
    "#..............#",
    "#.##.######.##.#",
    "#o.#........#.o#",
    "##.#.#.##.#.#.##",
    "#....#....#....#",
    "#..............#",
    "################",
};

static PongGame pong;
extern Animation *const pongAnimation = &pong;
static BreakoutGame breakout;
extern Animation *const breakoutAnimation = &breakout;
static FlappyGame flappy;
extern Animation *const flappyAnimation = &flappy;
static InvadersGame invaders;
extern Animation *const invadersAnimation = &invaders;
static PacManGame pacman;
extern Animation *const pacmanAnimation = &pacman;
