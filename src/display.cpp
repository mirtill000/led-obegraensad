#include "display.h"

#include <SPI.h>
#include <driver/gptimer.h>
#include <esp_timer.h>
#include <math.h>
#include <string.h>

#include "font_mini.h"
#include "font_compact.h"
#include "font_small.h"

Display display;

// ---------------------------------------------------------------------------
// Physical bit order of the OBEGRÄNSAD panel's shift-register chain.
//
// The four 8x8 plates are not wired in simple row-major order internally;
// positions[chainIndex] gives the (row * COLS + col) frame index that ends
// up at bit `chainIndex` of the 256-bit chain. This mapping is a property of
// the lamp's own PCB traces, not of the controller, so it is the same
// regardless of which microcontroller drives it.
// ---------------------------------------------------------------------------
static const uint8_t positions[TOTAL_PIXELS] = {
    0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21, 0x20, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x2f, 0x2e, 0x2d, 0x2c, 0x2b, 0x2a, 0x29, 0x28, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x4f, 0x4e, 0x4d, 0x4c, 0x4b, 0x4a, 0x49, 0x48, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x47, 0x46, 0x45, 0x44, 0x43, 0x42, 0x41, 0x40, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
    0x67, 0x66, 0x65, 0x64, 0x63, 0x62, 0x61, 0x60, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x69, 0x68, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
    0x8f, 0x8e, 0x8d, 0x8c, 0x8b, 0x8a, 0x89, 0x88, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
    0x87, 0x86, 0x85, 0x84, 0x83, 0x82, 0x81, 0x80, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0xa7, 0xa6, 0xa5, 0xa4, 0xa3, 0xa2, 0xa1, 0xa0, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
    0xaf, 0xae, 0xad, 0xac, 0xab, 0xaa, 0xa9, 0xa8, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xcf, 0xce, 0xcd, 0xcc, 0xcb, 0xca, 0xc9, 0xc8, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
    0xc7, 0xc6, 0xc5, 0xc4, 0xc3, 0xc2, 0xc1, 0xc0, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
    0xe7, 0xe6, 0xe5, 0xe4, 0xe3, 0xe2, 0xe1, 0xe0, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xef, 0xee, 0xed, 0xec, 0xeb, 0xea, 0xe9, 0xe8, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
};

// ---------------------------------------------------------------------------
// Grayscale refresh (binary code modulation).
//
// The panel's driver chips only know on or off per LED, so gray levels are
// made in time: render() turns the frame into PLANES bit planes (plane p
// holds bit p of every pixel's 5-bit level) and plane p is shown for
// 2^p time units, so over one cycle each LED is lit for a time
// proportional to its level. Three buffers let render() hand over a new
// frame without ever touching the one being displayed.
//
// With REFRESH_HW_TIMER a hardware timer (gptimer) on core 1, away from
// WiFi on core 0, counts plane times in TICK_US units. When a plane's time
// is up its interrupt wakes a top-priority task on core 1, which first latches the
// plane already shifted into the registers - so the plane changes exactly
// on the tick - and then shifts in the next one while it is shown. The
// older path (esp_timer, core 0) is kept behind the switch.
// ---------------------------------------------------------------------------
static const int PLANES = 5;
static const uint32_t PLANE_US[PLANES] = {120, 240, 480, 960, 1920};  // esp_timer path: ~3.7 ms per cycle
static const uint32_t TICK_US = 100;                                   // hardware path: 3.1 ms per cycle
static DRAM_ATTR const uint8_t PLANE_TICKS[PLANES] = {1, 2, 4, 8, 16};
static const int FRAME_BYTES = TOTAL_PIXELS / 8;

struct PlaneSet {
  uint8_t bits[PLANES][FRAME_BYTES];
};
static PlaneSet planeSets[3];
static volatile int8_t frontSet = 0;     // being displayed by the refresh
static volatile int8_t pendingSet = -1;  // next frame, picked up at cycle start
static portMUX_TYPE setLock = portMUX_INITIALIZER_UNLOCKED;
static uint8_t gammaTable[256];  // level 0-255 -> 0-31 on-time units

static void shiftBits(const uint8_t *bits) { SPI.writeBytes(bits, FRAME_BYTES); }
// The registers' outputs take what was shifted in on the latch's rising edge.
static void latch() {
  digitalWrite(PIN_LATCH, HIGH);
  digitalWrite(PIN_LATCH, LOW);
}
static void pushBits(const uint8_t *bits) {
  shiftBits(bits);
  latch();
}

