#include "backup.h"

#include <mbedtls/base64.h>
#include <nvs.h>

#include <vector>

#include "settings.h"

static const char *NAMESPACE = "obegransad";
static const char *FORMAT = "obegransad-backup";

static String quote(const String &s) {
  String out = "\"";
  for (unsigned i = 0; i < s.length(); i++) {
    const char c = s[i];
    if (c == '"' || c == '\\') out += '\\', out += c;
    else if (c == '\n') out += "\\n";
    else if ((uint8_t)c < 0x20) out += ' ';
    else out += c;
  }
  return out + "\"";
}

static const char *typeName(nvs_type_t t) {
  switch (t) {
    case NVS_TYPE_U8: return "u8";
    case NVS_TYPE_I8: return "i8";
    case NVS_TYPE_U16: return "u16";
    case NVS_TYPE_I16: return "i16";
    case NVS_TYPE_U32: return "u32";
    case NVS_TYPE_I32: return "i32";
    case NVS_TYPE_STR: return "str";
    case NVS_TYPE_BLOB: return "blob";
    default: return nullptr;
  }
}

String settingsBackup() {
  String json = String("{\"format\":\"") + FORMAT + "\",\"settings\":[";
  nvs_handle_t h;
  if (nvs_open(NAMESPACE, NVS_READONLY, &h) != ESP_OK) return json + "],\"quotes\":\"\"}";
  nvs_iterator_t it = nullptr;
  esp_err_t r = nvs_entry_find("nvs", NAMESPACE, NVS_TYPE_ANY, &it);
  bool first = true;
  while (r == ESP_OK) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    const char *type = typeName(info.type);
    String value;
    bool ok = type != nullptr;
    switch (info.type) {
      case NVS_TYPE_U8: { uint8_t v; ok = nvs_get_u8(h, info.key, &v) == ESP_OK; value = String(v); break; }
      case NVS_TYPE_I8: { int8_t v; ok = nvs_get_i8(h, info.key, &v) == ESP_OK; value = String(v); break; }
      case NVS_TYPE_U16: { uint16_t v; ok = nvs_get_u16(h, info.key, &v) == ESP_OK; value = String(v); break; }
      case NVS_TYPE_I16: { int16_t v; ok = nvs_get_i16(h, info.key, &v) == ESP_OK; value = String(v); break; }
      case NVS_TYPE_U32: { uint32_t v; ok = nvs_get_u32(h, info.key, &v) == ESP_OK; value = String(v); break; }
      case NVS_TYPE_I32: { int32_t v; ok = nvs_get_i32(h, info.key, &v) == ESP_OK; value = String(v); break; }
      case NVS_TYPE_STR: {
        size_t len = 0;
        ok = nvs_get_str(h, info.key, nullptr, &len) == ESP_OK;
        if (ok) {
          std::vector<char> buf(len);
          ok = nvs_get_str(h, info.key, buf.data(), &len) == ESP_OK;
          value = buf.data();
        }
        break;
      }
      case NVS_TYPE_BLOB: {  // base64
        size_t len = 0;
        ok = nvs_get_blob(h, info.key, nullptr, &len) == ESP_OK;
        if (ok) {
          std::vector<uint8_t> buf(len);
          ok = nvs_get_blob(h, info.key, buf.data(), &len) == ESP_OK;
          std::vector<unsigned char> b64(len * 4 / 3 + 8);
          size_t outLen = 0;
          mbedtls_base64_encode(b64.data(), b64.size(), &outLen, buf.data(), len);
          value = String((const char *)b64.data()).substring(0, outLen);
        }
        break;
      }
      default: break;
    }
    if (ok) {
      json += first ? "[" : ",[";
      json += quote(info.key) + "," + quote(type) + "," + quote(value) + "]";
      first = false;
    }
    r = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);
  nvs_close(h);
  return json + "],\"quotes\":" + quote(settings.quotes) + "}";
}

// --- restore: a tiny reader for the file written above -----------------------

struct Reader {
  const String &s;
  int i = 0;
  void ws() {
    while (i < (int)s.length() && isspace((unsigned char)s[i])) i++;
  }
  bool eat(char c) {
    ws();
    if (i < (int)s.length() && s[i] == c) {
      i++;
      return true;
    }
    return false;
  }
  bool str(String &out) {
    if (!eat('"')) return false;
    out = "";
    while (i < (int)s.length() && s[i] != '"') {
      char c = s[i++];
      if (c == '\\' && i < (int)s.length()) {
        c = s[i++];
        if (c == 'n') c = '\n';
      }
      out += c;
    }
    return eat('"');
  }
};

struct Entry {
  String key, type, value;
};

const char *restoreSettings(const String &json) {
  if (json.indexOf(String("\"format\":\"") + FORMAT + "\"") < 0) return "Non è un backup della lampada";
  // Read everything first: nothing is changed if the file is broken.
  std::vector<Entry> entries;
  const int at = json.indexOf("\"settings\":");
  if (at < 0) return "Backup senza impostazioni";
  Reader r{json, at + 11};
  if (!r.eat('[')) return "Backup non valido";
  if (!r.eat(']')) {
    do {
      Entry e;
      if (!r.eat('[') || !r.str(e.key) || !r.eat(',') || !r.str(e.type) || !r.eat(',') || !r.str(e.value) || !r.eat(']')) {
        return "Backup non valido";
      }
      if (e.key.length() == 0 || e.key.length() > 15) return "Backup non valido";
      entries.push_back(e);
    } while (r.eat(','));
    if (!r.eat(']')) return "Backup non valido";
  }
  String quotes;
  const int q = json.indexOf("\"quotes\":");
  if (q >= 0) {
    Reader rq{json, q + 9};
    if (!rq.str(quotes)) return "Backup non valido";
  }

  nvs_handle_t h;
  if (nvs_open(NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return "Memoria delle impostazioni non disponibile";
  nvs_erase_all(h);  // settings missing from the backup go back to their defaults
  for (const Entry &e : entries) {
    const char *k = e.key.c_str();
    const long n = e.value.toInt();
    if (e.type == "u8") nvs_set_u8(h, k, n);
    else if (e.type == "i8") nvs_set_i8(h, k, n);
    else if (e.type == "u16") nvs_set_u16(h, k, n);
    else if (e.type == "i16") nvs_set_i16(h, k, n);
    else if (e.type == "u32") nvs_set_u32(h, k, strtoul(e.value.c_str(), nullptr, 10));
    else if (e.type == "i32") nvs_set_i32(h, k, n);
    else if (e.type == "str") nvs_set_str(h, k, e.value.c_str());
    else if (e.type == "blob") {
      std::vector<unsigned char> buf(e.value.length());
      size_t len = 0;
      if (mbedtls_base64_decode(buf.data(), buf.size(), &len, (const unsigned char *)e.value.c_str(), e.value.length()) == 0) {
        nvs_set_blob(h, k, buf.data(), len);
      }
    }
  }
  nvs_commit(h);
  nvs_close(h);
  settings.quotes = quotes;
  saveQuotes();
  return nullptr;
}
