#pragma once

// Background task (core 0) for everything fetched from the internet -
// weather, web info - so slow downloads never stall the display or the
// web page. Start it once WiFi is up.
void startNetTask();
