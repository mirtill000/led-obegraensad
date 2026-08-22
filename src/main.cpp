#include <Arduino.h>

#include "constants.h"
#include "display.h"

void setup() {
  Serial.begin(115200);
  display.begin();
}

void loop() {
  display.scrollTextOnce(MESSAGE, SCROLL_DELAY_MS);
}
