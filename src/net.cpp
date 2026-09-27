#include "net.h"

#include <Arduino.h>

#include "webinfo.h"
#include "weather.h"
#include "world.h"

static void netTask(void *) {
  for (;;) {
    weatherTick();
    webInfoTick();
    worldTick();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void startNetTask() {
  // HTTPS needs a roomy stack.
  xTaskCreatePinnedToCore(netTask, "net", 16384, nullptr, 1, nullptr, 0);
}
