#pragma once

#include <Arduino.h>

// Facts about the lamp for the Terminal animation, so its commands print
// real answers (all upper case, short).
namespace sysinfo {
String files(int max);  // "ls": names in the flash filesystem, one per line ('\n')
String ip();            // "ip": 192.168.1.50
String freeFlash();     // "df": free space in the filesystem, e.g. 840K
String freeHeap();      // "top": free memory, e.g. 182K
String chipTemp();      // "top": chip temperature, e.g. 47C
}  // namespace sysinfo
