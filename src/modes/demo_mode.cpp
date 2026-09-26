#include "modes/demo_mode.h"

#include "display.h"
#include "font_mini.h"
#include "font_tiny.h"
#include "modes/quotes_mode.h"
#include "settings.h"

// --- text in the two small fonts --------------------------------------------

enum class Font : uint8_t { Tiny, Mini };

static int glyphWidth(Font f, char c) { return f == Font::Tiny ? findTinyGlyph(c)->width : findMiniGlyph(c)->width; }

// Width in pixels, with one blank column between glyphs.
static int textWidth(Font f, const String &s) {
  int w = 0;
  for (unsigned i = 0; i < s.length(); i++) w += glyphWidth(f, s[i]) + 1;
  return w > 0 ? w - 1 : 0;
}

static void drawText(Font f, int x, int y, const String &s) {
  for (unsigned i = 0; i < s.length() && x < COLS; i++) {
    const int w = glyphWidth(f, s[i]);
    const int h = f == Font::Tiny ? TINY_HEIGHT : MINI_HEIGHT;
    const uint8_t *rows = f == Font::Tiny ? findTinyGlyph(s[i])->rows : findMiniGlyph(s[i])->rows;
    if (x + w > 0) {
      for (int r = 0; r < h; r++) {
        for (int c = 0; c < w; c++) {
          if (rows[r] & (0x80 >> c)) display.setPixel(x + c, y + r, true);
        }
      }
    }
    x += w + 1;
  }
}

static std::vector<String> words(const String &text) {
  std::vector<String> out;
  int start = 0;
  while (start < (int)text.length()) {
    int end = text.indexOf(' ', start);
    if (end < 0) end = text.length();
    if (end > start) out.push_back(text.substring(start, end));
    start = end + 1;
  }
  return out;
}

// Splits `text` into `count` lines of about the same width, at spaces.
static std::vector<String> balancedLines(Font f, const String &text, int count) {
  const std::vector<String> w = words(text);
  const float target = textWidth(f, text) / (float)count;
  std::vector<String> lines;
  size_t i = 0;
  for (int line = 0; line < count; line++) {
    String s;
    if (line == count - 1) {
      for (; i < w.size(); i++) s += (s.length() ? " " : "") + w[i];
    } else {
      while (i < w.size()) {
        const String with = s.length() ? s + " " + w[i] : w[i];
        // Take the word if the line stays nearer the target with it.
        if (s.length() && fabsf(textWidth(f, with) - target) > fabsf(textWidth(f, s) - target)) break;
        s = with;
        i++;
      }
    }
    lines.push_back(s);
  }
  return lines;
}

// --- the mode -----------------------------------------------------------------

void DemoMode::start() {
  started_ = false;  // update() picks the first quote
}

void DemoMode::next() {
  const uint16_t count = QuotesMode::count();
  if (!started_) index_ = count ? esp_random() % count : 0;
  started_ = true;
  const String quote = QuotesMode::quoteAt(index_++);
  quote_ = Display::fontText(quote);

  if (settings.demoStyle == "rows3") style_ = ROWS3;
  else if (settings.demoStyle == "pages") style_ = PAGES;
  else if (settings.demoStyle == "rows2") style_ = ROWS2;
  else style_ = (Style)(autoStyle_++ % STYLES);

  if (style_ == PAGES) {
    pager_.start(quote);
    return;
  }
  layout();
  offset_ = -COLS;
  lastStep_ = millis();
  display.beginTransition();
  drawScroll();
}

void DemoMode::layout() {
  const Font f = style_ == ROWS3 ? Font::Tiny : Font::Mini;
  lines_ = balancedLines(f, quote_, style_ == ROWS3 ? 3 : 2);
  width_ = 0;
  for (const String &l : lines_) width_ = max(width_, textWidth(f, l));
}

// Scrolling styles: the lines move together, left-aligned.
//   rows3: rows 1-4, 6-9, 11-14    rows2: rows 2-6, 9-13
void DemoMode::drawScroll() {
  display.clear();
  const Font f = style_ == ROWS3 ? Font::Tiny : Font::Mini;
  static const int ROWS3_Y[] = {1, 6, 11}, ROWS2_Y[] = {2, 9};
  for (size_t i = 0; i < lines_.size(); i++) drawText(f, -offset_, style_ == ROWS3 ? ROWS3_Y[i] : ROWS2_Y[i], lines_[i]);
  display.render();
}

void DemoMode::update(uint32_t now) {
  if (!started_) return next();
  if (style_ == PAGES) {
    if (pager_.update(now, interval(PAGE_MS))) next();
    return;
  }
  if (now - lastStep_ < interval(SCROLL_DELAY_MS)) return;
  lastStep_ = now;
  if (++offset_ >= width_) return next();  // the longest line has gone by
  drawScroll();
}
