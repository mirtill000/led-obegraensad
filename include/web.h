#pragma once

// Control page on port 80: pick a mode, edit the scrolling text, set
// brightness and speed. Call webBegin() once WiFi is up, webLoop() from
// loop().
void webBegin();
void webLoop();

// Pages connected for live updates (Server-Sent Events on port 81).
int liveClients();
