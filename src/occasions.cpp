#include "occasions.h"

#include "timekeeping.h"
#include "webinfo.h"

String birthdayGreeting(const String &title) {
  String name = title;
  name.trim();
  String lower = name;
  lower.toLowerCase();
  int cut;
  if (lower.startsWith("compleanno di ")) {
    name = name.substring(14);
  } else if (lower.startsWith("compleanno ")) {
    name = name.substring(11);
  } else if ((cut = lower.indexOf("'s birthday")) > 0) {
    name = name.substring(0, cut);
  } else if ((cut = lower.indexOf(" compleanno")) > 0) {
    name = name.substring(0, cut);
  } else if (lower.startsWith("birthday ")) {
    name = name.substring(9);
  } else if (lower == "compleanno" || lower == "birthday") {
    name = "";
  }
  name.trim();
  return name.length() ? "Buon compleanno, " + name + "!" : String("Buon compleanno!");
}

Occasion occasionOn(const struct tm &t, const String &birthday) {
  Occasion o;
  const int month = t.tm_mon + 1, day = t.tm_mday;
  if (birthday.length()) {
    o.animation = "cake";
    o.name = birthday;
  } else if ((month == 12 && day == 31 && t.tm_hour >= 22) || (month == 1 && day == 1)) {
    o.animation = "fireworks";
    o.name = "Capodanno";
  } else if (month == 12 && day >= 24 && day <= 26) {
    o.animation = "xmastree";
    o.name = "Natale";
  } else if (month == 12) {
    o.animation = "snow";
    o.name = "Dicembre";
  } else if (month == 2 && day == 14) {
    o.animation = "hearts";
    o.name = "San Valentino";
  } else if (month == 10 && day == 31) {
    o.animation = "pumpkin";
    o.name = "Halloween";
  }
  return o;
}

Occasion occasionNow() {
  struct tm t;
  if (!localTime(t)) return Occasion();
  return occasionOn(t, webInfoNow().birthday);
}
