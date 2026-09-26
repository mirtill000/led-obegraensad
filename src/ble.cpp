// Bluetooth LE remote control. One GATT service; every characteristic needs
// an encrypted, authenticated link: the remote pairs once with the PIN
// shown on the web page (static passkey, bonding, MITM protection) and is
// remembered after that.
//
//   command (write)       one UTF-8 command per write, e.g. "k L":
//     k <L|R|U|D|A>       game key          m <mode id>     show a mode
//     g <game id|auto>    play a game       a <anim id|auto> an animation
//     d <0|1>             demo off/on       x               the mode's button
//     n                   next mode         b <1-255>       brightness
//     t <text>            show this text    p <icon>|<text> notification
//     s <1-9>             speed of the mode being shown
//   state (read, notify)  {"m":mode,"mn":name,"x":button name,"g":game,
//                          "gn":game name,"d":demo,"f":demo forced,"b":brightness,
//                          "t":"HH:MM"} - sent when it changes
//   frame (read, notify)  the panel as seen: 256 levels 0-15, two pixels per
//                          byte (high nibble first), row by row - at most
//                          every 150 ms, when it changes
//   catalog (read)        lines "M|G|A <tab> id <tab> name": modes, games,
//                          animations
//
// The BLE stack runs in its own task: commands are queued there and run
// here, in loop(), like the web page's.
#include "ble.h"

#include <BLEDevice.h>
#include <BLESecurity.h>
#include <BLEServer.h>
#include <freertos/queue.h>
#include <host/ble_store.h>

#include "animation.h"
#include "display.h"
#include "modes.h"
#include "modes/ambient_mode.h"
#include "modes/notify_mode.h"
#include "settings.h"
#include "timekeeping.h"

namespace {

struct Command {
  char text[244];
};

QueueHandle_t commands = nullptr;
BLEServer *server = nullptr;
BLECharacteristic *stateChar = nullptr, *frameChar = nullptr, *catalogChar = nullptr;
bool running = false;

class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
    Command cmd = {};
    const String value = c->getValue();
    strncpy(cmd.text, value.c_str(), sizeof(cmd.text) - 1);
    xQueueSend(commands, &cmd, 0);  // full queue: dropped (keys come again)
  }
};

String catalog() {
  String out;
  for (uint8_t i = 0; i < MODE_COUNT; i++) {
    if (!MODES[i]->hidden()) out += String("M\t") + MODES[i]->id() + "\t" + MODES[i]->name() + "\n";
  }
  for (uint8_t i = 0; i < ANIMATION_COUNT; i++) {
    out += String(ANIMATIONS[i]->isGame() ? "G\t" : "A\t") + ANIMATIONS[i]->id() + "\t" + ANIMATIONS[i]->name() + "\n";
  }
  return out;
}

String quoted(const char *s) {
  String out = "\"";
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') out += '\\';
    out += *s;
  }
  return out + "\"";
}

String stateJson() {
  Mode *m = currentMode();
  String j = "{\"m\":" + quoted(m->id()) + ",\"mn\":" + quoted(m->name());
  j += ",\"x\":" + quoted(m->actionName() ? m->actionName() : "");
  const char *game = m->gameId();
  if (game) {
    const bool player = strcmp(m->id(), "ambient") == 0 || strcmp(m->id(), "games") == 0;
    const AmbientMode *a = player ? static_cast<const AmbientMode *>(m) : nullptr;
    const bool forced = a && a->demoForced();
    j += ",\"g\":" + quoted(game) + ",\"gn\":" + quoted(a && a->playing() ? a->playing()->name() : m->name());
    j += String(",\"d\":") + (forced || demoMode(game) ? 1 : 0) + ",\"f\":" + (forced ? 1 : 0);
  }
  j += ",\"b\":" + String(settings.brightness);
  struct tm t;
  if (localTime(t)) {
    char hhmm[6];
    strftime(hhmm, sizeof(hhmm), "%H:%M", &t);
    j += ",\"t\":\"" + String(hhmm) + "\"";
  }
  return j + "}";
}

