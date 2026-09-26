#include "sysinfo.h"

#include <LittleFS.h>
#include <WiFi.h>

namespace sysinfo {

String files(int max) {
  String out;
  File root = LittleFS.open("/");
  int n = 0;
  for (File f = root.openNextFile(); f && n < max; f = root.openNextFile(), n++) {
    String name = f.name();
    if (name.startsWith("/")) name.remove(0, 1);
    const int dot = name.lastIndexOf('.');
    if (dot > 0) name = name.substring(0, dot);  // "quotes.txt" -> "quotes"
    name.toUpperCase();
    if (out.length()) out += '\n';
    out += name + (f.isDirectory() ? "/" : "");
  }
  return out.length() ? out : String("-");
}

String ip() { return WiFi.localIP().toString(); }

String freeFlash() { return String((unsigned)((LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024)) + "K"; }

String freeHeap() { return String(ESP.getFreeHeap() / 1024) + "K"; }

String chipTemp() { return String((int)lroundf(temperatureRead())) + "C"; }

}  // namespace sysinfo
