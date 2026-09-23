#include "display.h"

#include <SPI.h>
#include <string.h>

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

void Display::begin() {
  pinMode(PIN_LATCH, OUTPUT);
  digitalWrite(PIN_LATCH, LOW);
  // EN is active low, so PWM on it dims the whole panel: the larger the duty
  // cycle, the longer the outputs are off.
  ledcAttach(PIN_ENABLE, 20000, 8);
  setBrightness(255);

  SPI.begin(PIN_CLOCK, -1 /* MISO unused */, PIN_DATA, -1 /* SS unused */);
  SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));

  clear();
  render();
}

void Display::setBrightness(uint8_t brightness) {
  if (brightness == 0) brightness = 1;
  ledcWrite(PIN_ENABLE, 255 - brightness);
}

void Display::clear() { memset(frame_, 0, sizeof(frame_)); }

int Display::frameIndex(int x, int y) {
  if (x < 0 || x >= COLS || y < 0 || y >= ROWS) return -1;
  if (FLIP_HORIZONTAL) x = COLS - 1 - x;
  if (FLIP_VERTICAL) y = ROWS - 1 - y;

  // Rotate clockwise (panel is square, so COLS == ROWS).
  int px = x, py = y;
  if (ROTATION == 90) {
    px = COLS - 1 - y;
    py = x;
  } else if (ROTATION == 180) {
    px = COLS - 1 - x;
    py = ROWS - 1 - y;
  } else if (ROTATION == 270) {
    px = y;
    py = ROWS - 1 - x;
  }
  return py * COLS + px;
}

void Display::setPixel(int x, int y, bool on) {
  int i = frameIndex(x, y);
  if (i >= 0) frame_[i] = on ? 1 : 0;
}

bool Display::getPixel(int x, int y) const {
  int i = frameIndex(x, y);
  return i >= 0 && frame_[i];
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
  static uint8_t bits[TOTAL_PIXELS / 8];
  memset(bits, 0, sizeof(bits));

  for (int chainIndex = 0; chainIndex < TOTAL_PIXELS; chainIndex++) {
    if (frame_[positions[chainIndex]]) {
      bits[chainIndex >> 3] |= (0x80 >> (chainIndex & 7));
    }
  }

  digitalWrite(PIN_LATCH, LOW);
  SPI.writeBytes(bits, sizeof(bits));
  digitalWrite(PIN_LATCH, HIGH);
}

int Display::scrollWidth(const char *text) {
  const int len = strlen(text);
  const char *split = strchr(text, '|');
  if (split == nullptr) return textWidth(text, 0, len);
  const int mid = split - text;
  return max(textWidth(text, 0, mid), textWidth(text, mid + 1, len));
}

void Display::drawScrollFrame(const char *text, int offset) {
  const int len = strlen(text);
  const char *split = strchr(text, '|');

  clear();
  if (split == nullptr) {
    // One line, vertically centred.
    drawText(-offset, (ROWS - FONT_HEIGHT) / 2, text, 0, len);
  } else {
    // Two lines at the top and bottom edges, both starting together.
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
