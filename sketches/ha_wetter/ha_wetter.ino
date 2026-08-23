// ha_wetter — Wind, Luftdruck und Wetterlage in vier Spalten.
//
//   Richtung   Kompassrose mit Pfeil, darunter Sektor und Gradzahl
//   Wind       Geschwindigkeit in km/h
//   Luftdruck  aktueller Wert und die Aenderung der letzten drei Stunden
//   Wetter     Icon plus Zustand als Wort
//
// Entscheidungen, die man dem Bild nicht ansieht:
//
//   * Der Pfeil zeigt dorthin, WOHER der Wind kommt — also auf die NNW-Marke,
//     wenn NNW danebensteht. Home Assistant liefert die Richtung nach
//     meteorologischer Konvention als Herkunft. Ein kartenueblicher Pfeil in
//     Wehrichtung zeigte bei "NNW" nach SSO, und Bild und Text saehen aus, als
//     widersprachen sie sich.
//
//   * Der Luftdruck kommt vom Solarnode, nicht aus der Vorhersage. Der Wert
//     liegt rund 16 hPa unter dem, was Wetter-Apps zeigen: die reduzieren auf
//     Meereshoehe, der Solarnode misst vor Ort. Fuer die Aussage ist das ohne
//     Belang — beim Luftdruck zaehlt die Tendenz, und eine Differenz ist
//     hoehenunabhaengig. Drei Stunden sind dabei nicht willkuerlich, sondern
//     der meteorologische Standardzeitraum fuer die Drucktendenz.
//
//   * Fuenf Icons plus Mond statt nur Sonne und Regen. Home Assistant kennt 15
//     Zustaende; mit zwei Bildern waere aus 'cloudy' und 'fog' ein Sonnentag
//     geworden. Die Zuordnung steht als Tabelle in CONDITIONS.
//
//   * Icons als Umriss, nicht gefuellt: erst die Form 2 px groesser in
//     Schwarz, dann dieselbe Form in Weiss darueber. Eine flaechig schwarze
//     Wolke waere auf E-Paper ein Klecks, und Umrisse aus einzelnen Boegen
//     zusammenzusetzen scheitert daran, dass sich die Boegen der drei
//     Wolkenbaeuche gegenseitig durchschneiden.
//
// Zugangsdaten: secrets.h (per .gitignore ausgeschlossen), Vorlage secrets.h.example

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <math.h>

#include "EPD.h"
#include "weather.h"
#include "secrets.h"

uint8_t ImageBW[27200];

const int PIN_DISPLAY_POWER = 7;
const uint16_t DISPLAY_ROTATION = 0;

const int SCREEN_W = 792;    // sichtbar; der Puffer (EPD_W) ist 800 breit
const int SCREEN_H = 272;

// Drei Stunden ist der meteorologische Standard fuer die Drucktendenz.
const int TREND_HOURS = 3;

const unsigned long UPDATE_INTERVAL_MS = 10UL * 60UL * 1000UL;
const unsigned long RETRY_INTERVAL_MS  = 60UL * 1000UL;
const int FULL_REFRESH_EVERY = 6;          // jeder sechste Durchgang, also stuendlich

const char* TZ_BERLIN = "CET-1CEST,M3.5.0,M10.5.0/3";

// Ueber friendly_name verifiziert, nicht geraten.
const char* ENT_WIND_SPEED  = "sensor.windgeschwindigkeit";
const char* ENT_WIND_DIR    = "sensor.windrichtung";
const char* ENT_WIND_SECTOR = "sensor.windrichtung_sektor";
const char* ENT_PRESSURE    = "sensor.solarnode_luftdruck";
const char* ENT_CONDITION   = "weather.forecast_home";

