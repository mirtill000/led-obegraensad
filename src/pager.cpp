#include "pager.h"

#include "display.h"
#include "font_tiny.h"

#include <ctype.h>
#include <string.h>

// Width of `text` in `font`, without the gap after the last letter.
static int widthIn(TextFont font, const String &text) {
  if (!text.length()) return 0;
  return Display::textWidthIn(font, text.c_str(), 0, text.length()) - Display::fontSpacingOf(font);
}
static int linesPerPage(TextFont font) { return font == TextFont::Tiny ? 3 : 2; }
static int lineGap(TextFont font) { return font == TextFont::Tiny ? 1 : font == TextFont::Mini ? 2 : 0; }

int Pager::textWidth(const String &text) {
  int w = 0;
  for (unsigned i = 0; i < text.length(); i++) w += findTinyGlyph(text[i])->width + 1;
  return w > 0 ? w - 1 : 0;
}

void Pager::drawText(int x, int y, const String &text) {
  for (unsigned i = 0; i < text.length() && x < COLS; i++) {
    const TinyGlyph *g = findTinyGlyph(text[i]);
    if (x + g->width > 0) {
      for (int r = 0; r < TINY_HEIGHT; r++) {
        for (int c = 0; c < g->width; c++) {
          if (g->rows[r] & (0x80 >> c)) display.setPixel(x + c, y + r, true);
        }
      }
    }
    x += g->width + 1;
  }
}

static bool isVowel(char c) {
  c = toupper(c);
  return c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U';
}

// Consonant pairs that stay together at the start of a syllable (MA-DRE,
// PA-STA is handled by the S rule, CHIE-SA, GNO-MO).
static bool inseparable(char a, char b) {
  a = toupper(a);
  b = toupper(b);
  if ((b == 'L' || b == 'R') && strchr("BCDFGPTV", a)) return true;
  return (a == 'C' || a == 'G') && (b == 'H' || b == 'N');
}

// Where `word` may be split, by the main Italian syllable rules: a single
// consonant goes with the next vowel (CO-ME), double consonants split
// (LEZ-ZA), S + consonant and the pairs above go with the next vowel
// (PA-STA, MA-DRE), otherwise the split is after the first consonant
// (PER-SO-NA); also after an apostrophe or a hyphen.
static std::vector<bool> syllableBreaks(const String &word) {
  const int n = word.length();
  std::vector<bool> ok(n + 1, false);
  int lastVowel = -1;
  for (int i = 0; i < n; i++) {
    const char c = word[i];
    if ((c == '\'' || c == '-') && i + 1 < n) ok[i + 1] = true;
    if (!isVowel(c)) continue;
    if (lastVowel >= 0) {
      const int s = lastVowel + 1, len = i - s;  // consonants between two vowels
      bool letters = true;
      for (int k = s; k < i; k++) letters = letters && isalpha(word[k]);
      if (len > 0 && letters) {
        int split = s + 1;  // after the first consonant: PER-SO-NA, and doubles: LEZ-ZA
        const bool doubled = len >= 2 && toupper(word[s]) == toupper(word[s + 1]);
        if (!doubled && (len == 1 || toupper(word[s]) == 'S' || (len == 2 && inseparable(word[s], word[s + 1])))) split = s;
        ok[split] = true;
      }
    }
    lastVowel = i;
  }
  return ok;
}

// Pieces of a word too wide for a line: as many syllables per piece as fit
// in COLS pixels (a syllable that alone is too wide is cut).
static std::vector<String> splitWord(TextFont font, const String &word) {
  const std::vector<bool> ok = syllableBreaks(word);
  std::vector<String> pieces;
  unsigned start = 0;
  while (start < word.length()) {
    unsigned end = 0;
    for (unsigned p = start + 1; p <= word.length(); p++) {
      if ((p == word.length() || ok[p]) && widthIn(font, word.substring(start, p)) <= COLS) end = p;
    }
    if (end == 0) {  // no syllable break fits: cut at the widest that fits
      end = start + 1;
      while (end < word.length() && widthIn(font, word.substring(start, end + 1)) <= COLS) end++;
    }
    pieces.push_back(word.substring(start, end));
    start = end;
  }
  return pieces;
}

// Lines up to COLS pixels wide, words kept whole where they fit and split
// by syllables where they don't.
static std::vector<String> wrap(TextFont font, const String &text) {
  std::vector<String> lines;
  String line;
  int start = 0;
  while (start < (int)text.length()) {
    int end = text.indexOf(' ', start);
    if (end < 0) end = text.length();
    String word = text.substring(start, end);
    start = end + 1;
    if (word.length() == 0) continue;
    if (widthIn(font, word) > COLS) {  // too wide for any line: by syllables
      if (line.length()) {
        lines.push_back(line);
        line = "";
      }
      std::vector<String> pieces = splitWord(font, word);
      for (size_t k = 0; k + 1 < pieces.size(); k++) lines.push_back(pieces[k]);
      word = pieces.back();  // the last piece may share a line with what follows
    }
    const String with = line.length() ? line + " " + word : word;
    if (widthIn(font, with) <= COLS) {
      line = with;
    } else {
      lines.push_back(line);
      line = word;
    }
  }
  if (line.length()) lines.push_back(line);
  return lines;
}

void Pager::start(const String &text) {
  // "Attuale" pages use its compact letters, as scrolling text does.
  const TextFont f = Display::scrollFont();
  start(text, f == TextFont::Small ? TextFont::Compact : f);
}

void Pager::start(const String &text, TextFont font) {
  font_ = font;
  lastStep_ = millis();
  display.beginTransition();
  if (font_ == TextFont::Big) {
    text_ = Display::fontText(text);
    text_.replace('|', ' ');
    width_ = Display::textWidthIn(TextFont::Big, text_.c_str(), 0, text_.length());
    offset_ = -COLS;
    lines_.clear();
    draw();
    return;
  }
  lines_ = wrap(font_, Display::fontText(text));
  page_ = 0;
  draw();
}

bool Pager::update(uint32_t now, uint32_t pageMs) {
  if (font_ == TextFont::Big) {
    // Scrolls at the same pace the speed setting gives scrolling text.
    if (now - lastStep_ < max<uint32_t>(1, pageMs * SCROLL_DELAY_MS / PAGE_MS)) return false;
    lastStep_ = now;
    if (++offset_ >= width_) return true;
    draw();
    return false;
  }
  if (now - lastStep_ < pageMs) return false;
  lastStep_ = now;
  page_++;
  if (page_ * linesPerPage(font_) >= lines_.size()) return true;
  display.beginTransition();
  draw();
  return false;
}

void Pager::draw() {
  display.clear();
  if (font_ == TextFont::Big) {
    display.drawTextIn(TextFont::Big, -offset_, 0, text_.c_str(), 0, text_.length());
    display.render();
    return;
  }
  const int per = linesPerPage(font_), h = Display::fontHeightOf(font_), gap = lineGap(font_);
  const size_t first = page_ * per;
  const int n = first < lines_.size() ? min((int)(lines_.size() - first), per) : 0;
  const int height = n * h + (n - 1) * gap;
  int y = max(0, (ROWS - height) / 2);
  for (int i = 0; i < n; i++) {
    const String &line = lines_[first + i];
    display.drawTextIn(font_, (COLS - widthIn(font_, line)) / 2, y, line.c_str(), 0, line.length());
    y += h + gap;
  }
  display.render();
}
