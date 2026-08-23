// HTTPClient-Ersatz fuer den Host-Build.
//
// Statt einer HTTP-Anfrage wird eine lokale Datei gelesen. Der Dateiname ergibt
// sich aus der entity_id in der URL, sodass der Sketch-Code unveraendert bleibt:
//
//   .../api/history/period/...?filter_entity_id=sensor.xyz&...  ->  data/sensor.xyz.json
//   .../api/states/sensor.xyz                                   ->  data/state_sensor.xyz.json
//
// Gefuellt wird data/ von tools/simulator/fetch.sh — echte Antworten der
// echten Instanz. Damit laeuft im Simulator derselbe Parser ueber dieselben
// Daten wie auf dem Geraet.
#pragma once
#include <Arduino.h>

#define HTTP_CODE_OK           200
#define HTTP_CODE_UNAUTHORIZED 401
#define HTTP_CODE_NOT_FOUND    404

const char* simDataDir();

class HTTPClient {
public:
  bool begin(const String& url) { url_ = url.s; body_.clear(); return true; }
  void addHeader(const String&, const String&) {}
  void end() {}
  String getString() { return String(body_); }

  int GET() {
    std::string name;
    const size_t f = url_.find("filter_entity_id=");
    if (f != std::string::npos) {
      const size_t a = f + 17;
      const size_t b = url_.find('&', a);
      name = url_.substr(a, b == std::string::npos ? std::string::npos : b - a);
    } else {
      const size_t a = url_.find("/api/states/");
      if (a == std::string::npos) return HTTP_CODE_NOT_FOUND;
      name = "state_" + url_.substr(a + 12);
    }

    const std::string path = std::string(simDataDir()) + "/" + name + ".json";
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) {
      fprintf(stderr, "  [sim] fehlt: %s\n", path.c_str());
      return HTTP_CODE_NOT_FOUND;
    }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) body_.append(buf, n);
    fclose(fp);
    fprintf(stderr, "  [sim] %s (%zu Byte)\n", name.c_str(), body_.size());
    return HTTP_CODE_OK;
  }

private:
  std::string url_, body_;
};