// Home Assistant kennt diese 15 Zustaende. Was hier nicht steht, landet als
// Wolke mit dem Rohzustand als Text — besser ein unbekanntes Wort als ein
// falsches Bild.
const ConditionDef CONDITIONS[] = {
  { "sunny",          ICON_SUN,    "sonnig"      },
  { "clear-night",    ICON_MOON,   "klar"        },
  { "partlycloudy",   ICON_PARTLY, "heiter"      },
  { "cloudy",         ICON_CLOUD,  "bewoelkt"    },
  { "fog",            ICON_CLOUD,  "Nebel"       },
  { "windy",          ICON_CLOUD,  "windig"      },
  { "windy-variant",  ICON_CLOUD,  "windig"      },
  { "rainy",          ICON_RAIN,   "Regen"       },
  { "pouring",        ICON_RAIN,   "Starkregen"  },
  { "lightning",      ICON_RAIN,   "Gewitter"    },
  { "lightning-rainy",ICON_RAIN,   "Gewitter"    },
  { "hail",           ICON_SNOW,   "Hagel"       },
  { "snowy",          ICON_SNOW,   "Schnee"      },
  { "snowy-rainy",    ICON_SNOW,   "Schneeregen" },
  { "exceptional",    ICON_CLOUD,  "Unwetter"    },
};
const int NCONDITION = sizeof(CONDITIONS) / sizeof(CONDITIONS[0]);

// --- Layout -----------------------------------------------------------------
// Vier Spalten zu je rund 197 px zwischen dem 2 px starken Aussenrahmen, unten
// ein Streifen fuer die Fusszeile — dieselbe Bildsprache wie ha_kacheln.
//
// Breitester Fall ist der Luftdruck: "1008,0" belegt bei Groesse 48
// 4*24 + 12 (enges Komma) + 24 = 132 px und passt damit in die 181 px
// Inhaltsbreite einer Spalte.
const int FRAME_X0 = 2, FRAME_X1 = SCREEN_W - 3;      // 789
const int FRAME_Y0 = 2, FRAME_Y1 = SCREEN_H - 3;      // 269
const int GRID_Y1  = 247;
const int COL_X[]  = { FRAME_X0, 199, 396, 593, FRAME_X1 };
const int NCOL = 4;

const int PAD_X    = 8;
const int HEAD_Y   = 10;                              // Spaltenueberschrift, Groesse 16
const int VALUE_Y  = 100;                             // grosse Zahl, Groesse 48
const int UNIT_Y   = 160;                             // Einheit, Groesse 24
const int EXTRA_Y  = 200;                             // Trend bzw. Zustandswort, Groesse 24
const int FOOTER_SZ = 16;
const int FOOTER_Y  = GRID_Y1 + 5;                    // 252, Textzeile 252..268

const int ROSE_CY = 118, ROSE_R = 66;                 // Rose 52..184
const int ROSE_TEXT_Y = 192;                          // Sektor, Groesse 24
const int ROSE_DEG_Y  = 222;                          // Gradzahl, Groesse 16

Reading windSpeed, windDir, windSector, condition;
Trend   pressure;
bool lastOk = false;
int  updateCount = 0;

// ---------------------------------------------------------------------------
// Zeichenhilfen
// ---------------------------------------------------------------------------

// Paint_SetPixel() prueft seine Koordinaten NICHT und schreibt sonst ueber den
// Puffer hinaus; die Parameter sind uint16_t, ein negativer Wert wuerde zu
// einer riesigen Zahl. Deshalb signed rechnen und vorher abfangen.
static void safePixel(int x, int y, uint16_t color) {
  if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
  Paint_SetPixel((uint16_t)x, (uint16_t)y, color);
}

static void fillRect(int x0, int y0, int x1, int y1, uint16_t color) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) safePixel(x, y, color);
}

static void safeLine(int x0, int y0, int x1, int y1, uint16_t color) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    safePixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    const int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

// Eine 1 px starke Linie ist auf diesem Panel kaum zu sehen — dieselbe
// Beobachtung wie bei EPD_DrawCircle().
static void thickLine(int x0, int y0, int x1, int y1, int th, uint16_t color) {
  for (int i = 0; i < th; i++) {
    safeLine(x0 + i, y0, x1 + i, y1, color);
    safeLine(x0, y0 + i, x1, y1 + i, color);
  }
}