// At the start of a cycle, take the newest frame if render() left one.
static void takePendingFrame() {
  portENTER_CRITICAL(&setLock);
  if (pendingSet >= 0) {
    frontSet = pendingSet;
    pendingSet = -1;
  }
  portEXIT_CRITICAL(&setLock);
}

// --- hardware timer path ---------------------------------------------------
static gptimer_handle_t tickTimer;
static TaskHandle_t refreshTask;
static volatile uint8_t isrPlane = PLANES - 1;  // plane being shown (onTick() moves on first)
static volatile int64_t notifiedAt = 0;  // when the interrupt woke the task, us

// Refresh statistics for the diagnostics page (hardware path only).
static volatile uint32_t statPlanes = 0, statMissed = 0, statMaxLatency = 0;
static volatile uint64_t statLatencySum = 0;

// The alarm fires once per plane, when its time is up, and is re-armed for
// the length of the next one (a fixed 100 us tick would interrupt the core
// 10000 times a second; this is 1600).
static bool IRAM_ATTR onTick(gptimer_handle_t timer, const gptimer_alarm_event_data_t *event, void *) {
  isrPlane = isrPlane + 1 == PLANES ? 0 : isrPlane + 1;
  gptimer_alarm_config_t alarm = {};
  alarm.alarm_count = event->alarm_value + PLANE_TICKS[isrPlane] * TICK_US;
  gptimer_set_alarm_action(timer, &alarm);
  notifiedAt = esp_timer_get_time();
  BaseType_t woken = pdFALSE;
  vTaskNotifyGiveFromISR(refreshTask, &woken);
  return woken == pdTRUE;  // switch to the refresh task right away
}

static void refreshLoop(void *) {
  shiftBits(planeSets[frontSet].bits[0]);
  for (;;) {
    const uint32_t wakes = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    latch();  // plane isrPlane (shifted in last time) is shown from now
    const uint32_t latency = (uint32_t)(esp_timer_get_time() - notifiedAt);
    statPlanes = statPlanes + 1;
    if (wakes > 1) statMissed = statMissed + (wakes - 1);  // plane changes this task never saw
    statLatencySum = statLatencySum + latency;
    if (latency > statMaxLatency) statMaxLatency = latency;
    const uint8_t next = isrPlane + 1 == PLANES ? 0 : isrPlane + 1;
    if (next == 0) takePendingFrame();
    shiftBits(planeSets[frontSet].bits[next]);
  }
}

static void startHardwareRefresh() {
  xTaskCreatePinnedToCore(refreshLoop, "display", 3072, nullptr, configMAX_PRIORITIES - 1, &refreshTask, 1);
  const gptimer_config_t config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = 1000000,  // 1 us
  };
  gptimer_new_timer(&config, &tickTimer);
  const gptimer_event_callbacks_t callbacks = {.on_alarm = onTick};
  gptimer_register_event_callbacks(tickTimer, &callbacks, nullptr);  // interrupt on this core (1)
  gptimer_alarm_config_t alarm = {};
  alarm.alarm_count = PLANE_TICKS[0] * TICK_US;  // the first plane, then onTick() re-arms it
  gptimer_set_alarm_action(tickTimer, &alarm);
  gptimer_enable(tickTimer);
  gptimer_start(tickTimer);
}

Display::RefreshStats Display::refreshStats() {
  RefreshStats r;
  r.hardwareTimer = GRAYSCALE && REFRESH_HW_TIMER;
  r.planes = statPlanes;
  r.missed = statMissed;
  r.maxLatencyUs = statMaxLatency;
  r.avgLatencyUs = r.planes ? (uint32_t)(statLatencySum / r.planes) : 0;
  r.cycleUs = TICK_US * 31;
  return r;
}

void Display::resetRefreshStats() {
  statPlanes = 0;
  statMissed = 0;
  statMaxLatency = 0;
  statLatencySum = 0;
}

// --- esp_timer path (REFRESH_HW_TIMER false) -------------------------------
static esp_timer_handle_t refreshTimer;

static void refreshTick(void *) {
  static uint8_t plane = 0;
  if (plane == 0) takePendingFrame();
  pushBits(planeSets[frontSet].bits[plane]);
  esp_timer_start_once(refreshTimer, PLANE_US[plane]);
  plane = (plane + 1) % PLANES;
}

static void startTimerRefresh() {
  const esp_timer_create_args_t args = {
      .callback = refreshTick,
      .arg = nullptr,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "display",
      .skip_unhandled_events = true,
  };
  esp_timer_create(&args, &refreshTimer);
  esp_timer_start_once(refreshTimer, PLANE_US[0]);
}

