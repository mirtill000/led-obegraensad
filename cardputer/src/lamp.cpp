#include "lamp.h"

#include <BLEDevice.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <string.h>
#include <BLESecurity.h>
#include <Preferences.h>
#include <host/ble_store.h>

#include "protocol.h"

namespace lamp {

namespace {

Status current = Status::NeedPin;
String problem;
uint32_t pairingPin = 0;
State lampState;
uint8_t pixels[256];
volatile uint32_t pixelsVersion = 0;
std::vector<Item> items;

BLEClient *client = nullptr;
BLERemoteCharacteristic *commandChar = nullptr, *stateChar = nullptr, *frameChar = nullptr;
BLEAdvertisedDevice *found = nullptr;

// User actions (send / setPin / forget) come from the UI task; they are
// queued here and carried out on the BLE task, so the connection is only
// ever touched from one task.
enum class Cmd : uint8_t { Send, SetPin, Forget };
struct CmdMsg {
  Cmd type;
  uint32_t pin;
  char text[224];
};
QueueHandle_t cmdQueue = nullptr;
volatile bool authenticated = false, authFailed = false, dropped = false;

// Notifications arrive on the BLE task: they are copied here and parsed in
// loop().
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

// State, catalog and picture are written by the BLE task and read by the UI
// task's draw(): a mutex so a read never sees a half-written String or a
// torn frame. Created in begin(), before either task touches them.
SemaphoreHandle_t dataMutex = nullptr;
struct Guard {
  Guard() { if (dataMutex) xSemaphoreTake(dataMutex, portMAX_DELAY); }
  ~Guard() { if (dataMutex) xSemaphoreGive(dataMutex); }
};
String pendingState;
volatile bool statePending = false;

// Value of "key" in the lamp's flat JSON (strings and numbers).
String field(const String &json, const char *key) {
  const String k = String("\"") + key + "\":";
  int i = json.indexOf(k);
  if (i < 0) return "";
  i += k.length();
  if (json[i] != '"') {
    int end = i;
    while (end < (int)json.length() && json[end] != ',' && json[end] != '}') end++;
    return json.substring(i, end);
  }
  String out;
  for (i++; i < (int)json.length() && json[i] != '"'; i++) {
    if (json[i] == '\\' && i + 1 < (int)json.length()) i++;
    out += json[i];
  }
  return out;
}

void parseState(const String &json) {
  Guard g;
  lampState.mode = field(json, "m");
  lampState.modeName = field(json, "mn");
  lampState.button = field(json, "x");
  lampState.game = field(json, "g");
  lampState.gameName = field(json, "gn");
  lampState.demo = field(json, "d") != "0";
  lampState.demoForced = field(json, "f") == "1";
  lampState.time = field(json, "t");
  const String b = field(json, "b");
  if (b.length()) lampState.brightness = b.toInt();
}

void parseCatalog(const String &text) {
  Guard g;
  items.clear();
  int start = 0;
  while (start < (int)text.length()) {
    int end = text.indexOf('\n', start);
    if (end < 0) end = text.length();
    const String line = text.substring(start, end);
    const int a = line.indexOf('\t'), b = line.indexOf('\t', a + 1);
    if (a == 1 && b > a) items.push_back({line[0], line.substring(a + 1, b), line.substring(b + 1)});
    start = end + 1;
  }
}

void onState(BLERemoteCharacteristic *, uint8_t *data, size_t length, bool) {
  String s;
  s.concat((const char *)data, length);
  portENTER_CRITICAL(&lock);
  pendingState = s;
  statePending = true;
  portEXIT_CRITICAL(&lock);
}

void onFrame(BLERemoteCharacteristic *, uint8_t *data, size_t length, bool) {
  if (length < 128) return;
  Guard g;
  for (int i = 0; i < 128; i++) {
    pixels[2 * i] = data[i] >> 4;
    pixels[2 * i + 1] = data[i] & 15;
  }
  pixelsVersion = pixelsVersion + 1;
}

class ClientCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient *) override {}
  void onDisconnect(BLEClient *) override { dropped = true; }
};

class SecurityCallbacks : public BLESecurityCallbacks {
  void onAuthenticationComplete(ble_gap_conn_desc *desc) override {
    authenticated = desc->sec_state.encrypted && desc->sec_state.authenticated;
    authFailed = !authenticated;
  }
};

void setSecurity() {
  BLESecurity *security = new BLESecurity();
  security->setPassKey(true, pairingPin);            // what we "type" when the lamp asks
  security->setCapability(ESP_IO_CAP_IN);            // keyboard: enters the lamp's PIN
  security->setAuthenticationMode(true, true, true);  // bonding, MITM, secure connections
}

bool search() {
  BLEScan *scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);
  BLEScanResults *results = scan->start(3, false);
  delete found;
  found = nullptr;
  for (int i = 0; results && i < results->getCount(); i++) {
    BLEAdvertisedDevice d = results->getDevice(i);
    if (d.haveServiceUUID() && d.isAdvertisingService(BLEUUID(LAMP_SERVICE_UUID))) {
      found = new BLEAdvertisedDevice(d);
      break;
    }
  }
  scan->clearResults();
  return found != nullptr;
}

