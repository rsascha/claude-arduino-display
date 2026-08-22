// ha_verlauf — zeichnet den Temperaturverlauf aus Home Assistant als Kurve
// auf das CrowPanel 5.79" E-Paper (792x272).
//
// Datenquelle ist die REST-History-API. Die reicht nur so weit zurueck, wie
// HAs Recorder aufbewahrt (Standard: purge_keep_days = 10). Fuer laengere
// Zeitraeume braeuchte man die Langzeitstatistik, die es nur ueber WebSocket
// gibt — deutlich mehr Aufwand auf dem ESP32.
//
// Zugangsdaten: secrets.h (per .gitignore ausgeschlossen), Vorlage secrets.h.example

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <math.h>

#include "EPD.h"
#include "series.h"
#include "secrets.h"

uint8_t ImageBW[27200];

const int PIN_DISPLAY_POWER = 7;
const uint16_t DISPLAY_ROTATION = 0;

const int SCREEN_W = 792;    // sichtbar; der Puffer (EPD_W) ist 800 breit
const int SCREEN_H = 272;

const int HISTORY_DAYS = 10;                      // Purge-Grenze von HA
const unsigned long UPDATE_INTERVAL_MS = 30UL * 60UL * 1000UL;
const char* TZ_BERLIN = "CET-1CEST,M3.5.0,M10.5.0/3";

// Zeichenflaeche der Kurve
const int PLOT_X0 = 62,  PLOT_X1 = 772;
// PLOT_Y1 = 222 statt 230: darunter muessen ZWEI Textzeilen der Hoehe 16
// nebeneinander passen (Datum bei 227..243, Fusszeile bei 248..264) plus
// der Rahmen bei 269. Mit 230 ueberlappten sich Datum und Fusszeile um 6 px.
const int PLOT_Y0 = 46,  PLOT_Y1 = 222;
const int NCOL = PLOT_X1 - PLOT_X0;               // 710 Pixelspalten

// Ein Wert je Pixelspalte. So bleibt der Speicherbedarf konstant, egal wie
// viele Messpunkte HA liefert: 710 * 4 Byte = 2,8 KB.
float colVal[NCOL];

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

static void safeLine(int x0, int y0, int x1, int y1, uint16_t color) {
  // Bresenham selbst, damit die Bereichspruefung greift
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    safePixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

static void dottedH(int x0, int x1, int y, int gap) {
  for (int x = x0; x <= x1; x += gap) safePixel(x, y, BLACK);
}
static void dottedV(int x, int y0, int y1, int gap) {
  for (int y = y0; y <= y1; y += gap) safePixel(x, y, BLACK);
}

// Das Grad-Zeichen fehlt im Font (95 Eintraege, nur ASCII 32..126).
static void drawDegreeSign(int cx, int cy, int r, int thickness) {
  for (int t = 0; t < thickness; t++) EPD_DrawCircle(cx, cy, r + t, BLACK, 0);
}

static int textWidth(const char* s, int size) { return (int)strlen(s) * (size / 2); }

// EPD_ShowString() kann nur ASCII 32..126 — alles andere liest ueber das
// Font-Array hinaus. Umlaute aus HA-Namen also ersetzen.
static void toAscii(char* s) {
  for (; *s; s++) if (*s < 32 || *s > 126) *s = '?';
}

// ---------------------------------------------------------------------------
// Zeit
// ---------------------------------------------------------------------------

// Tage seit 1970-01-01 aus einem Kalenderdatum (Algorithmus von Howard Hinnant).
// Bewusst ohne timegm(), um Zeitzonen-Ueberraschungen auszuschliessen: die
// Zeitstempel von HA sind immer UTC.
static long daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2u) / 5u + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (long)era * 146097L + (long)doe - 719468L;
}

// "2026-08-12T02:14:36.406721+00:00" -> Unix-Zeit. 0 bei Formatfehler.
static time_t parseIsoUtc(const char* s) {
  int Y, M, D, h, m, sec;
  if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &Y, &M, &D, &h, &m, &sec) != 6) return 0;
  return (time_t)(daysFromCivil(Y, M, D) * 86400L + h * 3600L + m * 60L + sec);
}

static void formatIsoUtc(time_t t, char* out, size_t n) {
  struct tm g;
  gmtime_r(&t, &g);
  // 'Z' statt '+00:00': ein unkodiertes '+' wird im Query-String zum
  // Leerzeichen, HA antwortet dann mit "Invalid end_time".
  strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &g);
}

// ---------------------------------------------------------------------------
// Netzwerk
// ---------------------------------------------------------------------------