// Eigene Kreisfunktionen statt EPD_DrawCircle(): das prueft seine Koordinaten
// nicht, kennt keine Strichstaerke und kann nichts in Weiss aus einer Flaeche
// herausschneiden — beides braucht der Icon-Code.
static void fillDisc(int cx, int cy, int r, uint16_t color) {
  const int rr = r * r;
  for (int y = -r; y <= r; y++)
    for (int x = -r; x <= r; x++)
      if (x * x + y * y <= rr) safePixel(cx + x, cy + y, color);
}

static void drawRing(int cx, int cy, int r, int th, uint16_t color) {
  const int ro = r * r, ri = (r - th) * (r - th);
  for (int y = -r; y <= r; y++)
    for (int x = -r; x <= r; x++) {
      const int d = x * x + y * y;
      if (d <= ro && d >= ri) safePixel(cx + x, cy + y, color);
    }
}

// Gefuelltes Dreieck ueber die Kantenfunktion. Gebraucht fuer den Kompasspfeil:
// EPD.h kennt nur Linie, Rechteck und Kreis.
static float edgeFn(float ax, float ay, float bx, float by, float px, float py) {
  return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static void fillTriangle(float x0, float y0, float x1, float y1,
                         float x2, float y2, uint16_t color) {
  const int minX = (int)floorf(fminf(x0, fminf(x1, x2)));
  const int maxX = (int)ceilf (fmaxf(x0, fmaxf(x1, x2)));
  const int minY = (int)floorf(fminf(y0, fminf(y1, y2)));
  const int maxY = (int)ceilf (fmaxf(y0, fmaxf(y1, y2)));
  for (int y = minY; y <= maxY; y++)
    for (int x = minX; x <= maxX; x++) {
      const float px = x + 0.5f, py = y + 0.5f;
      const float w0 = edgeFn(x0, y0, x1, y1, px, py);
      const float w1 = edgeFn(x1, y1, x2, y2, px, py);
      const float w2 = edgeFn(x2, y2, x0, y0, px, py);
      if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0))
        safePixel(x, y, color);
    }
}

// Das Grad-Zeichen fehlt im Font (ASCII 32..126), also selbst zeichnen.
static void drawDegreeSign(int cx, int cy, int r, int thickness) {
  drawRing(cx, cy, r + thickness, thickness, BLACK);
}

// EPD_ShowString() bricht nicht um; Zeichenbreite ist size/2.
static int textWidth(const char* s, int size) { return (int)strlen(s) * (size / 2); }

// Dezimaltrennzeichen auf Deutsch. snprintf() schreibt immer einen Punkt, und
// die Locale-Umschaltung dafuer gibt es in der Arduino-Laufzeit nicht.
static void commaDecimal(char* s) {
  for (char* p = s; *p; p++) if (*p == '.') *p = ',';
}

// Der Font ist dickte-gleich: EPD_ShowString() rueckt je Zeichen size/2 vor,
// auch beim Komma, dessen Tinte nur die ersten rund 30 % der Zelle belegt.
// Zahlen werden deshalb zeichenweise gesetzt, nach Komma und Punkt nur size/4.
// Weiter nicht: EPD_ShowChar() malt die ganze Zelle inklusive Hintergrund und
// wuerde das Komma sonst wieder ausradieren. (Siehe ha_kacheln.)
static int advanceFor(char c, int size) {
  return (c == ',' || c == '.') ? size / 4 : size / 2;
}

static int numberWidth(const char* s, int size) {
  int w = 0;
  for (const char* p = s; *p; p++) w += advanceFor(*p, size);
  return w;
}

static void showNumber(int x, int y, const char* s, int size, uint16_t color) {
  for (const char* p = s; *p; p++) {
    EPD_ShowChar((uint16_t)x, (uint16_t)y, (uint16_t)*p, (uint16_t)size, color);
    x += advanceFor(*p, size);
  }
}

static void showCentered(int cx, int y, const char* s, int size) {
  EPD_ShowString(cx - textWidth(s, size) / 2, y, s, size, BLACK);
}

static void showNumberCentered(int cx, int y, const char* s, int size) {
  showNumber(cx - numberWidth(s, size) / 2, y, s, size, BLACK);
}

