#include "webinfo.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "settings.h"
#include "timekeeping.h"

static WebInfo latest;
static SemaphoreHandle_t lock = xSemaphoreCreateMutex();
static volatile bool requested = true;

WebInfo webInfoNow() {
  xSemaphoreTake(lock, portMAX_DELAY);
  WebInfo copy = latest;
  xSemaphoreGive(lock);
  return copy;
}

void requestWebInfoUpdate() { requested = true; }

// ---------------------------------------------------------------------------
// Word of the day: a built-in list, one per day of the year.
static const char *const WORDS[] = {
    "Serendipità: trovare per caso una cosa bella mentre se ne cerca un'altra",
    "Resilienza: la capacità di riprendersi dopo una difficoltà",
    "Meriggiare: riposare all'ombra nelle ore calde del mezzogiorno",
    "Petricore: il profumo della terra dopo la pioggia",
    "Abbiocco: la sonnolenza che arriva dopo un pranzo abbondante",
    "Apricità: il tepore del sole in una giornata d'inverno",
    "Lapalissiano: così ovvio da non aver bisogno di spiegazioni",
    "Sprezzatura: fare con naturalezza una cosa difficile",
    "Struggimento: nostalgia intensa e dolce",
    "Ineffabile: che non si può esprimere a parole",
    "Limerenza: lo stato di chi è appena innamorato",
    "Solivago: che ama stare e camminare da solo",
    "Procrastinare: rimandare a domani quello che si potrebbe fare oggi",
    "Epifania: l'improvvisa comprensione di qualcosa",
    "Effimero: che dura pochissimo",
    "Ondivago: che cambia spesso idea o direzione",
    "Pervicace: ostinato, che non cede",
    "Obnubilare: offuscare, annebbiare la mente",
    "Sbadigliare: aprire la bocca per sonno o noia",
    "Cincischiare: perdere tempo in cose da poco",
    "Zuzzurellone: persona allegra e un po' infantile",
    "Tergiversare: evitare di decidere prendendo tempo",
    "Garbo: modo di fare gentile ed elegante",
    "Uggioso: grigio e malinconico, detto del tempo",
    "Fanfaluca: una chiacchiera senza importanza",
    "Lungimirante: che sa vedere lontano e prevedere",
    "Crepuscolo: la luce tenue dopo il tramonto",
    "Ciarliero: che parla molto e volentieri",
    "Mirabolante: straordinario, da lasciare a bocca aperta",
    "Scapigliato: con i capelli in disordine",
    "Imbambolato: fermo e distratto, come incantato",
    "Luccicanza: il brillare di qualcosa alla luce",
    "Sfavillare: brillare di luce viva",
    "Arzigogolo: un ragionamento troppo complicato",
    "Quisquilia: una cosa di nessuna importanza",
    "Madornale: enorme, detto di un errore",
    "Rocambolesco: pieno di avventure incredibili",
    "Sollucchero: una gioia che fa andare in estasi",
    "Bighellonare: andare in giro senza meta",
    "Ghiribizzo: un capriccio improvviso",
};
static const int WORD_COUNT = sizeof(WORDS) / sizeof(WORDS[0]);

// ---------------------------------------------------------------------------
// Streaming parsers: responses can be large, so they are read piece by
// piece instead of being held in memory whole.