void run(const char *text) {
  const char op = text[0];
  const String arg = text[0] && text[1] == ' ' ? String(text + 2) : String();
  switch (op) {
    case 'k':
      if (arg.length() == 1 && strchr("LRUDA", arg[0])) currentMode()->input(arg[0]);
      return;
    case 'm':
      if (setMode(arg)) saveSettings();
      return;
    case 'g':
    case 'a': {
      const Animation *a = findAnimation(arg);
      if (arg != "auto" && !(a && a->isGame() == (op == 'g'))) return;
      (op == 'g' ? settings.game : settings.ambient) = arg;
      setMode(op == 'g' ? "games" : "ambient");
      restartMode();
      saveSettings();
      return;
    }
    case 'd':
      if (currentMode()->gameId()) {
        setDemoMode(currentMode()->gameId(), arg == "1");
        saveSettings();
      }
      return;
    case 'x':
      currentMode()->action();
      return;
    case 'n':
      nextMode();
      saveSettings();
      return;
    case 'b':
      settings.brightness = constrain(arg.toInt(), 1, 255);
      saveSettings();
      refreshModes();
      return;
    case 't':
      if (!arg.length()) return;
      settings.text = arg.substring(0, 200);
      setMode("text");
      saveSettings();
      return;
    case 'p': {
      const int bar = arg.indexOf('|');
      NotifyMode::push(bar >= 0 ? arg.substring(bar + 1) : arg, bar >= 0 ? arg.substring(0, bar) : String());
      return;
    }
    case 's':
      setSpeedLevel(currentMode()->id(), arg.toInt());
      saveSettings();
      return;
  }
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *) override { Serial.println("BLE: remote connected"); }
  void onDisconnect(BLEServer *) override { Serial.println("BLE: remote disconnected"); }
};

}  // namespace

void bleBegin() {
  if (!settings.bleOn) return;
  commands = xQueueCreate(8, sizeof(Command));
  BLEDevice::init(HOSTNAME);
  BLEDevice::setMTU(247);

  BLESecurity *security = new BLESecurity();
  security->setPassKey(true, settings.blePin);
  security->setCapability(ESP_IO_CAP_OUT);  // "displays" the PIN: the remote types it
  security->setAuthenticationMode(true, true, true);  // bonding, MITM, secure connections

  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  server->advertiseOnDisconnect(true);
  BLEService *service = server->createService(BLE_SERVICE_UUID);
  const uint32_t readSecure = BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_READ_AUTHEN;

  BLECharacteristic *command = service->createCharacteristic(
      BLE_COMMAND_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR |
                            BLECharacteristic::PROPERTY_WRITE_AUTHEN);
  command->setCallbacks(new CommandCallbacks());
  stateChar = service->createCharacteristic(BLE_STATE_UUID, readSecure | BLECharacteristic::PROPERTY_NOTIFY);
  frameChar = service->createCharacteristic(BLE_FRAME_UUID, readSecure | BLECharacteristic::PROPERTY_NOTIFY);
  catalogChar = service->createCharacteristic(BLE_CATALOG_UUID, readSecure);
  catalogChar->setValue(catalog());
  service->start();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(BLE_SERVICE_UUID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();
  running = true;
  Serial.printf("BLE: advertising as %s, PIN %06lu\n", HOSTNAME, (unsigned long)settings.blePin);
}

bool bleConnected() { return running && server->getConnectedCount() > 0; }

void bleForgetRemotes() {
  if (running) ble_store_clear();
  settings.blePin = 100000 + esp_random() % 900000;
  saveSettings();
}

void bleLoop() {
  if (!running) return;
  Command cmd;
  while (xQueueReceive(commands, &cmd, 0) == pdTRUE) run(cmd.text);
  if (!bleConnected()) return;

  const uint32_t now = millis();
  static uint32_t lastFrame = 0, lastState = 0, frameHash = 0, stateHash = 0;
  if (now - lastFrame >= 150) {
    lastFrame = now;
    uint8_t frame[TOTAL_PIXELS / 2];
    uint32_t h = 2166136261u;
    for (int i = 0; i < TOTAL_PIXELS / 2; i++) {
      const int x = (2 * i) % COLS, y = (2 * i) / COLS;
      frame[i] = (display.shownLevel(x, y) >> 4) << 4 | display.shownLevel(x + 1, y) >> 4;
      h = (h ^ frame[i]) * 16777619u;
    }
    if (h != frameHash) {
      frameHash = h;
      frameChar->setValue(frame, sizeof(frame));
      frameChar->notify();
    }
  }
  if (now - lastState >= 500) {
    lastState = now;
    const String json = stateJson();
    uint32_t h = 2166136261u;
    for (unsigned i = 0; i < json.length(); i++) h = (h ^ (uint8_t)json[i]) * 16777619u;
    if (h != stateHash) {
      stateHash = h;
      stateChar->setValue(json);
      stateChar->notify();
    }
  }
}