// ---------------------------------------------------------------------------
// Kompassrose
// ---------------------------------------------------------------------------

// Bildschirmkoordinaten zu einem Winkel in Grad, 0 = Norden = oben.
static void polar(int cx, int cy, float deg, float r, float& x, float& y) {
  const float a = deg * (float)M_PI / 180.0f;
  x = cx + r * sinf(a);
  y = cy - r * cosf(a);
}

static void drawCompass(int cx, int cy, int r, bool haveDir, float deg) {
  drawRing(cx, cy, r, 3, BLACK);

  // Marken: lang fuer die vier Haupt-, kurz fuer die Zwischenrichtungen.
  for (int i = 0; i < 16; i++) {
    const float a = i * 22.5f;
    const int len = (i % 4 == 0) ? 12 : (i % 2 == 0 ? 8 : 4);
    float x0, y0, x1, y1;
    polar(cx, cy, a, (float)(r - 3), x0, y0);
    polar(cx, cy, a, (float)(r - 3 - len), x1, y1);
    thickLine((int)x0, (int)y0, (int)x1, (int)y1, (i % 4 == 0) ? 3 : 2, BLACK);
  }

  // Buchstaben INNEN, nicht aussen: aussen braeuchten sie 16 px zusaetzlichen
  // Rand, und die Spalte ist nur 197 px breit.
  const int lr = r - 30;
  struct { const char* s; float deg; } marks[] = {
    { "N", 0 }, { "O", 90 }, { "S", 180 }, { "W", 270 }
  };
  for (auto& m : marks) {
    float x, y;
    polar(cx, cy, m.deg, (float)lr, x, y);
    EPD_ShowString((uint16_t)(x - 4), (uint16_t)(y - 8), m.s, 16, BLACK);
  }

  if (!haveDir) {
    showCentered(cx, cy - 8, "?", 24);
    return;
  }

  // Kompassnadel: langes Dreieck zur Herkunftsrichtung, kurzes Gegenstueck.
  // Der Pfeil zeigt dorthin, WOHER der Wind kommt — siehe Kopfkommentar.
  const float L = r - 34, W = 11, T = 22;
  float tx, ty, lx, ly, rx, ry, bx, by;
  polar(cx, cy, deg,         L, tx, ty);
  polar(cx, cy, deg - 90.0f, W, lx, ly);
  polar(cx, cy, deg + 90.0f, W, rx, ry);
  polar(cx, cy, deg + 180.0f, T, bx, by);
  fillTriangle(tx, ty, lx, ly, rx, ry, BLACK);
  fillTriangle(lx, ly, bx, by, rx, ry, BLACK);
  fillDisc(cx, cy, 4, BLACK);
}

// ---------------------------------------------------------------------------
// Wetter-Icons
// ---------------------------------------------------------------------------

// Die Wolke als Vereinigung dreier Kreise und eines Rechtecks. `grow` blaeht
// die Form auf: erst in Schwarz mit grow=2 zeichnen, dann in Weiss mit grow=0
// darueber — uebrig bleibt ein 2 px starker Umriss. Der Umweg ist noetig, weil
// sich die Boegen der drei Baeuche sonst gegenseitig durchschneiden wuerden.
static void cloudShape(int cx, int cy, int w, int grow, uint16_t color) {
  // Drei Baeuche und ein Rechteck, das die Luecken darunter schliesst. Das
  // Rechteck endet genau an den aeusseren Kreisraendern und nicht bei
  // cx +- w/2: sonst steht rechts eine rechtwinklige Stufe ueber den Bauch
  // hinaus, was am Panel wie ein Zeichenfehler aussieht.
  const int leftX  = cx - w / 3, leftR  = w / 6;
  const int midX   = cx - w / 8, midR   = w / 4;
  const int rightX = cx + w / 4, rightR = w / 5;

  fillDisc(midX,   cy - w / 10, midR   + grow, color);
  fillDisc(leftX,  cy + w / 12, leftR  + grow, color);
  fillDisc(rightX, cy + w / 20, rightR + grow, color);
  fillRect(leftX  - leftR  - grow, cy + w / 12 - grow,
           rightX + rightR + grow, cy + w / 5  + grow, color);
}

