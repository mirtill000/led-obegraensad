#include "occasions.h"

#include "timekeeping.h"
#include "webinfo.h"

void easterDate(int year, int &month, int &day) {
  const int a = year % 19, b = year / 100, c = year % 100, d = b / 4, e = b % 4;
  const int f = (b + 8) / 25, g = (b - f + 1) / 3, h = (19 * a + b - d - g + 15) % 30;
  const int i = c / 4, k = c % 4, l = (32 + 2 * e + 2 * i - h - k) % 7;
  const int m = (a + 11 * h + 22 * l) / 451;
  month = (h + l - 7 * m + 114) / 31;
  day = (h + l - 7 * m + 114) % 31 + 1;
}

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
  int em, ed;
  easterDate(t.tm_year + 1900, em, ed);
  // Easter Monday: the day after (Easter is never on the last day of a month
  // in a way that matters here: 22 March - 25 April).
  const bool easter = (month == em && (day == ed || day == ed + 1)) || (em == 3 && ed == 31 && month == 4 && day == 1);
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
  } else if (easter) {
    o.animation = "easter";
    o.name = "Pasqua";
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