void Display::begin() {
  pinMode(PIN_LATCH, OUTPUT);
  digitalWrite(PIN_LATCH, LOW);
  // EN is active low, so PWM on it dims the whole panel: the larger the duty
  // cycle, the longer the outputs are off.
  ledcAttach(PIN_ENABLE, 39000, 8);
  setBrightness(255);

  SPI.begin(PIN_CLOCK, -1 /* MISO unused */, PIN_DATA, -1 /* SS unused */);
  SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));

  for (int i = 0; i < 256; i++) {
    const int units = (int)lroundf(31.0f * powf(i / 255.0f, 2.2f));
    gammaTable[i] = (i > 0 && units == 0) ? 1 : units;  // any level > 0 stays visible
  }

  clear();
  render();

  if (GRAYSCALE) {
    if (REFRESH_HW_TIMER) startHardwareRefresh();
    else startTimerRefresh();
  }
}

void Display::setBrightness(uint8_t brightness) {
  if (brightness == 0) brightness = 1;
  ledcWrite(PIN_ENABLE, 255 - brightness);
}

void Display::clear() { memset(frame_, 0, sizeof(frame_)); }

int Display::frameIndex(int x, int y) const {
  if (x < 0 || x >= COLS || y < 0 || y >= ROWS) return -1;
  if (FLIP_HORIZONTAL) x = COLS - 1 - x;
  if (FLIP_VERTICAL) y = ROWS - 1 - y;

  // Rotate clockwise (panel is square, so COLS == ROWS).
  int px = x, py = y;
  if (rotation_ == 90) {
    px = COLS - 1 - y;
    py = x;
  } else if (rotation_ == 180) {
    px = COLS - 1 - x;
    py = ROWS - 1 - y;
  } else if (rotation_ == 270) {
    px = y;
    py = ROWS - 1 - x;
  }
  return py * COLS + px;
}

void Display::setLevel(int x, int y, uint8_t level) {
  int i = frameIndex(x, y);
  if (i >= 0) frame_[i] = level;
}

uint8_t Display::getLevel(int x, int y) const {
  int i = frameIndex(x, y);
  return i >= 0 ? frame_[i] : 0;
}

int Display::drawChar(int x, int y, char c) {
  const Glyph *g = findGlyph(c);
  for (int row = 0; row < FONT_HEIGHT; row++) {
    for (int col = 0; col < g->width; col++) {
      if (g->rows[row] & (0x80 >> col)) setPixel(x + col, y + row, true);
    }
  }
  return g->width;
}

int Display::textWidth(const char *text, int start, int end) {
  int width = 0;
  for (int i = start; i < end; i++) {
    width += findGlyph(text[i])->width + FONT_SPACING;
  }
  return width;
}

void Display::drawText(int x, int y, const char *text, int start, int end) {
  for (int i = start; i < end && x < COLS; i++) {
    x += drawChar(x, y, text[i]) + FONT_SPACING;
  }
}

void Display::drawBitmap(int x, int y, const uint16_t *bitmap, int width, int rows) {
  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < width; col++) {
      if (bitmap[row] & (0x8000 >> col)) setPixel(x + col, y + row, true);
    }
  }
}

void Display::render() {
  memcpy(target_, frame_, sizeof(target_));
  output();
}

static const uint32_t FADE_MS = 600;
static const uint32_t WIPE_MS = 500;
static const uint32_t BLEND_STEP_MS = 16;

void Display::beginTransition() {
  if (transition_ == Transition::None) return;
  memcpy(from_, shown_, sizeof(from_));
  blending_ = true;
  blendStart_ = millis();
}

void Display::tick(uint32_t now) {
  if (blending_ && now - lastBlend_ >= BLEND_STEP_MS) output();
}

uint8_t Display::shownLevel(int x, int y) const {
  const int i = frameIndex(x, y);
  return i >= 0 ? shown_[i] : 0;
}