static void drawCloud(int cx, int cy, int w) {
  cloudShape(cx, cy, w, 2, BLACK);
  cloudShape(cx, cy, w, 0, WHITE);
}

static void drawSun(int cx, int cy, int r) {
  drawRing(cx, cy, r, 3, BLACK);
  for (int i = 0; i < 8; i++) {
    float x0, y0, x1, y1;
    polar(cx, cy, i * 45.0f, (float)(r + 6),  x0, y0);
    polar(cx, cy, i * 45.0f, (float)(r + 16), x1, y1);
    thickLine((int)x0, (int)y0, (int)x1, (int)y1, 3, BLACK);
  }
}

static void drawMoon(int cx, int cy, int r) {
  fillDisc(cx, cy, r, BLACK);
  fillDisc(cx + r / 2, cy - r / 6, r - 3, WHITE);
}

static void drawDrops(int cx, int cy, int w, int n) {
  for (int i = 0; i < n; i++) {
    const int x = cx - w / 3 + i * (w / (n + 1));
    thickLine(x, cy, x - 6, cy + 16, 3, BLACK);
  }
}

static void drawFlakes(int cx, int cy, int w, int n) {
  for (int i = 0; i < n; i++) {
    const int x = cx - w / 3 + i * (w / (n + 1));
    const int y = cy + 8 + (i % 2) * 8;
    thickLine(x - 6, y, x + 6, y, 2, BLACK);
    thickLine(x, y - 6, x, y + 6, 2, BLACK);
    thickLine(x - 4, y - 4, x + 4, y + 4, 2, BLACK);
    thickLine(x - 4, y + 4, x + 4, y - 4, 2, BLACK);
  }
}

static void drawIcon(IconKind kind, int cx, int cy) {
  switch (kind) {
    case ICON_SUN:    drawSun(cx, cy, 26); break;
    case ICON_MOON:   drawMoon(cx, cy, 30); break;
    case ICON_PARTLY:
      drawSun(cx - 22, cy - 20, 18);
      drawCloud(cx + 14, cy + 14, 80);
      break;
    case ICON_CLOUD:  drawCloud(cx, cy, 96); break;
    case ICON_RAIN:
      drawCloud(cx, cy - 14, 90);
      drawDrops(cx, cy + 22, 90, 4);
      break;
    case ICON_SNOW:
      drawCloud(cx, cy - 16, 90);
      drawFlakes(cx, cy + 18, 90, 3);
      break;
  }
}

// ---------------------------------------------------------------------------
// Zeit
// ---------------------------------------------------------------------------

static long daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (long)era * 146097 + (long)doe - 719468;
}

static time_t parseIsoUtc(const char* s) {
  int Y, M, D, h, mi, se;
  if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &Y, &M, &D, &h, &mi, &se) != 6) return 0;
  return (time_t)(daysFromCivil(Y, M, D) * 86400L + h * 3600L + mi * 60L + se);
}

// HA antwortet auf '+00:00' mit "Invalid end_time": das '+' wird im
// Query-String zum Leerzeichen dekodiert. Deshalb 'Z'.
static void formatIsoUtc(time_t t, char* out, size_t n) {
  struct tm g;
  gmtime_r(&t, &g);
  strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &g);
}

static bool timeIsSet() { return time(nullptr) >= 1600000000L; }

static bool waitForTime(unsigned long timeoutMs) {
  if (timeIsSet()) return true;
  configTzTime(TZ_BERLIN, "pool.ntp.org", "time.nist.gov", "192.168.178.1");
  const unsigned long t0 = millis();
  while (!timeIsSet() && millis() - t0 < timeoutMs) delay(250);
  return timeIsSet();
}

// ---------------------------------------------------------------------------
// Home Assistant
// ---------------------------------------------------------------------------

static bool connectWiFi(unsigned long timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  const unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) delay(250);
  return WiFi.status() == WL_CONNECTED;
}

