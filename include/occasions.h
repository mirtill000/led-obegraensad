#pragma once

#include <Arduino.h>
#include <time.h>

// Days that deserve their own animation, with settings.occasions on: on
// such a day the lamp shows it for the first minute of every hour (as an
// interlude: then it goes back to what it was doing), and it is always
// available among the Animazioni ("Ricorrenze").
//
//   31 Dec from 22:00 and 1 Jan    fireworks     Capodanno
//   24-26 Dec                      Christmas tree Natale
//   the rest of December           snow          Dicembre
//   14 Feb                         hearts        San Valentino
//   31 Oct                         pumpkin       Halloween
//   a birthday in the calendar     cake          Compleanno (wins over the rest)
//
// Birthdays: today's events of the iCal calendar (Dal web) whose title
// says "compleanno" or "birthday" - by day and month, so the yearly
// repeating birthdays of Google Calendar count too.
struct Occasion {
  const char *animation = nullptr;  // animation id, nullptr = an ordinary day
  String name;                      // "Halloween", "Compleanno di Anna"
};

Occasion occasionOn(const struct tm &local, const String &birthday);
Occasion occasionNow();

// Whether a seasonal animation belongs to the days around `local` - it is
// only offered then (Animation::available()):
//   snow       December - February
//   xmastree   1 December - 6 January
//   fireworks  26 December - 6 January
//   hearts     1 - 15 February
//   pumpkin    15 October - 2 November
//   cake       a birthday in the calendar today
// Any other id: always.
bool inSeason(const char *animationId, const struct tm &local, bool birthdayToday);
// "Buon compleanno, Anna!" from a calendar title ("Compleanno di Anna",
// "Anna's birthday", ...).
String birthdayGreeting(const String &title);
