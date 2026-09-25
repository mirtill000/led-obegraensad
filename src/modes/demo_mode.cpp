#include "modes/demo_mode.h"

#include "display.h"
#include "font_mini.h"
#include "font_tiny.h"
#include "modes/quotes_mode.h"
#include "settings.h"

static const uint32_t PAGE_MS = 2500;  // each still page (scaled by the speed setting)

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

// Lines up to COLS pixels wide, words kept whole unless one alone is too
// wide (then it is cut where it has to be).
static std::vector<String> wrappedLines(Font f, const String &text) {
  std::vector<String> lines;
  String line;
  for (String word : words(text)) {
    while (textWidth(f, word) > COLS) {  // too long for any line: cut it
      if (line.length()) {
        lines.push_back(line);
        line = "";
      }
      unsigned n = 1;
      while (n < word.length() && textWidth(f, word.substring(0, n + 1)) <= COLS) n++;
      lines.push_back(word.substring(0, n));
      word = word.substring(n);
    }
    const String with = line.length() ? line + " " + word : word;
    if (textWidth(f, with) <= COLS) {
      line = with;
    } else {
      lines.push_back(line);
      line = word;
    }
  }
  if (line.length()) lines.push_back(line);
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
  quote_ = Display::fontText(QuotesMode::quoteAt(index_++));

  if (settings.demoStyle == "rows3") style_ = ROWS3;
  else if (settings.demoStyle == "pages") style_ = PAGES;
  else if (settings.demoStyle == "rows2") style_ = ROWS2;
  else style_ = (Style)(autoStyle_++ % STYLES);

  layout();
  offset_ = -COLS;
  page_ = 0;
  lastStep_ = millis();
  display.beginTransition();
  if (style_ == PAGES) drawPage();
  else drawScroll();
}

void DemoMode::layout() {
  if (style_ == PAGES) {
    lines_ = wrappedLines(Font::Tiny, quote_);
    width_ = 0;
    return;
  }
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

// Pages: up to three lines, each centred, the block centred vertically.
void DemoMode::drawPage() {
  display.clear();
  const size_t first = page_ * 3;
  const int n = min((int)(lines_.size() - first), 3);
  const int height = n * TINY_HEIGHT + (n - 1);
  int y = (ROWS - height) / 2;
  for (int i = 0; i < n; i++) {
    const String &l = lines_[first + i];
    drawText(Font::Tiny, (COLS - textWidth(Font::Tiny, l)) / 2, y, l);
    y += TINY_HEIGHT + 1;
  }
  display.render();
}

void DemoMode::update(uint32_t now) {
  if (!started_) return next();
  if (style_ == PAGES) {
    if (now - lastStep_ < interval(PAGE_MS)) return;
    lastStep_ = now;
    page_++;
    if (page_ * 3 >= lines_.size()) return next();
    display.beginTransition();
    drawPage();
    return;
  }
  if (now - lastStep_ < interval(SCROLL_DELAY_MS)) return;
  lastStep_ = now;
  if (++offset_ >= width_) return next();  // the longest line has gone by
  drawScroll();
}
