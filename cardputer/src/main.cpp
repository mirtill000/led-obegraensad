// OBEGRÄNSAD remote on an M5Stack Cardputer ADV.
//
// A menu (; . to move, Enter to choose, ` to go back) with:
//   Telecomando giochi  - the lamp's picture on screen; ; . , / are the
//                         arrows, Space/Enter the main button, D demo on/off,
//                         X the lamp's mode button (next game), Esc back
//   Giochi / Modalita' / Animazioni - pick one from the lamp's own lists
//   Scrivi un testo     - type it, Enter: the lamp scrolls it
//   Notifica            - an icon (, /) and a text, Enter: sent as a
//                         notification
//   Luminosita'         - , / to change it
//   Impostazioni        - forget the lamp (pair again with a new PIN)
// The first time it asks for the PIN shown in the lamp's web page
// (Bluetooth section).
#include <M5Cardputer.h>

#include "lamp.h"

namespace {

M5Canvas canvas(&M5Cardputer.Display);

const uint16_t BG = TFT_BLACK, FG = TFT_WHITE, DIM = 0x8410, ACCENT = 0xFD20;  // orange
const int W = 240, H = 135;

enum class Screen : uint8_t { Menu, Remote, List, Text, Notify, Brightness, Settings };
Screen screen = Screen::Menu;

const char *const MENU[] = {"Telecomando giochi", "Giochi",   "Modalita'",  "Animazioni",
                            "Scrivi un testo",    "Notifica", "Luminosita'", "Impostazioni"};
const int MENU_COUNT = sizeof(MENU) / sizeof(MENU[0]);
int menuPos = 0;

char listKind = 'G';  // the List screen: 'G' games, 'M' modes, 'A' animations
int listPos = 0;
std::vector<lamp::Item> listItems;

String typed;  // PIN, text, notification
const char *const ICONS[] = {"bell", "mail", "check", "alert", "heart", "phone", "home", "star", ""};
const char *const ICON_NAMES[] = {"campanella", "busta", "spunta", "attenzione", "cuore", "telefono", "casa", "stella", "nessuna"};
const int ICON_COUNT = 9;
int iconPos = 0;
String flash;  // a short confirmation at the bottom
uint32_t flashUntil = 0;

// The lamp's names are UTF-8 with Italian accents; the font is ASCII.
String plain(const String &s) {
  String out;
  for (unsigned i = 0; i < s.length(); i++) {
    const uint8_t c = s[i];
    if (c == 0xC3 && i + 1 < s.length()) {
      const uint8_t d = s[++i];
      if (d >= 0xA0 && d <= 0xA5) out += "a'";
      else if (d >= 0xA8 && d <= 0xAB) out += "e'";
      else if (d >= 0xAC && d <= 0xAF) out += "i'";
      else if (d >= 0xB2 && d <= 0xB6) out += "o'";
      else if (d >= 0xB9 && d <= 0xBC) out += "u'";
      else if (d == 0x88 || d == 0x89) out += "E'";
      continue;
    }
    if (c >= 0x80) continue;
    out += (char)c;
  }
  return out;
}

void say(const String &text) {
  flash = text;
  flashUntil = millis() + 1500;
}

void send(const String &command, const char *confirmation = nullptr) {
  if (!lamp::send(command)) return say("Lampada non collegata");
  if (confirmation) say(confirmation);
}

// --- drawing -----------------------------------------------------------------

void header() {
  canvas.fillRect(0, 0, W, 14, 0x2104);
  canvas.setTextColor(FG);
  canvas.setTextSize(1);
  const lamp::Status st = lamp::status();
  const lamp::State s = lamp::state();
  String left = st == lamp::Status::Ready ? "Lampada: " + plain(s.modeName)
                : st == lamp::Status::NeedPin ? "Abbinamento"
                                              : "Cerco la lampada...";
  canvas.drawString(left, 3, 3);
  const String right = s.time + "  " + String(M5Cardputer.Power.getBatteryLevel()) + "%";
  canvas.drawRightString(right, W - 3, 3);
}

void footer(const char *hint) {
  canvas.setTextColor(millis() < flashUntil ? ACCENT : DIM);
  canvas.drawString(millis() < flashUntil ? flash : String(hint), 3, H - 10);
}

// The lamp's panel, `cell` pixels per LED, at (x, y).
void preview(int x, int y, int cell) {
  uint8_t f[256];
  lamp::frameSnapshot(f);
  canvas.fillRect(x - 1, y - 1, cell * 16 + 2, cell * 16 + 2, 0x18E3);
  for (int i = 0; i < 256; i++) {
    const uint8_t v = f[i] * 17;
    const uint16_t color = v ? canvas.color565(v, v * 3 / 4, v / 3) : 0x0841;  // warm, like the lamp
    canvas.fillRect(x + (i % 16) * cell, y + (i / 16) * cell, cell - 1, cell - 1, color);
  }
}

void drawPin() {
  canvas.setTextColor(FG);
  canvas.drawString("Abbina la lampada", 10, 24);
  canvas.setTextColor(DIM);
  canvas.drawString("Scrivi il PIN della pagina web della", 10, 42);
  canvas.drawString("lampada (sezione Bluetooth), poi Invio.", 10, 54);
  canvas.setTextColor(ACCENT);
  canvas.setTextSize(3);
  canvas.drawString(typed + ((millis() / 400) % 2 ? "_" : " "), 10, 72);
  canvas.setTextSize(1);
  if (lamp::message().length()) {
    canvas.setTextColor(TFT_RED);
    canvas.drawString(plain(lamp::message()), 10, 104);
  }
}

void drawSearching() {
  canvas.setTextColor(FG);
  canvas.drawString(lamp::status() == lamp::Status::Connecting ? "Collegamento e abbinamento..." : "Cerco la lampada...",
                    10, 40);
  canvas.setTextColor(DIM);
  if (lamp::message().length()) canvas.drawString(plain(lamp::message()), 10, 60);
  footer("Esc: impostazioni");
}

void drawMenu() {
  for (int i = 0; i < MENU_COUNT; i++) {
    const int y = 18 + i * 14;
    if (i == menuPos) canvas.fillRoundRect(2, y - 2, 150, 13, 3, ACCENT);
    canvas.setTextColor(i == menuPos ? BG : FG);
    canvas.drawString(MENU[i], 8, y);
  }
  preview(162, 22, 4);
  footer("; . sposta  Invio sceglie");
}

void drawRemote() {
  preview(4, 18, 7);
  const lamp::State &s = lamp::state();
  canvas.setTextColor(FG);
  const int x = 122;
  canvas.drawString(s.game.length() ? plain(s.gameName) : plain(s.modeName), x, 20);
  canvas.setTextColor(s.demo ? DIM : ACCENT);
  canvas.drawString(!s.game.length() ? "(non e' un gioco)" : s.demo ? "Demo (D per giocare)" : "Giochi tu", x, 34);
  canvas.setTextColor(DIM);
  canvas.drawString("; . , /  frecce", x, 54);
  canvas.drawString("Spazio   salta/spara", x, 66);
  canvas.drawString("D  demo si'/no", x, 78);
  canvas.drawString("X  " + plain(s.button), x, 90);
  canvas.drawString("`  indietro", x, 102);
  if (s.demoForced) {
    canvas.setTextColor(ACCENT);
    canvas.drawString("D per giocare a questo", x, 116);
  }
}

void drawList() {
  const int rows = 7;
  const int first = max(0, min(listPos - rows / 2, (int)listItems.size() - rows));
  for (int i = first; i < (int)listItems.size() && i < first + rows; i++) {
    const int y = 18 + (i - first) * 14;
    const lamp::Item &it = listItems[i];
    const lamp::State s = lamp::state();
    const bool current = it.id == s.mode || it.id == s.game;
    if (i == listPos) canvas.fillRoundRect(2, y - 2, W - 4, 13, 3, ACCENT);
    canvas.setTextColor(i == listPos ? BG : current ? ACCENT : FG);
    canvas.drawString(plain(it.name) + (current ? "  <" : ""), 8, y);
  }
  footer("; . sposta  Invio sceglie  ` indietro");
}

void drawEditor(const char *title, bool withIcon) {
  canvas.setTextColor(FG);
  canvas.drawString(title, 6, 20);
  if (withIcon) {
    canvas.setTextColor(ACCENT);
    canvas.drawString(String("< ") + ICON_NAMES[iconPos] + " >", 6, 36);
  }
  canvas.drawRoundRect(4, 52, W - 8, 44, 4, DIM);
  canvas.setTextColor(FG);
  canvas.setTextWrap(true);
  canvas.setCursor(8, 56);
  canvas.print(typed + ((millis() / 400) % 2 ? "_" : " "));
  canvas.setTextWrap(false);
  canvas.setTextColor(DIM);
  canvas.drawString(String(typed.length()) + "/200", W - 50, 100);
  footer(withIcon ? ", / icona  Invio invia  ` indietro" : "Invio: la lampada lo mostra  ` indietro");
}

void drawBrightness() {
  const int b = lamp::state().brightness;
  canvas.setTextColor(FG);
  canvas.drawString("Luminosita'", 10, 24);
  canvas.drawRoundRect(10, 50, W - 20, 20, 4, DIM);
  canvas.fillRoundRect(12, 52, (W - 24) * b / 255, 16, 3, ACCENT);
  canvas.drawString(String(b * 100 / 255) + "%", 10, 80);
  footer(", / cambia  ` indietro");
}

void drawSettings() {
  canvas.setTextColor(FG);
  canvas.drawString("Impostazioni", 10, 22);
  canvas.setTextColor(DIM);
  canvas.drawString("PIN abbinato: " + (lamp::pin() ? String(lamp::pin()) : String("nessuno")), 10, 42);
  canvas.drawString("F: dimentica la lampada e riabbina", 10, 62);
  canvas.drawString("(dopo aver cambiato PIN sulla lampada)", 10, 74);
  footer("` indietro");
}

void draw() {
  canvas.fillSprite(BG);
  header();
  const lamp::Status st = lamp::status();
  if (st == lamp::Status::NeedPin) drawPin();
  else if (st != lamp::Status::Ready && screen != Screen::Settings) drawSearching();
  else {
    switch (screen) {
      case Screen::Menu: drawMenu(); break;
      case Screen::Remote: drawRemote(); break;
      case Screen::List: drawList(); break;
      case Screen::Text: drawEditor("Testo da far scorrere", false); break;
      case Screen::Notify: drawEditor("Notifica", true); break;
      case Screen::Brightness: drawBrightness(); break;
      case Screen::Settings: drawSettings(); break;
    }
  }
  canvas.pushSprite(&M5Cardputer.Display, 0, 0);
}

// --- input -------------------------------------------------------------------

void openList(char kind) {
  listKind = kind;
  listItems.clear();
  if (kind != 'M') listItems.push_back({kind, "auto", kind == 'G' ? "Automatica (a turno, in demo)" : "Automatica"});
  for (const lamp::Item &it : lamp::catalog()) {
    if (it.kind == kind) listItems.push_back(it);
  }
  listPos = 0;
  screen = Screen::List;
}

// Arrows held down repeat (the lamp's games expect a steady stream).
void remoteKeys(const Keyboard_Class::KeysState &k, bool changed) {
  static uint32_t lastRepeat = 0;
  static const struct {
    char key, command;
  } ARROWS[] = {{';', 'U'}, {'.', 'D'}, {',', 'L'}, {'/', 'R'}};
  const uint32_t now = millis();
  for (const auto &a : ARROWS) {
    if (!M5Cardputer.Keyboard.isKeyPressed(a.key)) continue;
    if (changed || now - lastRepeat >= 110) {
      lamp::send(String("k ") + a.command);
      lastRepeat = now;
    }
  }
  if (!changed) return;
  for (char c : k.word) {
    if (c == ' ') lamp::send("k A");
    if (c == 'd' || c == 'D') {
      const lamp::State s = lamp::state();
      if (!s.game.length()) {
        say("Non e' un gioco");
      } else if (s.demoForced) {
        // "Automatica" is always a demo: pin the game showing now, then hand
        // control over, so D still gets you out of the demo.
        send("g " + s.game);
        send("d 0", "Ora giochi tu");
      } else {
        send(s.demo ? "d 0" : "d 1", s.demo ? "Ora giochi tu" : "Demo riattivata");
      }
    }
    if (c == 'x' || c == 'X') send("x");
    if (c == '`') screen = Screen::Menu;
  }
  if (k.enter) lamp::send("k A");
  if (k.esc) screen = Screen::Menu;
}

void menuChoose() {
  switch (menuPos) {
    case 0: screen = Screen::Remote; break;
    case 1: openList('G'); break;
    case 2: openList('M'); break;
    case 3: openList('A'); break;
    case 4: typed = ""; screen = Screen::Text; break;
    case 5: typed = ""; screen = Screen::Notify; break;
    case 6: screen = Screen::Brightness; break;
    case 7: screen = Screen::Settings; break;
  }
}

void editKeys(const Keyboard_Class::KeysState &k) {
  const bool notify = screen == Screen::Notify;
  for (char c : k.word) {
    if (c == '`' && typed.length() == 0) {
      screen = Screen::Menu;
      return;
    }
    if (notify && (c == ',' || c == '/') && k.fn) {
      iconPos = (iconPos + (c == '/' ? 1 : ICON_COUNT - 1)) % ICON_COUNT;
      continue;
    }
    if (typed.length() < 200) typed += c;
  }
  if (notify && (k.left || k.right)) iconPos = (iconPos + (k.right ? 1 : ICON_COUNT - 1)) % ICON_COUNT;
  if (k.del && typed.length()) typed.remove(typed.length() - 1);
  if (k.esc) screen = Screen::Menu;
  if (k.enter && typed.length()) {
    if (notify) send(String("p ") + ICONS[iconPos] + "|" + typed, "Notifica inviata");
    else send("t " + typed, "Testo inviato");
    typed = "";
  }
}

void keys() {
  const bool changed = M5Cardputer.Keyboard.isChange();
  if (!changed && screen != Screen::Remote) return;
  if (changed && !M5Cardputer.Keyboard.isPressed()) return;  // released
  const Keyboard_Class::KeysState k = M5Cardputer.Keyboard.keysState();
  const lamp::Status st = lamp::status();

  if (st == lamp::Status::NeedPin) {
    if (!changed) return;
    for (char c : k.word) {
      if (c >= '0' && c <= '9' && typed.length() < 6) typed += c;
    }
    if (k.del && typed.length()) typed.remove(typed.length() - 1);
    if (k.enter && typed.length() == 6) {
      lamp::setPin(typed.toInt());
      typed = "";
    }
    return;
  }
  if (st != lamp::Status::Ready) {
    if (changed && (k.esc || std::find(k.word.begin(), k.word.end(), '`') != k.word.end())) screen = Screen::Settings;
    if (screen != Screen::Settings) return;
  }
  if (screen == Screen::Remote) return remoteKeys(k, changed);
  if (screen == Screen::Text || screen == Screen::Notify) return editKeys(k);

  auto has = [&k](char c) { return std::find(k.word.begin(), k.word.end(), c) != k.word.end(); };
  const bool up = has(';') || k.up, down = has('.') || k.down, left = has(',') || k.left, right = has('/') || k.right;
  const bool back = has('`') || k.esc;
  switch (screen) {
    case Screen::Menu:
      if (up) menuPos = (menuPos + MENU_COUNT - 1) % MENU_COUNT;
      if (down) menuPos = (menuPos + 1) % MENU_COUNT;
      if (k.enter) menuChoose();
      break;
    case Screen::List:
      if (up && listPos > 0) listPos--;
      if (down && listPos + 1 < (int)listItems.size()) listPos++;
      if (k.enter && !listItems.empty()) {
        const lamp::Item &it = listItems[listPos];
        send(String(listKind == 'M' ? "m " : listKind == 'G' ? "g " : "a ") + it.id, "Fatto");
        if (listKind == 'G') screen = Screen::Remote;  // straight to the controls
      }
      if (back) screen = Screen::Menu;
      break;
    case Screen::Brightness: {
      const int b = lamp::state().brightness;
      if (left || right) send("b " + String(constrain(b + (right ? 16 : -16), 1, 255)));
      if (back || k.enter) screen = Screen::Menu;
      break;
    }
    case Screen::Settings:
      if (has('f') || has('F')) {
        lamp::forget();
        say("Lampada dimenticata");
        screen = Screen::Menu;
      }
      if (back) screen = Screen::Menu;
      break;
    default:
      break;
  }
}

}  // namespace