// Scans Wikipedia's "on this day" (selected) response as it arrives:
//   {"selected": [{"text": "...", "pages": [...], "year": 1846}, ...]}
// It picks out each "text" and the "year" that follows it, so the large
// "pages" parts are never kept in memory.
class OnThisDayScanner : public Stream {
 public:
  OnThisDayScanner(String *out, uint8_t max) : out_(out), max_(max) {}
  size_t write(uint8_t c) override {
    switch (state_) {
      case SEEK_SELECTED:
        if (match(c, "\"selected\"")) state_ = SEEK_TEXT;
        break;
      case SEEK_TEXT:
        if (match(c, "\"text\":\"")) {
          text_ = "";
          state_ = IN_TEXT;
        }
        break;
      case IN_TEXT:
        if (escape_ > 0) {
          escaped(c);
        } else if (c == '\\') {
          escape_ = 1;
        } else if (c == '"') {
          state_ = SEEK_YEAR;
          pos_ = pos2_ = 0;
        } else if (text_.length() < 400) {
          text_ += (char)c;
        }
        break;
      case SEEK_YEAR:
        if (matchAlt(c, "\"text\":\"")) {  // another text before any year: start over
          text_ = "";
          state_ = IN_TEXT;
        } else if (match(c, "\"year\":")) {
          year_ = 0;
          negative_ = false;
          state_ = IN_YEAR;
        }
        break;
      case IN_YEAR:
        if (c == '-') {
          negative_ = true;
        } else if (c >= '0' && c <= '9') {
          year_ = year_ * 10 + (c - '0');
        } else if (c != ' ') {
          if (count < max_) {
            out_[count++] = String("Accadde oggi, ") + (negative_ ? String(year_) + " a.C." : String(year_)) + ": " + text_;
          }
          state_ = SEEK_TEXT;
          pos_ = 0;
        }
        break;
    }
    return 1;
  }
  size_t write(const uint8_t *buf, size_t n) override {
    for (size_t k = 0; k < n; k++) write(buf[k]);
    return n;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}

  uint8_t count = 0;

 private:
  enum State { SEEK_SELECTED, SEEK_TEXT, IN_TEXT, SEEK_YEAR, IN_YEAR };

  // Incremental substring matchers (the needles have no repeated prefixes
  // that matter here).
  bool match(uint8_t c, const char *needle) { return step(c, needle, pos_); }
  bool matchAlt(uint8_t c, const char *needle) { return step(c, needle, pos2_); }
  static bool step(uint8_t c, const char *needle, int &pos) {
    if (c == (uint8_t)needle[pos]) {
      if (needle[++pos] == 0) {
        pos = 0;
        return true;
      }
    } else {
      pos = c == (uint8_t)needle[0] ? 1 : 0;
    }
    return false;
  }

  // Escape sequences inside the text: \" \\ \/ \n and \uXXXX.
  void escaped(uint8_t c) {
    if (escape_ == 1) {
      if (c == 'u') {
        escape_ = 2;
        unicode_ = 0;
        return;
      }
      text_ += (c == 'n' || c == 't') ? ' ' : (char)c;
      escape_ = 0;
      return;
    }
    unicode_ = unicode_ * 16 + (c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10);
    if (++escape_ < 6) return;
    escape_ = 0;
    if (unicode_ < 0x80) {
      text_ += (char)unicode_;
    } else if (unicode_ < 0x800) {
      text_ += (char)(0xC0 | (unicode_ >> 6));
      text_ += (char)(0x80 | (unicode_ & 0x3F));
    } else {
      text_ += (char)(0xE0 | (unicode_ >> 12));
      text_ += (char)(0x80 | ((unicode_ >> 6) & 0x3F));
      text_ += (char)(0x80 | (unicode_ & 0x3F));
    }
  }

  String *out_;
  uint8_t max_;
  State state_ = SEEK_SELECTED;
  int pos_ = 0, pos2_ = 0;
  String text_;
  int escape_ = 0;
  uint32_t unicode_ = 0;
  long year_ = 0;
  bool negative_ = false;
};

uint8_t parseOnThisDay(const String &json, String *out, uint8_t max) {
  OnThisDayScanner scanner(out, max);
  scanner.write((const uint8_t *)json.c_str(), json.length());
  return scanner.count;
}