void Display::output() {
  const uint32_t now = millis();
  lastBlend_ = now;
  const uint32_t duration = transition_ == Transition::Wipe ? WIPE_MS : FADE_MS;
  if (blending_ && (transition_ == Transition::None || now - blendStart_ >= duration)) blending_ = false;
  if (!blending_) {
    memcpy(shown_, target_, sizeof(shown_));
  } else {
    float t = (now - blendStart_) / (float)duration;
    t = t * t * (3 - 2 * t);  // ease in and out
    if (transition_ == Transition::Fade) {
      for (int i = 0; i < TOTAL_PIXELS; i++) shown_[i] = (uint8_t)(from_[i] + (target_[i] - from_[i]) * t + 0.5f);
    } else {
      // The edge moves across the logical columns (whatever the rotation),
      // one column of soft blend wide.
      const float edge = t * (COLS + 1);
      for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
          const int i = frameIndex(x, y);
          const float k = constrain(edge - x, 0.0f, 1.0f);
          shown_[i] = (uint8_t)(from_[i] + (target_[i] - from_[i]) * k + 0.5f);
        }
      }
    }
  }

  if (!GRAYSCALE) {
    static uint8_t bits[FRAME_BYTES];
    memset(bits, 0, sizeof(bits));
    for (int chainIndex = 0; chainIndex < TOTAL_PIXELS; chainIndex++) {
      if (shown_[positions[chainIndex]]) bits[chainIndex >> 3] |= (0x80 >> (chainIndex & 7));
    }
    pushBits(bits);
    return;
  }

  // Build the planes in the buffer that is neither shown nor queued.
  portENTER_CRITICAL(&setLock);
  const int8_t front = frontSet, pending = pendingSet;
  portEXIT_CRITICAL(&setLock);
  int8_t target = 0;
  while (target == front || target == pending) target++;

  PlaneSet &set = planeSets[target];
  memset(&set, 0, sizeof(set));
  for (int chainIndex = 0; chainIndex < TOTAL_PIXELS; chainIndex++) {
    const uint8_t units = gammaTable[shown_[positions[chainIndex]]];
    const uint8_t mask = 0x80 >> (chainIndex & 7);
    for (int p = 0; p < PLANES; p++) {
      if (units & (1 << p)) set.bits[p][chainIndex >> 3] |= mask;
    }
  }

  portENTER_CRITICAL(&setLock);
  pendingSet = target;
  portEXIT_CRITICAL(&setLock);
}

String Display::fontText(const String &utf8) {
  // Two-byte UTF-8 sequences starting with 0xC3, second byte -> the plain
  // letter; an apostrophe after it stands for the accent.
  static const struct {
    uint8_t second;
    char out;
  } LATIN[] = {
      {0xA0, 'a'}, {0xA1, 'a'}, {0xA8, 'e'}, {0xA9, 'e'}, {0xAC, 'i'}, {0xAD, 'i'},
      {0xB2, 'o'}, {0xB3, 'o'}, {0xB9, 'u'}, {0xBA, 'u'},  // lowercase
      {0x80, 'A'}, {0x81, 'A'}, {0x88, 'E'}, {0x89, 'E'}, {0x8C, 'I'}, {0x8D, 'I'},
      {0x92, 'O'}, {0x93, 'O'}, {0x99, 'U'}, {0x9A, 'U'},  // capitals
  };
  String out;
  out.reserve(utf8.length());
  for (unsigned i = 0; i < utf8.length(); i++) {
    const uint8_t c = utf8[i];
    if (c < 0x80) {
      out += (char)c;
      continue;
    }
    // Length of this UTF-8 sequence, to skip it as a whole.
    const int len = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : 2;
    char mapped = ' ';
    if (c == 0xC3 && i + 1 < utf8.length()) {
      for (const auto &l : LATIN) {
        if ((uint8_t)utf8[i + 1] == l.second) {
          out += l.out;
          mapped = '\'';  // "perché" -> "perche'"
        }
      }
    } else if (c == 0xE2 && i + 2 < utf8.length() && (uint8_t)utf8[i + 1] == 0x80) {
      const uint8_t third = utf8[i + 2];
      if (third == 0x98 || third == 0x99) mapped = '\'';  // curly apostrophe
      if (third == 0x93 || third == 0x94) mapped = '-';   // en and em dash
    }
    out += mapped;
    i += len - 1;
  }
  return out;
}

// ---------------------------------------------------------------------------
// Fonts for scrolling text.

static TextFont scrollFont_ = TextFont::Small;
static const int GLYPH_COUNT = sizeof(FONT_GLYPHS) / sizeof(FONT_GLYPHS[0]);

void Display::setScrollFont(TextFont font) { scrollFont_ = font; }
TextFont Display::scrollFont() { return scrollFont_; }

static int fontHeight(TextFont f) { return f == TextFont::Big ? 16 : f == TextFont::Mini ? MINI_HEIGHT : FONT_HEIGHT; }
static int fontSpacing(TextFont f) { return f == TextFont::Big ? 2 : FONT_SPACING; }
int Display::scrollFontHeight() { return fontHeight(scrollFont_); }