static bool haGet(const String& path, String& out, String& errorOut) {
  HTTPClient http;
  const String url = String("http://") + HA_HOST + ":" + HA_PORT + path;
  if (!http.begin(url)) { errorOut = "http.begin fehlgeschlagen"; return false; }
  http.addHeader("Authorization", String("Bearer ") + HA_TOKEN);
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    errorOut = String("HTTP ") + code;
    if (code == HTTP_CODE_UNAUTHORIZED) errorOut += " (Token?)";
    if (code == HTTP_CODE_NOT_FOUND)    errorOut += " (entity_id?)";
    http.end();
    return false;
  }
  out = http.getString();
  http.end();
  return true;
}

// Holt /api/states/<entity> und zieht den Zustand heraus. 'unavailable' und
// 'unknown' sind gueltige Zustaende, keine Fehler — sie gelten hier aber als
// "kein Wert", damit atof() sie nicht klaglos zu 0.0 macht.
static bool fetchState(const char* entity, Reading& r) {
  String body, err;
  r = Reading();
  if (!haGet(String("/api/states/") + entity, body, err)) {
    Serial.printf("HA: %-32s FEHLER %s\n", entity, err.c_str());
    return false;
  }
  const char* p = strstr(body.c_str(), "\"state\":\"");
  if (!p) return false;
  p += 9;
  const char* e = strchr(p, '"');
  if (!e) return false;

  const size_t n = (size_t)(e - p);
  if (n == 0 || n >= sizeof(r.text)) return false;
  memcpy(r.text, p, n);
  r.text[n] = 0;

  if (!strcmp(r.text, "unavailable") || !strcmp(r.text, "unknown")) return false;

  const bool numeric = (*p == '-' || (*p >= '0' && *p <= '9'));
  r.value = numeric ? atof(p) : 0.0f;
  r.ok = true;
  Serial.printf("HA: %-32s %s\n", entity, r.text);
  return true;
}

// Aktueller Wert und Vergleichswert aus EINER History-Antwort. Gesucht ist der
// letzte Messwert VOR tRef, nicht der erste der Antwort: so stimmt der
// Vergleich auch, wenn der Funktion ein laengerer Zeitraum vorgesetzt wird —
// der Simulator legt genau das bereit. (Wie in ha_kacheln.)
static bool fetchTrend(const char* entity, time_t tRef, time_t tEnd, Trend& t) {
  String body, err;
  t = Trend();

  char s0[32], s1[32];
  formatIsoUtc(tRef, s0, sizeof(s0));
  formatIsoUtc(tEnd, s1, sizeof(s1));

  // Ohne end_time liefert HA nur EINEN Tag ab start_time, nicht bis jetzt.
  const String path = String("/api/history/period/") + s0 +
                      "?filter_entity_id=" + entity +
                      "&end_time=" + s1 +
                      "&minimal_response&no_attributes";
  if (!haGet(path, body, err)) {
    Serial.printf("HA: %-32s FEHLER %s\n", entity, err.c_str());
    return false;
  }

  const char* p = body.c_str();
  bool haveFallback = false;
  float fallback = 0;

  while ((p = strstr(p, "\"state\":\"")) != nullptr) {
    p += 9;
    const char* stateEnd = strchr(p, '"');
    if (!stateEnd) break;
    const bool numeric = (*p == '-' || (*p >= '0' && *p <= '9'));
    const float v = numeric ? atof(p) : NAN;

    const char* lc = strstr(stateEnd, "\"last_changed\":\"");
    if (!lc) break;
    lc += 16;
    const time_t ts = parseIsoUtc(lc);
    p = lc;

    if (isnan(v)) continue;
    t.value = v;
    t.ok = true;
    t.points++;

    if (ts <= tRef)        { t.ref = v; t.hasRef = true; }
    else if (!haveFallback){ fallback = v; haveFallback = true; }
  }
  if (!t.hasRef && haveFallback) { t.ref = fallback; t.hasRef = true; }

  Serial.printf("HA: %-32s %.1f (vor %d h %.1f), %d Punkte\n",
                entity, t.value, TREND_HOURS, t.hasRef ? t.ref : NAN, t.points);
  return t.ok;
}

