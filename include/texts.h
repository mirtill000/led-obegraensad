#pragma once

// The messages the firmware gives back to people - the page, the Cardputer,
// the API - in one place, so the same situation reads the same everywhere
// and a translation means changing this file only (scripts/check_texts.py
// fails the build if one of them is written out again in src/).
//
// What isn't here, and why:
//  - the names of modes, games and animations: each class says its own
//    (name()), and the page and the Cardputer read them from the lamp;
//  - the names of settings and of their choices: SETTING_DEFS (settings.cpp),
//    sent to the page and the Cardputer;
//  - what the panel draws (scrolling texts, quotes, words of the day): content.
namespace txt {

// Commands and settings that are refused.
constexpr const char *EMPTY_COMMAND = "Comando vuoto";
constexpr const char *UNKNOWN_COMMAND = "Comando sconosciuto";
constexpr const char *UNKNOWN_KEY = "Tasto sconosciuto";
constexpr const char *UNKNOWN_MODE = "Modalità sconosciuta";
constexpr const char *UNKNOWN_GAME = "Gioco sconosciuto";
constexpr const char *UNKNOWN_ANIMATION = "Animazione sconosciuta";
constexpr const char *UNKNOWN_SCENE = "Non c'è niente con questo nome";
constexpr const char *OUT_OF_SEASON = "Fuori stagione";
constexpr const char *UNKNOWN_SETTING = "Impostazione sconosciuta";
constexpr const char *SETTING_PAGE_ONLY = "Si cambia solo dalla pagina";
constexpr const char *SETTING_READ_ONLY = "Impostazione non modificabile";
constexpr const char *EMPTY_TEXT = "Testo vuoto";
constexpr const char *NEED_TEXT_OR_ICON = "Serve un testo o un'icona conosciuta";
constexpr const char *BAD_PIXEL = "Pixel non valido";
constexpr const char *NEED_NAME = "Serve un nome";
constexpr const char *VALUE_NOT_ALLOWED = "Valore non ammesso";
constexpr const char *VALUE_TOO_LONG = "Testo troppo lungo";
constexpr const char *VALUE_MISSING = "Valore mancante";
constexpr const char *VALUE_INVALID = "Valore non valido";
constexpr const char *NUMBER_INVALID = "Numero non valido";
constexpr const char *NUMBER_OUT_OF_RANGE = "Valore fuori dai limiti";
constexpr const char *DRAWING_NOT_FOUND = "Disegno non trovato";
constexpr const char *DRAWING_INVALID = "Disegno non valido";
constexpr const char *GALLERY_FULL = "Impossibile salvare (galleria piena?)";
constexpr const char *SPRITE_NOT_FOUND = "Sprite non trovato";
constexpr const char *SPRITE_BAD_SIZE = "Lo sprite non ha la misura giusta";

// The state of what the lamp downloads (netfetch.cpp).
constexpr const char *FETCH_WAITING = "in attesa";
constexpr const char *FETCH_UPDATED = "aggiornato";        // "aggiornato 3 min fa"
constexpr const char *FETCH_ERROR = "errore";              // "errore 503"
constexpr const char *FETCH_UNREACHABLE = "non raggiungibile";
constexpr const char *FETCH_RETRY = "riprovo tra";         // "riprovo tra 4 min"
constexpr const char *FETCH_RETRY_SOON = "poco";
constexpr const char *FETCH_LAST = "ultimo dato di";       // "ultimo dato di 2 h fa"
constexpr const char *FETCH_CACHED = "dalla memoria";
constexpr const char *AGO_NOW = "adesso";
constexpr const char *AGO_MIN = " min fa";
constexpr const char *AGO_HOURS = " h fa";
constexpr const char *AGO_DAYS = " g fa";

}  // namespace txt
