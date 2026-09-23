#pragma once

#include <Arduino.h>

// Things fetched from the web for the "Dal web" mode, each a line of text:
//  - the word of the day (from a built-in list: no reliable free Italian API)
//  - "on this day" events from Italian Wikipedia (once a day)
//  - the next event of an iCal calendar (settings.icalUrl, every 15 min)
// Fetched by the network task; read from anywhere with the copies below.
struct WebInfo {
  String word;            // "Parola del giorno: ..."; empty if switched off
  String history[12];     // "Accadde oggi, 1846: ..."
  uint8_t historyCount = 0;
  String event;           // "Domani 9:30 Dentista"; empty if none/off
  String historyStatus;   // for the page: "12 eventi" / error
  String calendarStatus;
};

WebInfo webInfoNow();
// Refetch now (after the settings changed).
void requestWebInfoUpdate();
// Called by the network task.
void webInfoTick();

// Parsers, exposed for tests: feed a whole response.
uint8_t parseOnThisDay(const String &json, String *out, uint8_t max);
// Next event at or after `now` (epoch seconds) in an iCal file; false if none.
bool parseCalendar(const String &ics, time_t now, String &summary, time_t &start, bool &allDay);
String describeEvent(const String &summary, time_t start, bool allDay, time_t now);