// The keyboard and the screen: their own task on core 1. It is the only
// task that touches the display and the I2C bus (keyboard, battery), so
// there is never contention there, and it redraws at a steady ~30 fps
// whatever the Bluetooth is doing - which is what stops the flicker.
void uiTask(void *) {
  for (;;) {
    M5Cardputer.update();
    keys();
    draw();
    vTaskDelay(pdMS_TO_TICKS(33));
  }
}

// Bluetooth on core 0, next to the BLE controller: its scan blocks for a
// few seconds at a time, but that can no longer freeze the screen.
void bleTask(void *) {
  for (;;) {
    lamp::loop();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(120);
  // The render buffer (240x135x2 = ~65 KB): explicitly in internal SRAM -
  // this board (ESP32-S3FN8) has no PSRAM. A single full-screen canvas
  // pushed in one SPI burst is what avoids tearing.
  canvas.setPsram(false);
  if (!canvas.createSprite(W, H)) {
    M5Cardputer.Display.println("Errore: memoria schermo");
  }
  canvas.setFont(&fonts::Font0);
  canvas.setTextSize(1);
  lamp::begin();  // creates the data mutex before either task runs
  xTaskCreatePinnedToCore(uiTask, "ui", 8192, nullptr, 2, nullptr, 1);
  xTaskCreatePinnedToCore(bleTask, "ble", 8192, nullptr, 1, nullptr, 0);
}

void loop() { vTaskDelay(pdMS_TO_TICKS(1000)); }