// ---------------------------------------------------------------------------
// Panel
// ---------------------------------------------------------------------------

static void panelInit() {
  EPD_GPIOInit();
  EPD_FastMode1Init();     // enthaelt den Hardware-Reset
}

static void newFrame() {
  Paint_NewImage(ImageBW, EPD_W, EPD_H, DISPLAY_ROTATION, WHITE);
  Paint_Clear(WHITE);
}

static void flush(bool full) {
  if (full) {
    panelInit();
    EPD_Display_Clear();
    EPD_Update();
    panelInit();
    EPD_Display(ImageBW);
    EPD_Update();
  } else {
    panelInit();
    EPD_Display(ImageBW);
    EPD_FastUpdate();
  }
}

static void showError(const char* headline, const char* detail) {
  newFrame();
  EPD_DrawRectangle(4, 4, SCREEN_W - 5, SCREEN_H - 5, BLACK, 0);
  EPD_ShowString(50,  70, headline, 48, BLACK);
  EPD_ShowString(50, 140, detail,   24, BLACK);
  EPD_ShowString(50, 190, "Details per: make monitor", 16, BLACK);
  flush(true);
}

// ---------------------------------------------------------------------------
// Darstellung
// ---------------------------------------------------------------------------

static int colCenter(int col) { return (COL_X[col] + COL_X[col + 1]) / 2; }

static void drawGrid() {
  EPD_DrawRectangle(FRAME_X0,     FRAME_Y0,     FRAME_X1,     FRAME_Y1,     BLACK, 0);
  EPD_DrawRectangle(FRAME_X0 + 1, FRAME_Y0 + 1, FRAME_X1 - 1, FRAME_Y1 - 1, BLACK, 0);
  for (int c = 1; c < NCOL; c++)
    fillRect(COL_X[c], FRAME_Y0, COL_X[c] + 1, GRID_Y1, BLACK);
  fillRect(FRAME_X0, GRID_Y1, FRAME_X1, GRID_Y1 + 1, BLACK);
}

static void drawDirection() {
  const int cx = colCenter(0);
  showCentered(cx, HEAD_Y, "Richtung", 16);
  drawCompass(cx, ROSE_CY, ROSE_R, windDir.ok, windDir.value);

  showCentered(cx, ROSE_TEXT_Y, windSector.ok ? windSector.text : "n/a", 24);

  if (windDir.ok) {
    char deg[24];
    snprintf(deg, sizeof(deg), "aus %d Grad", (int)lroundf(windDir.value));
    showCentered(cx, ROSE_DEG_Y, deg, 16);
  }
}

static void drawWind() {
  const int cx = colCenter(1);
  showCentered(cx, HEAD_Y, "Wind", 16);

  if (!windSpeed.ok) {
    showCentered(cx, VALUE_Y, "n/a", 48);
    return;
  }
  char v[16];
  snprintf(v, sizeof(v), "%.1f", windSpeed.value);
  commaDecimal(v);
  showNumberCentered(cx, VALUE_Y, v, 48);
  showCentered(cx, UNIT_Y, "km/h", 24);
}

static void drawPressure() {
  const int cx = colCenter(2);
  showCentered(cx, HEAD_Y, "Luftdruck", 16);

  if (!pressure.ok) {
    showCentered(cx, VALUE_Y, "n/a", 48);
    return;
  }
  char v[16];
  snprintf(v, sizeof(v), "%.1f", pressure.value);
  commaDecimal(v);
  showNumberCentered(cx, VALUE_Y, v, 48);
  showCentered(cx, UNIT_Y, "hPa", 24);

  if (!pressure.hasRef) {
    showCentered(cx, EXTRA_Y, "kein Vergleich", 16);
    return;
  }
  const float d = pressure.value - pressure.ref;
  char t[24];
  // "%+.1f" macht aus -0,04 ein "-0,0" — ein Vorzeichen vor einer Null sieht
  // nach einem Fehler aus. Genau null bekommt deshalb gar keins.
  if (fabsf(d) < 0.05f) snprintf(t, sizeof(t), "0.0");
  else                  snprintf(t, sizeof(t), "%+.1f", d);
  commaDecimal(t);
  showNumberCentered(cx, EXTRA_Y, t, 24);

  char span[24];
  snprintf(span, sizeof(span), "seit %d h", TREND_HOURS);
  showCentered(cx, EXTRA_Y + 26, span, 16);
}

