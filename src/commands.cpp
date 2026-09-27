#include "commands.h"

#include "animation.h"
#include "modes.h"
#include "modes/notify_mode.h"
#include "remote_protocol.h"
#include "settings.h"

// "d 1 dino" -> ("1", "dino"); "d 1" -> ("1", "")
static void splitArgs(const String &arg, String &first, String &second) {
  const int space = arg.indexOf(' ');
  first = space < 0 ? arg : arg.substring(0, space);
  second = space < 0 ? String() : arg.substring(space + 1);
}

const char *runCommand(const String &command) {
  if (!command.length()) return "Comando vuoto";
  const char op = command[0];
  const String arg = command.length() > 2 && command[1] == ' ' ? command.substring(2) : String();
  switch (op) {
    case 'k':
      if (arg.length() != 1 || !strchr(REMOTE_KEYS, arg[0])) return "Tasto sconosciuto";
      currentMode()->input(arg[0]);
      return nullptr;
    case 'm':
      if (!setMode(arg)) return "Modalità sconosciuta";
      saveSettings();
      return nullptr;
    case 'g':
    case 'a': {
      const Animation *a = findAnimation(arg);
      if (arg != "auto" && !(a && a->isGame() == (op == 'g'))) return op == 'g' ? "Gioco sconosciuto" : "Animazione sconosciuta";
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
      if (!(a && a->isGame())) return "Gioco sconosciuto";
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
      if (!arg.length()) return "Testo vuoto";
      settings.text = arg.substring(0, 200);
      setMode("text");
      saveSettings();
      return nullptr;
    case 'p': {
      const int bar = arg.indexOf('|');
      if (!NotifyMode::push(bar >= 0 ? arg.substring(bar + 1) : arg, bar >= 0 ? arg.substring(0, bar) : String())) {
        return "Serve un testo o un'icona conosciuta";
      }
      return nullptr;
    }
    case 's': {
      String level, id;
      splitArgs(arg, level, id);
      if (!id.length()) id = currentMode()->id();
      if (!validModeId(id)) return "Modalità sconosciuta";
      setSpeedLevel(id.c_str(), level.toInt());
      saveSettings();
      return nullptr;
    }
  }
  return "Comando sconosciuto";
}