// Reads an iCal file line by line (unfolding continuation lines) and keeps
// only the next event, so the whole calendar never sits in memory.
class CalendarScanner : public Stream {
 public:
  explicit CalendarScanner(time_t now) : now_(now) {}
  size_t write(uint8_t c) override {
    if (c == '\r') return 1;
    if (c == '\n') {
      pendingBreak_ = true;
      return 1;
    }
    if (pendingBreak_) {
      pendingBreak_ = false;
      if (c == ' ' || c == '\t') return 1;  // folded line continues
      line(current_);
      current_ = "";
    }
    if (current_.length() < 400) current_ += (char)c;
    return 1;
  }
  size_t write(const uint8_t *buf, size_t n) override {
    for (size_t k = 0; k < n; k++) write(buf[k]);
    return n;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}

  void finish() {
    if (current_.length()) line(current_);
    current_ = "";
  }

  bool found = false;
  time_t start = 0;
  bool allDay = false;
  String summary;

 private:
  // Days since 1970-01-01 for a civil date (UTC).
  static long daysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const long yoe = y - era * 400;
    const long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
  }

  // DTSTART value: 20260923T150000Z (UTC), 20260923T150000 (local) or
  // 20260923 (all-day, local).
  static bool parseTime(const String &v, time_t &out, bool &allDay) {
    if (v.length() < 8) return false;
    struct tm t = {};
    t.tm_year = v.substring(0, 4).toInt() - 1900;
    t.tm_mon = v.substring(4, 6).toInt() - 1;
    t.tm_mday = v.substring(6, 8).toInt();
    allDay = v.length() < 15;
    if (!allDay) {
      t.tm_hour = v.substring(9, 11).toInt();
      t.tm_min = v.substring(11, 13).toInt();
      t.tm_sec = v.substring(13, 15).toInt();
    }
    if (!allDay && v.endsWith("Z")) {
      out = (time_t)daysFromCivil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday) * 86400 + t.tm_hour * 3600 +
            t.tm_min * 60 + t.tm_sec;
    } else {
      t.tm_isdst = -1;
      out = mktime(&t);  // local time
    }
    return true;
  }

  void line(const String &l) {
    if (l == "BEGIN:VEVENT") {
      inEvent_ = true;
      evStart_ = "";
      evSummary_ = "";
    } else if (l == "END:VEVENT" && inEvent_) {
      inEvent_ = false;
      time_t t;
      bool whole;
      if (!parseTime(evStart_, t, whole)) return;
      // All-day events count for the whole day they are on.
      const time_t ends = whole ? t + 86400 : t;
      if (ends <= now_) return;
      if (!found || t < start) {
        found = true;
        start = t;
        allDay = whole;
        summary = evSummary_;
      }
    } else if (inEvent_ && l.startsWith("DTSTART")) {
      evStart_ = l.substring(l.lastIndexOf(':') + 1);
    } else if (inEvent_ && l.startsWith("SUMMARY")) {
      evSummary_ = l.substring(l.indexOf(':') + 1);
      evSummary_.replace("\\,", ",");
      evSummary_.replace("\\;", ";");
      evSummary_.replace("\\n", " ");
    }
  }

  time_t now_;
  String current_;
  bool pendingBreak_ = false;
  bool inEvent_ = false;
  String evStart_, evSummary_;
};

bool parseCalendar(const String &ics, time_t now, String &summary, time_t &start, bool &allDay) {
  CalendarScanner scanner(now);
  scanner.write((const uint8_t *)ics.c_str(), ics.length());
  scanner.finish();
  summary = scanner.summary;
  start = scanner.start;
  allDay = scanner.allDay;
  return scanner.found;
}