static void drawWeather() {
  const int cx = colCenter(3);
  showCentered(cx, HEAD_Y, "Wetter", 16);

  IconKind icon = ICON_CLOUD;
  const char* label = condition.ok ? condition.text : "n/a";
  if (condition.ok) {
    for (int i = 0; i < NCONDITION; i++)
      if (!strcmp(CONDITIONS[i].state, condition.text)) {
        icon  = CONDITIONS[i].icon;
        label = CONDITIONS[i].label;
        break;
      }
  }
  drawIcon(icon, cx, 118);
  showCentered(cx, EXTRA_Y + 6, label, 24);
}

static void drawFooter() {
  char stamp[48];
  struct tm lt;
  const time_t nowT = time(nullptr);
  localtime_r(&nowT, &lt);
  strftime(stamp, sizeof(stamp), "Stand %d.%m. %H:%M", &lt);
  EPD_ShowString(COL_X[0] + PAD_X, FOOTER_Y, stamp, FOOTER_SZ, BLACK);

  const char* src = "Wind und Wetter: Vorhersage - Luftdruck: Solarnode vor Ort";
  EPD_ShowString(FRAME_X1 - PAD_X - textWidth(src, FOOTER_SZ), FOOTER_Y,
                 src, FOOTER_SZ, BLACK);
}

static void drawScreen(bool full) {
  newFrame();
  drawGrid();
  drawDirection();
  drawWind();
  drawPressure();
  drawWeather();
  drawFooter();
  flush(full);
}

// ---------------------------------------------------------------------------

static bool update() {
  if (!connectWiFi(20000)) {
    showError("Kein WLAN", "SSID/Passwort in secrets.h pruefen");
    return false;
  }
  if (!waitForTime(20000)) {
    char detail[64];
    snprintf(detail, sizeof(detail), "NTP stumm - WLAN ok, IP %s",
             WiFi.localIP().toString().c_str());
    showError("Keine Zeit", detail);
    return false;
  }

  const time_t tEnd = time(nullptr);
  const time_t tRef = tEnd - (time_t)TREND_HOURS * 3600;

  int good = 0;
  if (fetchState(ENT_WIND_SPEED,  windSpeed))  good++;
  if (fetchState(ENT_WIND_DIR,    windDir))    good++;
  if (fetchState(ENT_WIND_SECTOR, windSector)) good++;
  if (fetchState(ENT_CONDITION,   condition))  good++;
  if (fetchTrend(ENT_PRESSURE, tRef, tEnd, pressure)) good++;

  // Einzelne Ausfaelle sind kein Grund, das ganze Bild wegzuwerfen — die
  // betroffene Spalte zeigt "n/a".
  if (good == 0) {
    showError("Keine Daten", "Kein Sensor lieferte Werte");
    return false;
  }
  Serial.printf("HA: %d von 5 Werten\n", good);

  const bool full = (updateCount % FULL_REFRESH_EVERY) == 0;
  Serial.printf("Panel: %s\n", full ? "Vollrefresh" : "Fast-Update");
  drawScreen(full);
  updateCount++;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n--- ha_wetter ---");

  pinMode(PIN_DISPLAY_POWER, OUTPUT);
  digitalWrite(PIN_DISPLAY_POWER, HIGH);   // ohne das bleibt das Panel dunkel

  connectWiFi(20000);
  waitForTime(20000);

  lastOk = update();
}

void loop() {
  delay(lastOk ? UPDATE_INTERVAL_MS : RETRY_INTERVAL_MS);
  lastOk = update();
}