bool connect() {
  authenticated = authFailed = dropped = false;
  if (!client) {
    client = BLEDevice::createClient();
    client->setClientCallbacks(new ClientCallbacks());
  }
  if (!client->connect(found)) {
    problem = "Connessione non riuscita";
    return false;
  }
  client->setMTU(247);
  BLERemoteService *service = client->getService(LAMP_SERVICE_UUID);
  if (!service) {
    problem = "Servizio non trovato";
    client->disconnect();
    return false;
  }
  commandChar = service->getCharacteristic(LAMP_COMMAND_UUID);
  stateChar = service->getCharacteristic(LAMP_STATE_UUID);
  frameChar = service->getCharacteristic(LAMP_FRAME_UUID);
  BLERemoteCharacteristic *catalogChar = service->getCharacteristic(LAMP_CATALOG_UUID);
  if (!commandChar || !stateChar || !frameChar || !catalogChar) {
    problem = "Lampada non compatibile";
    client->disconnect();
    return false;
  }
  // The first protected read starts pairing (with the PIN) if needed.
  const String json = stateChar->readValue();
  if (authFailed || json.length() == 0) {
    problem = "PIN sbagliato?";
    authFailed = true;
    client->disconnect();
    return false;
  }
  parseState(json);
  parseCatalog(catalogChar->readValue());
  stateChar->registerForNotify(onState);
  frameChar->registerForNotify(onFrame);
  const String first = frameChar->readValue();
  if (first.length() >= 128) onFrame(nullptr, (uint8_t *)first.c_str(), first.length(), false);
  problem = "";
  return true;
}

}  // namespace

void begin() {
  Preferences prefs;
  prefs.begin("remote", true);
  pairingPin = prefs.getUInt("pin", 0);
  prefs.end();
  dataMutex = xSemaphoreCreateMutex();
  cmdQueue = xQueueCreate(8, sizeof(CmdMsg));
  BLEDevice::init("Cardputer telecomando");
  BLEDevice::setSecurityCallbacks(new SecurityCallbacks());
  setSecurity();
  current = pairingPin ? Status::Searching : Status::NeedPin;
}

void setPinImpl(uint32_t p);
void forgetImpl();
void sendImpl(const String &command);

void loop() {
  CmdMsg m;
  while (cmdQueue && xQueueReceive(cmdQueue, &m, 0) == pdTRUE) {
    if (m.type == Cmd::Send) sendImpl(m.text);
    else if (m.type == Cmd::SetPin) setPinImpl(m.pin);
    else forgetImpl();
  }
  if (statePending) {
    portENTER_CRITICAL(&lock);
    const String s = pendingState;
    statePending = false;
    portEXIT_CRITICAL(&lock);
    parseState(s);
  }
  switch (current) {
    case Status::NeedPin:
      return;
    case Status::Searching:
      if (search()) current = Status::Connecting;
      else problem = "Lampada non trovata: e' accesa, col Bluetooth attivo?";
      return;
    case Status::Connecting:
      if (connect()) {
        current = Status::Ready;
      } else if (authFailed) {
        forgetImpl();
        problem = "PIN sbagliato: controlla quello sulla pagina della lampada";
      } else {
        current = Status::Searching;
      }
      return;
    case Status::Ready:
      if (dropped || !client->isConnected()) {
        problem = "Connessione persa, la cerco di nuovo";
        current = Status::Searching;
      }
      return;
  }
}

Status status() { return current; }
const String &message() { return problem; }
uint32_t pin() { return pairingPin; }

void setPinImpl(uint32_t p) {
  pairingPin = p;
  Preferences prefs;
  prefs.begin("remote", false);
  prefs.putUInt("pin", p);
  prefs.end();
  setSecurity();
  current = Status::Searching;
  problem = "";
}

void forgetImpl() {
  if (client && client->isConnected()) client->disconnect();
  ble_store_clear();  // the pairing keys
  Preferences prefs;
  prefs.begin("remote", false);
  prefs.remove("pin");
  prefs.end();
  pairingPin = 0;
  current = Status::NeedPin;
}

void sendImpl(const String &command) {
  if (current == Status::Ready && commandChar) commandChar->writeValue(command, false);
}

// Public entry points, called from the UI task: they queue the work for
// the BLE task rather than touch the connection directly.
void setPin(uint32_t p) {
  CmdMsg m = {Cmd::SetPin, p, {}};
  if (cmdQueue) xQueueSend(cmdQueue, &m, 0);
}

void forget() {
  CmdMsg m = {Cmd::Forget, 0, {}};
  if (cmdQueue) xQueueSend(cmdQueue, &m, 0);
}

bool send(const String &command) {
  if (current != Status::Ready) return false;
  CmdMsg m = {Cmd::Send, 0, {}};
  strncpy(m.text, command.c_str(), sizeof(m.text) - 1);
  return cmdQueue && xQueueSend(cmdQueue, &m, 0) == pdTRUE;
}

State state() {
  Guard g;
  return lampState;
}
uint32_t frameSnapshot(uint8_t out[256]) {
  Guard g;
  memcpy(out, pixels, 256);
  return pixelsVersion;
}
uint32_t frameVersion() { return pixelsVersion; }
std::vector<Item> catalog() {
  Guard g;
  return items;
}

}  // namespace lamp
