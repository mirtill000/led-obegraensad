#include "commands.h"

#include "animation.h"
#include "catalog.h"
#include "modes.h"
#include "modes/board.h"
#include "modes/life_mode.h"
#include "modes/notify_mode.h"
#include "remote_protocol.h"
#include "settings.h"
#include "texts.h"
#include "timekeeping.h"
#include "weather.h"
#include "webinfo.h"
#include "display.h"

// "d 1 dino" -> ("1", "dino"); "d 1" -> ("1", "")
static void splitArgs(const String &arg, String &first, String &second) {
  const int space = arg.indexOf(' ');
  first = space < 0 ? arg : arg.substring(0, space);
  second = space < 0 ? String() : arg.substring(space + 1);
}

// What a changed setting touches (see SettingEffect), once for a whole
// request.
void applySettingEffects(const SettingDef *const *changed, int count) {
  uint16_t fx = 0;
  for (int i = 0; i < count; i++) fx |= changed[i]->effects;
  if (fx & FX_FONT) Display::setScrollFont(fontForSettings());
  if (fx & FX_ROTATION) {
    display.setRotation(rotationForSettings());
    Display::setVerticalText(settings.vertical);
  }
  if (fx & FX_TRANSITION) display.setTransition(transitionForSettings());
  if (fx & FX_TIMEZONE) applyTimezone();
  if (fx & FX_WEATHER) requestWeatherUpdate();
  if (fx & FX_WEB) requestWebInfoUpdate();
  if (fx & FX_PLAYLIST) restartPlaylist();
  bool restarted = false;
  for (int i = 0; i < count; i++) {
    const SettingDef &d = *changed[i];
    if (d.effects & FX_SHOW) {
      setMode(d.mode);
      restarted = true;
    } else if ((d.effects & FX_RESTART) && !restarted && (!d.mode || strcmp(currentMode()->id(), d.mode) == 0)) {
      restartMode();
      restarted = true;
    }
  }
  if (fx & (FX_MODES | FX_PLAYLIST)) refreshModes();
}

const char *runCommand(const String &command) {
  if (!command.length()) return txt::EMPTY_COMMAND;
  const char op = command[0];
  const String arg = command.length() > 2 && command[1] == ' ' ? command.substring(2) : String();
  switch (op) {
    case 'k':
      if (arg.length() != 1 || !(strchr(REMOTE_KEYS, arg[0]) || strchr(REMOTE_KEYS_P2, arg[0]))) return txt::UNKNOWN_KEY;
      currentMode()->input(arg[0]);
      return nullptr;
    case 'v':
      if (!showScene(arg)) return validScene(arg) ? txt::OUT_OF_SEASON : txt::UNKNOWN_SCENE;
      saveSettings();
      return nullptr;
    case 'm':
      if (!setMode(arg)) return txt::UNKNOWN_MODE;
      saveSettings();
      return nullptr;
    case 'g':
    case 'a': {
      const Animation *a = findAnimation(arg);
      if (arg != "auto" && !(a && a->isGame() == (op == 'g'))) return op == 'g' ? txt::UNKNOWN_GAME : txt::UNKNOWN_ANIMATION;
      (op == 'g' ? settings.game : settings.ambient) = arg;
      setMode(op == 'g' ? "games" : "ambient");
      restartMode();
      saveSettings();
      return nullptr;
    }
    case 'd': {
      String on, id;
      splitArgs(arg, on, id);
      if (!id.length() && currentMode()->gameId()) id = currentMode()->gameId();
      const Animation *a = findAnimation(id);
      if (!(a && a->isGame())) return txt::UNKNOWN_GAME;
      setDemoMode(id.c_str(), on == "1");
      saveSettings();
      return nullptr;
    }
    case 'x':
      currentMode()->action();
      return nullptr;
    case 'n':
      nextMode();
      saveSettings();
      return nullptr;
    case 'b':
      settings.brightness = constrain(arg.toInt(), 1, 255);
      saveSettings();
      refreshModes();
      return nullptr;
    case 't':
      if (!arg.length()) return txt::EMPTY_TEXT;
      settings.text = arg.substring(0, 200);
      setMode("text");
      saveSettings();
      return nullptr;
    case 'p': {
      const int bar = arg.indexOf('|');
      if (!NotifyMode::push(bar >= 0 ? arg.substring(bar + 1) : arg, bar >= 0 ? arg.substring(0, bar) : String())) {
        return txt::NEED_TEXT_OR_ICON;
      }
      return nullptr;
    }
    case 'o': {
      // A setting, by its name in SETTING_DEFS: "o brightness 120".
      String name, value;
      splitArgs(arg, name, value);
      const SettingDef *d = findSetting(name);
      if (!d || !(d->flags & SET_WEB)) return txt::UNKNOWN_SETTING;
      if (d->effects & FX_REBOOT) return txt::SETTING_PAGE_ONLY;
      if (const char *reason = setSetting(*d, value)) return reason;
      saveSettings();
      applySettingEffects(&d, 1);
      return nullptr;
    }
    case 'w': {
      // The Gioco della vita's drawing board: paint, clear, set it going.
      if (arg == "l") {
        LifeMode::fromBoard();
      } else if (arg == "c") {
        board::clear();
      } else {
        int x, y, level;
        if (sscanf(arg.c_str(), "%d %d %d", &x, &y, &level) != 3 || x < 0 || x >= COLS || y < 0 || y >= ROWS) {
          return txt::BAD_PIXEL;
        }
        board::paint(x, y, constrain(level, 0, 255));
      }
      if (strcmp(currentMode()->id(), "life") != 0) {
        setMode("life");
        saveSettings();
      }
      return nullptr;
    }
    case 's': {
      String level, id;
      splitArgs(arg, level, id);
      if (!id.length()) id = currentMode()->id();
      if (!validModeId(id)) return txt::UNKNOWN_MODE;
      setSpeedLevel(id.c_str(), level.toInt());
      saveSettings();
      return nullptr;
    }
  }
  return txt::UNKNOWN_COMMAND;
}