String describeEvent(const String &summary, time_t start, bool allDay, time_t now) {
  static const char *const DAYS[] = {"Dom", "Lun", "Mar", "Mer", "Gio", "Ven", "Sab"};
  struct tm s, n;
  localtime_r(&start, &s);
  localtime_r(&now, &n);
  // Whole days between the two dates (local), via noon to dodge DST.
  struct tm a = s, b = n;
  a.tm_hour = b.tm_hour = 12;
  a.tm_min = b.tm_min = a.tm_sec = b.tm_sec = 0;
  const long days = lround(difftime(mktime(&a), mktime(&b)) / 86400.0);
  String when;
  if (days <= 0) {
    when = "Oggi";
  } else if (days == 1) {
    when = "Domani";
  } else {
    when = String(DAYS[s.tm_wday]) + " " + s.tm_mday + "/" + (s.tm_mon + 1);
  }
  if (allDay) return when + ": " + summary;
  char hm[6];
  snprintf(hm, sizeof(hm), "%d:%02d", s.tm_hour, s.tm_min);
  return when + " " + hm + " " + summary;
}

// ---------------------------------------------------------------------------

static bool get(const String &url, Stream &sink, int &code) {
  WiFiClientSecure client;
  client.setInsecure();  // public/read-only data
  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("OBEGRANSAD-lamp/1.0 (ESP32)");
  if (!http.begin(client, url)) return false;
  code = http.GET();
  if (code == HTTP_CODE_OK) http.writeToStream(&sink);
  http.end();
  return code == HTTP_CODE_OK;
}

void webInfoTick() {
  static int historyDay = -1;       // day of year fetched
  static uint32_t historyTried = 0, calendarTried = 0;
  static String calendarUrl;
  const uint32_t now = millis();
  struct tm t;
  if (!localTime(t)) return;  // everything here depends on the date

  const bool force = requested;
  requested = false;

  // Word of the day: no download.
  String word = settings.infoWord ? String("Parola del giorno - ") + WORDS[(t.tm_year * 366 + t.tm_yday) % WORD_COUNT] : "";
  xSemaphoreTake(lock, portMAX_DELAY);
  latest.word = word;
  if (!settings.infoCalendar || settings.icalUrl.length() == 0) {
    latest.event = "";
    latest.calendarStatus = settings.infoCalendar ? "manca il link del calendario" : "";
  }
  xSemaphoreGive(lock);

  if (WiFi.status() != WL_CONNECTED) return;

  if (settings.infoHistory && (force || historyDay != t.tm_yday) && (force || now - historyTried > 10 * 60000 || historyTried == 0)) {
    historyTried = now;
    char url[96];
    snprintf(url, sizeof(url), "https://api.wikimedia.org/feed/v1/wikipedia/it/onthisday/selected/%02d/%02d",
             t.tm_mon + 1, t.tm_mday);
    String items[12];
    OnThisDayScanner scanner(items, 12);
    int code = 0;
    get(url, scanner, code);
    const uint8_t count = scanner.count;
    xSemaphoreTake(lock, portMAX_DELAY);
    if (count) {
      for (uint8_t k = 0; k < count; k++) latest.history[k] = items[k];
      latest.historyCount = count;
      latest.historyStatus = String(count) + " eventi";
      historyDay = t.tm_yday;
    } else {
      latest.historyStatus = String("non disponibile (") + code + ")";
    }
    xSemaphoreGive(lock);
  }

  if (settings.infoCalendar && settings.icalUrl.length() &&
      (force || calendarUrl != settings.icalUrl || calendarTried == 0 || now - calendarTried > 15 * 60000)) {
    calendarTried = now;
    calendarUrl = settings.icalUrl;
    String url = calendarUrl;
    if (url.startsWith("webcal://")) url = String("https://") + url.substring(9);
    CalendarScanner scanner(time(nullptr));
    int code = 0;
    const bool ok = get(url, scanner, code);
    scanner.finish();
    xSemaphoreTake(lock, portMAX_DELAY);
    if (!ok) {
      latest.calendarStatus = String("non raggiungibile (") + code + ")";
    } else if (scanner.found) {
      latest.event = String("Prossimo evento - ") + describeEvent(scanner.summary, scanner.start, scanner.allDay, time(nullptr));
      latest.calendarStatus = "ok";
    } else {
      latest.event = "";
      latest.calendarStatus = "nessun evento in arrivo";
    }
    xSemaphoreGive(lock);
  }
}
