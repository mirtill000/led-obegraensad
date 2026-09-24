#include "modes/countdown_mode.h"

#include "bigdigits.h"
#include "display.h"
#include "settings.h"
#include "timekeeping.h"
#include "ui.h"

// Target time as epoch seconds, or 0 if the date isn't set/valid.
static time_t target() {
  const String &d = settings.countdownDate;
  if (d.length() != 10) return 0;
  struct tm t = {};
  t.tm_year = d.substring(0, 4).toInt() - 1900;
  t.tm_mon = d.substring(5, 7).toInt() - 1;
  t.tm_mday = d.substring(8, 10).toInt();
  t.tm_hour = settings.countdownTime.substring(0, 2).toInt();
  t.tm_min = settings.countdownTime.substring(3, 5).toInt();
  t.tm_isdst = -1;
  return mktime(&t);
}

// Whole calendar days from today to the target date (local).
static long daysLeft(time_t when) {
  const time_t now = time(nullptr);
  struct tm a, b;
  localtime_r(&when, &a);
  localtime_r(&now, &b);
  a.tm_hour = b.tm_hour = 12;
  a.tm_min = b.tm_min = a.tm_sec = b.tm_sec = 0;
  return lround(difftime(mktime(&a), mktime(&b)) / 86400.0);
}

String CountdownMode::sentence() {
  struct tm t;
  if (!localTime(t)) return "In attesa dell'ora...";
  const time_t when = target();
  if (!when) return "Scegli una data nella pagina";
  const String label = settings.countdownLabel;
  const long days = daysLeft(when);
  const long seconds = (long)difftime(when, time(nullptr));
  if (days < 0 || (days == 0 && seconds < -12 * 3600)) return label + " è passato";
  if (days == 0) {
    if (seconds <= 0) return label + ": è oggi!";
    const long h = seconds / 3600, m = seconds % 3600 / 60;
    return String("Mancano ") + (h ? String(h) + (h == 1 ? " ora e " : " ore e ") : String("")) + m +
           (m == 1 ? " minuto a " : " minuti a ") + label;
  }
  if (days == 1) return String("Domani: ") + label + "!";
  return String("Mancano ") + days + " giorni a " + label;
}

void CountdownMode::start() { lastDraw_ = 0; }

// Layout, like Previsioni (see ui.h):
//
//   rows 0-4    header: the event and when ("VACANZE  TRA 12 GIORNI")
//   rows 7-14   days left in big digits, or hours:minutes on the day
void CountdownMode::update(uint32_t now) {
  if (now - lastDraw_ < 50) return;
  lastDraw_ = now;
  display.clear();

  struct tm t;
  const time_t when = target();
  const String label = Display::fontText(settings.countdownLabel);
  if (!localTime(t)) {
    ui::headerLoop(label, now);
    ui::waiting(now, 10);
    display.render();
    return;
  }
  if (!when) {
    ui::headerLoop("SCEGLI UNA DATA NELLA PAGINA", now);
    display.render();
    return;
  }

  const long days = daysLeft(when);
  const long seconds = (long)difftime(when, time(nullptr));
  String header = label;
  if (days < 0 || (days == 0 && seconds < -12 * 3600)) {
    header += "  E' PASSATO";
  } else if (days == 0) {
    header += seconds <= 0 ? "  E' OGGI!" : "  OGGI";
    // Hours:minutes left in the small font.
    const long s = max(0L, seconds);
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld:%02ld", s / 3600, s % 3600 / 60);
    const int w = Display::textWidth(buf, 0, strlen(buf)) - 1;
    display.drawText((COLS - w) / 2, 7, buf, 0, strlen(buf));
  } else {
    header += days == 1 ? String("  DOMANI") : String("  TRA ") + days + " GIORNI";
    if (days <= 99) {
      drawBigNumber(days, 8);
    } else {
      const String n(days);
      const int w = Display::textWidth(n.c_str(), 0, n.length()) - 1;
      display.drawText((COLS - w) / 2, 7, n.c_str(), 0, n.length());
    }
  }
  ui::headerLoop(header, now);
  display.render();
}