// The big font: every small glyph scaled x2 with EPX (Scale2x), which keeps
// diagonals smooth instead of doubling the steps. Built once, on first use.
static uint16_t bigRows[GLYPH_COUNT][16];
static void buildBigFont() {
  static bool built = false;
  if (built) return;
  built = true;
  for (int i = 0; i < GLYPH_COUNT; i++) {
    const Glyph &g = FONT_GLYPHS[i];
    auto P = [&](int x, int y) -> bool {
      return x >= 0 && x < g.width && y >= 0 && y < FONT_HEIGHT && (g.rows[y] & (0x80 >> x));
    };
    for (int r = 0; r < 16; r++) bigRows[i][r] = 0;
    for (int y = 0; y < FONT_HEIGHT; y++) {
      for (int x = 0; x < g.width; x++) {
        const bool p = P(x, y), a = P(x, y - 1), b = P(x + 1, y), c = P(x - 1, y), d = P(x, y + 1);
        const bool out[4] = {(c == a && c != d && a != b) ? a : p, (a == b && a != c && b != d) ? b : p,
                             (d == c && d != b && c != a) ? c : p, (b == d && b != a && d != c) ? d : p};
        for (int k = 0; k < 4; k++) {
          if (out[k]) bigRows[i][2 * y + k / 2] |= 0x8000 >> (2 * x + k % 2);
        }
      }
    }
  }
}

// Rows of `c` in `font` (bit 15 = leftmost column); returns its width.
static int glyphRows(TextFont font, char c, uint16_t rows[16]) {
  if (font == TextFont::Mini) {
    const MiniGlyph *g = findMiniGlyph(c);
    for (int r = 0; r < MINI_HEIGHT; r++) rows[r] = g->rows[r] << 8;
    return g->width;
  }
  const Glyph *g = font == TextFont::Compact ? findCompactGlyph(c) : findGlyph(c);
  if (font == TextFont::Big) {
    buildBigFont();
    memcpy(rows, bigRows[g - FONT_GLYPHS], sizeof(bigRows[0]));
    return g->width * 2;
  }
  for (int r = 0; r < FONT_HEIGHT; r++) rows[r] = g->rows[r] << 8;
  return g->width;
}

int Display::textWidthIn(TextFont font, const char *text, int start, int end) {
  if (font == TextFont::Small) return textWidth(text, start, end);
  int width = 0;
  uint16_t rows[16];
  for (int i = start; i < end; i++) width += glyphRows(font, text[i], rows) + fontSpacing(font);
  return width;
}

void Display::drawTextIn(TextFont font, int x, int y, const char *text, int start, int end) {
  if (font == TextFont::Small) return drawText(x, y, text, start, end);
  uint16_t rows[16];
  const int height = fontHeight(font);
  for (int i = start; i < end && x < COLS; i++) {
    const int w = glyphRows(font, text[i], rows);
    if (x + w >= 0) drawBitmap(x, y, rows, w, height);
    x += w + fontSpacing(font);
  }
}

int Display::textRow(const String &position, int previous) {
  const int lowest = ROWS - scrollFontHeight();  // text touching the bottom edge
  if (position == "top") return 0;
  if (position == "bottom") return lowest;
  if (position != "random" || lowest < 3) return lowest / 2;  // the big font has no room to move
  int row;
  do {
    row = esp_random() % (lowest + 1);
  } while (previous >= 0 && abs(row - previous) < 2);
  return row;
}

// The font scrolling text is drawn in: Small ("Attuale") scrolls with its
// compact letters.
static TextFont scrollingFont() { return scrollFont_ == TextFont::Small ? TextFont::Compact : scrollFont_; }

int Display::scrollWidth(const char *text) {
  const int len = strlen(text);
  const char *split = strchr(text, '|');
  if (split == nullptr) return textWidthIn(scrollingFont(), text, 0, len);
  const int mid = split - text;
  return max(textWidth(text, 0, mid), textWidth(text, mid + 1, len));
}

void Display::drawScrollFrame(const char *text, int offset, int y) {
  const int len = strlen(text);
  const char *split = strchr(text, '|');

  clear();
  if (split == nullptr) {
    // One line in the scroll font, at row y or vertically centred.
    const int lowest = ROWS - scrollFontHeight();
    drawTextIn(scrollingFont(), -offset, y >= 0 ? min(y, lowest) : lowest / 2, text, 0, len);
  } else {
    // Two lines (small font) at the top and bottom edges, both starting together.
    const int mid = split - text;
    drawText(-offset, 0, text, 0, mid);
    drawText(-offset, ROWS - FONT_HEIGHT, text, mid + 1, len);
  }
  render();
}

void Display::scrollTextOnce(const char *text, uint16_t frameDelayMs) {
  const int width = scrollWidth(text);
  for (int offset = -COLS; offset < width; offset++) {
    drawScrollFrame(text, offset);
    delay(frameDelayMs);
  }
}