static bool connectWiFi(unsigned long timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) return true;
  Serial.printf("WLAN: verbinde mit '%s' ...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  const unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) delay(250);
  if (WiFi.status() != WL_CONNECTED) { Serial.println("WLAN: fehlgeschlagen."); return false; }
  Serial.printf("WLAN: verbunden, IP %s, RSSI %d dBm\n",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
  return true;
}

static bool haGet(const String& path, String& out, String& errorOut) {
  WiFiClient client;
  HTTPClient http;
  const String url = String("http://") + HA_HOST + ":" + String(HA_PORT) + path;
  if (!http.begin(client, url)) { errorOut = "http.begin fehlgeschlagen"; return false; }
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

// ---------------------------------------------------------------------------
// History einlesen
// ---------------------------------------------------------------------------

// Die Antwort ist ~19 KB gross. Arduino_JSON wuerde daraus einen Objektbaum
// bauen, der ein Vielfaches davon an RAM belegt — deshalb direkt im Text
// nach den beiden Feldern suchen, die uns interessieren.
static bool parseHistory(const String& payload, time_t tStart, time_t tEnd, Series& s) {
  for (int i = 0; i < NCOL; i++) colVal[i] = NAN;

  const char* p = payload.c_str();
  const long window = (long)(tEnd - tStart);
  if (window <= 0) return false;

  bool first = true;
  while ((p = strstr(p, "\"state\":\"")) != nullptr) {
    p += 9;
    const char* valueStart = p;
    const float v = atof(p);

    const char* lc = strstr(p, "\"last_changed\":\"");
    if (!lc) break;
    lc += 16;
    const time_t t = parseIsoUtc(lc);
    p = lc;

    if (t == 0) continue;

    // 'unavailable' und 'unknown' sind gueltige HA-Zustaende. atof() macht
    // daraus 0.0 — das wuerde die Kurve auf 0 Grad reissen. Deshalb pruefen,
    // ob der Wert ueberhaupt mit einer Ziffer oder '-' beginnt.
    const char* vs = valueStart;
    if (!(isdigit((unsigned char)*vs) || (*vs == '-' && isdigit((unsigned char)vs[1])))) continue;

    int c = (int)(((long)(t - tStart) * (long)NCOL) / window);
    if (c < 0 || c >= NCOL) continue;

    colVal[c] = isnan(colVal[c]) ? v : (colVal[c] + v) * 0.5f;

    if (first) { s.vmin = s.vmax = v; s.tFirst = t; first = false; }
    if (v < s.vmin) s.vmin = v;
    if (v > s.vmax) s.vmax = v;
    s.tLast = t;
    s.current = v;
    s.count++;
  }
  return s.count >= 2;
}

// ---------------------------------------------------------------------------
// Darstellung
// ---------------------------------------------------------------------------

static void beginFrame() {
  EPD_GPIOInit();
  Paint_NewImage(ImageBW, EPD_W, EPD_H, DISPLAY_ROTATION, WHITE);
  Paint_Clear(WHITE);
  EPD_FastMode1Init();
  EPD_Display_Clear();
  EPD_Update();
  EPD_GPIOInit();
  EPD_FastMode1Init();
}

static void endFrame() { EPD_Display(ImageBW); EPD_FastUpdate(); }

static void showError(const char* headline, const char* detail) {
  beginFrame();
  EPD_DrawRectangle(4, 4, SCREEN_W - 5, SCREEN_H - 5, BLACK, 0);
  EPD_ShowString(50,  70, headline, 48, BLACK);
  EPD_ShowString(50, 140, detail,   24, BLACK);
  EPD_ShowString(50, 190, "Details per: make monitor", 16, BLACK);
  endFrame();
}

static void drawChart(const char* label, const Series& s, time_t tStart, time_t tEnd) {
  beginFrame();
  EPD_DrawRectangle(2, 2, SCREEN_W - 3, SCREEN_H - 3, BLACK, 0);

  // --- y-Achse auf ganze Grad runden, mindestens 4 Grad Spanne ---
  int lo = (int)floorf(s.vmin) - 1;
  int hi = (int)ceilf(s.vmax) + 1;
  if (hi - lo < 4) hi = lo + 4;
  const int stepY = max(1, (int)lroundf((hi - lo) / 4.0f));

  // --- Gitter (zuerst, damit Text es spaeter ueberdeckt) ---
  for (int v = lo; v <= hi; v += stepY) {
    const int y = PLOT_Y1 - (int)lroundf((float)(v - lo) / (hi - lo) * (PLOT_Y1 - PLOT_Y0));
    dottedH(PLOT_X0, PLOT_X1, y, 6);
  }

  // Tagesmarken in lokaler Zeit
  struct tm tmv;
  time_t day = tStart;
  localtime_r(&day, &tmv);
  tmv.tm_hour = 0; tmv.tm_min = 0; tmv.tm_sec = 0; tmv.tm_mday += 1;
  day = mktime(&tmv);
  while (day < tEnd) {
    const int x = PLOT_X0 + (int)(((long)(day - tStart) * (PLOT_X1 - PLOT_X0)) / (long)(tEnd - tStart));
    if (x > PLOT_X0 && x < PLOT_X1) {
      dottedV(x, PLOT_Y0, PLOT_Y1, 8);
      char d[8];
      localtime_r(&day, &tmv);
      strftime(d, sizeof(d), "%d.%m.", &tmv);
      EPD_ShowString(x - textWidth(d, 16) / 2, PLOT_Y1 + 5, d, 16, BLACK);
    }
    day += 2 * 86400;
  }

  // --- Rahmen der Zeichenflaeche ---
  EPD_DrawRectangle(PLOT_X0, PLOT_Y0, PLOT_X1, PLOT_Y1, BLACK, 0);

  // --- y-Beschriftung ---
  for (int v = lo; v <= hi; v += stepY) {
    const int y = PLOT_Y1 - (int)lroundf((float)(v - lo) / (hi - lo) * (PLOT_Y1 - PLOT_Y0));
    char t[8];
    snprintf(t, sizeof(t), "%d", v);
    EPD_ShowString(PLOT_X0 - 8 - textWidth(t, 16), y - 8, t, 16, BLACK);
  }

  // --- Kurve: aufeinanderfolgende belegte Spalten verbinden ---
  int lastX = -1, lastY = -1;
  for (int c = 0; c < NCOL; c++) {
    if (isnan(colVal[c])) continue;
    const int x = PLOT_X0 + c;
    const int y = PLOT_Y1 - (int)lroundf((colVal[c] - lo) / (float)(hi - lo) * (PLOT_Y1 - PLOT_Y0));
    if (lastX >= 0) {
      safeLine(lastX, lastY, x, y, BLACK);
      safeLine(lastX, lastY - 1, x, y - 1, BLACK);   // 2 px stark
    }
    lastX = x; lastY = y;
  }

  // --- Kopfzeile ---
  EPD_ShowString(PLOT_X0, 16, label, 24, BLACK);

  char now[16];
  snprintf(now, sizeof(now), "%.1f", s.current);
  int x = PLOT_X1 - textWidth(now, 24) - 34;
  EPD_ShowString(x, 16, now, 24, BLACK);
  drawDegreeSign(x + textWidth(now, 24) + 10, 21, 4, 2);
  EPD_ShowString(x + textWidth(now, 24) + 20, 16, "C", 24, BLACK);

  // --- Fusszeile ---
  char foot[80];
  struct tm lt;
  time_t nowT = time(nullptr);
  localtime_r(&nowT, &lt);
  char stamp[24];
  strftime(stamp, sizeof(stamp), "%d.%m. %H:%M", &lt);
  snprintf(foot, sizeof(foot), "%d Tage   min %.1f   max %.1f   %d Werte   Stand %s",
           HISTORY_DAYS, s.vmin, s.vmax, s.count, stamp);
  EPD_ShowString(PLOT_X0, SCREEN_H - 24, foot, 16, BLACK);

  endFrame();
}

// ---------------------------------------------------------------------------

static void update() {
  String body, err;

  if (!connectWiFi(20000)) {
    showError("Kein WLAN", "SSID/Passwort in secrets.h pruefen");
    return;
  }

  // Anzeigename holen
  char label[48];
  snprintf(label, sizeof(label), "%s", HA_ENTITY_ID);
  if (haGet(String("/api/states/") + HA_ENTITY_ID, body, err)) {
    const int k = body.indexOf("\"friendly_name\":\"");
    if (k >= 0) {
      const int a = k + 17, b = body.indexOf('"', a);
      if (b > a) snprintf(label, sizeof(label), "%s", body.substring(a, b).c_str());
    }
  }
  toAscii(label);

  // Zeitfenster
  const time_t tEnd   = time(nullptr);
  const time_t tStart = tEnd - (time_t)HISTORY_DAYS * 86400;
  if (tEnd < 1600000000L) {           // NTP noch nicht gelaufen
    showError("Keine Zeit", "NTP nicht erreichbar");
    return;
  }
  char s0[32], s1[32];
  formatIsoUtc(tStart, s0, sizeof(s0));
  formatIsoUtc(tEnd,   s1, sizeof(s1));

  // Ohne end_time liefert HA nur EINEN Tag ab start_time, nicht bis jetzt.
  String path = String("/api/history/period/") + s0 +
                "?filter_entity_id=" + HA_ENTITY_ID +
                "&end_time=" + s1 +
                "&minimal_response&no_attributes";

  Serial.printf("HA: GET %s\n", path.c_str());
  if (!haGet(path, body, err)) {
    showError("Fehler", err.c_str());
    return;
  }
  Serial.printf("HA: %u Bytes empfangen\n", (unsigned)body.length());

  Series s;
  if (!parseHistory(body, tStart, tEnd, s)) {
    showError("Keine Daten", "History leer - purge_keep_days?");
    return;
  }
  Serial.printf("HA: %d Punkte, min %.1f, max %.1f, aktuell %.1f\n",
                s.count, s.vmin, s.vmax, s.current);

  drawChart(label, s, tStart, tEnd);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n--- ha_verlauf ---");

  pinMode(PIN_DISPLAY_POWER, OUTPUT);
  digitalWrite(PIN_DISPLAY_POWER, HIGH);   // ohne das bleibt das Panel dunkel

  connectWiFi(20000);
  configTzTime(TZ_BERLIN, "pool.ntp.org", "time.nist.gov");
  struct tm t;
  getLocalTime(&t, 10000);                 // auf NTP warten

  update();
}

void loop() {
  delay(UPDATE_INTERVAL_MS);
  update();
}
