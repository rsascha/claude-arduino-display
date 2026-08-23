// ha_umschalten — blendet im 5-Sekunden-Takt zwischen zwei Temperaturkurven um
// (Wohnzimmer / Schlafzimmer) und wechselt dabei reihum das Refresh-Verfahren
// des Panels durch. Zweck ist der Vergleich: wie sieht ein Bildwechsel auf
// E-Paper in Voll-, Fast- und Partial-Refresh aus, und wie lange dauert er?
//
// Der jeweils verwendete Modus und die gemessene Dauer stehen unten rechts auf
// dem Display. Die Dauer ist die des VORHERIGEN Wechsels — das aktuelle Bild
// ist zum Zeitpunkt des Zeichnens noch nicht geschrieben.
//
// Beide Verlaeufe werden EINMAL geholt und im Speicher gehalten; das Umschalten
// loest keine HTTP-Anfrage aus. Sonst wuerde die Netzwerklatenz messen, was hier
// das Panel messen soll. Aufgefrischt wird alle 30 Minuten.
//
// Basis ist sketches/ha_verlauf; Zeichencode und Parser sind von dort uebernommen.
//
// Zugangsdaten: secrets.h (per .gitignore ausgeschlossen), Vorlage secrets.h.example

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <math.h>

#include "EPD.h"
#include "series.h"
#include "screens.h"
#include "secrets.h"

uint8_t ImageBW[27200];

const int PIN_DISPLAY_POWER = 7;
const uint16_t DISPLAY_ROTATION = 0;

const int SCREEN_W = 792;    // sichtbar; der Puffer (EPD_W) ist 800 breit
const int SCREEN_H = 272;

const int HISTORY_DAYS = 10;                      // Purge-Grenze von HA
const char* TZ_BERLIN = "CET-1CEST,M3.5.0,M10.5.0/3";

// --- Test-Parameter ---------------------------------------------------------
const unsigned long HOLD_MS       = 5000;         // Standzeit je Bild
const int           SWITCHES_PER_MODE = 4;        // Wechsel, bevor der Modus wechselt
const unsigned long REFETCH_MS    = 30UL * 60UL * 1000UL;

// Die entity_ids sind ueber friendly_name verifiziert, nicht geraten: die
// Endung _2 beim Schlafzimmer entsteht daraus, dass alle vier SONOFF-Sensoren
// denselben Geraetenamen tragen. Wer hier raet, landet im Badezimmer.
const SensorDef SENSORS[] = {
  { "sensor.wohnzimmer_temperatur_sonoff_snzb_02d_temperatur", "Wohnzimmer"   },
  { "sensor.temperatur_sonoff_snzb_02d_temperatur_2",          "Schlafzimmer" },
};
const int NSENSOR = sizeof(SENSORS) / sizeof(SENSORS[0]);

const RefreshMode MODES[] = {
  { REFRESH_FULL, "Voll" },
  { REFRESH_FAST, "Fast" },
  { REFRESH_PART, "Part" },
};
const int NMODE = sizeof(MODES) / sizeof(MODES[0]);

// Zeichenflaeche der Kurve
const int PLOT_X0 = 62,  PLOT_X1 = 772;
// PLOT_Y1 = 222 statt 230: darunter muessen ZWEI Textzeilen der Hoehe 16
// nebeneinander passen (Datum bei 227..243, Fusszeile bei 248..264) plus
// der Rahmen bei 269. Mit 230 ueberlappten sich Datum und Fusszeile um 6 px.
const int PLOT_Y0 = 46,  PLOT_Y1 = 222;
const int NCOL = PLOT_X1 - PLOT_X0;               // 710 Pixelspalten

// Ein Wert je Pixelspalte, fuer jeden Sensor einer. 2 * 710 * 4 Byte = 5,7 KB —
// unabhaengig davon, wie viele Messpunkte HA liefert.
float  colVal[2][NCOL];
Series series[2];
char   labels[2][48];
time_t tWindowStart = 0, tWindowEnd = 0;
unsigned long lastFetch = 0;

// ---------------------------------------------------------------------------
// Zeichenhilfen  (uebernommen aus ha_verlauf)
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
    const int e2 = 2 * err;
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

// Es gibt keine Kreis-mit-Strichstaerke-Funktion; mehrere Radien uebereinander.
static void drawDegreeSign(int cx, int cy, int r, int thickness) {
  for (int t = 0; t < thickness; t++) EPD_DrawCircle(cx, cy, r + t, BLACK, 0);
}

// EPD_ShowString() bricht nicht um; Zeichenbreite ist size/2.
static int textWidth(const char* s, int size) { return (int)strlen(s) * (size / 2); }

// Die Font-Arrays haben 95 Eintraege fuer ASCII 32..126, Index ist chr - ' '.
// Ein Umlaut (>126) liest ueber das Array hinaus.
static void toAscii(char* s) {
  for (char* p = s; *p; p++) {
    const unsigned char c = (unsigned char)*p;
    if (c < 32 || c > 126) *p = '?';
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

// "2026-08-13T20:15:03.123456+00:00" -> time_t (UTC)
static time_t parseIsoUtc(const char* s) {
  int Y, M, D, h, mi, se;
  if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &Y, &M, &D, &h, &mi, &se) != 6) return 0;
  return (time_t)(daysFromCivil(Y, M, D) * 86400L + h * 3600L + mi * 60L + se);
}

// HA antwortet auf '+00:00' mit "Invalid end_time": das '+' wird im Query-String
// zum Leerzeichen dekodiert. Deshalb 'Z'.
static void formatIsoUtc(time_t t, char* out, size_t n) {
  struct tm g;
  gmtime_r(&t, &g);
  strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &g);
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
    // Fehler gehoeren auf das Display, nicht nur ins Log — sonst sieht man bei
    // einem Problem nur ein leeres Panel.
    if (code == HTTP_CODE_UNAUTHORIZED)  errorOut += " (Token?)";
    if (code == HTTP_CODE_NOT_FOUND)     errorOut += " (entity_id?)";
    http.end();
    return false;
  }
  out = http.getString();
  http.end();
  return true;
}

// Direkt mit strstr() scannen statt mit Arduino_JSON: 10 Tage History sind
// ~19 KB, ein Objektbaum daraus braucht ein Vielfaches an RAM.
static bool parseHistory(const String& payload, time_t tStart, time_t tEnd,
                         Series& s, float* col) {
  for (int i = 0; i < NCOL; i++) col[i] = NAN;

  const char* p = payload.c_str();
  const time_t span = tEnd - tStart;
  if (span <= 0) return false;

  bool first = true;
  while ((p = strstr(p, "\"state\":\"")) != nullptr) {
    p += 9;
    const char* stateEnd = strchr(p, '"');
    if (!stateEnd) break;

    // 'unavailable' und 'unknown' sind gueltige Zustaende, keine Fehler.
    // atof() macht daraus 0.0 und reisst die Kurve auf 0 Grad herunter.
    const bool numeric = (*p == '-' || (*p >= '0' && *p <= '9'));
    const float v = numeric ? atof(p) : NAN;

    const char* lc = strstr(stateEnd, "\"last_changed\":\"");
    if (!lc) break;
    lc += 16;
    const time_t t = parseIsoUtc(lc);
    p = lc;

    if (isnan(v) || t < tStart || t > tEnd) continue;

    const int c = (int)(((long)(t - tStart) * NCOL) / (long)span);
    if (c < 0 || c >= NCOL) continue;
    col[c] = v;

    if (first) { s.vmin = s.vmax = v; s.tFirst = t; first = false; }
    if (v < s.vmin) s.vmin = v;
    if (v > s.vmax) s.vmax = v;
    s.tLast   = t;
    s.current = v;
    s.count++;
  }
  return s.count > 0;
}

// ---------------------------------------------------------------------------
// Panel
// ---------------------------------------------------------------------------

static void panelInit() {
  EPD_GPIOInit();
  EPD_FastMode1Init();     // enthaelt den Hardware-Reset
}

// Schreibt den Puffer aufs Panel und misst, wie lange das dauert.
// Alle drei Modi bekommen dieselbe Init, damit sich der Vergleich sauber auf
// zwei Dinge beschraenkt: Loeschzyklus ja/nein und der Parameter zu 0x22.
static unsigned long flush(RefreshKind kind) {
  const unsigned long t0 = millis();
  switch (kind) {
    case REFRESH_FULL:
      panelInit();
      EPD_Display_Clear();
      EPD_Update();
      panelInit();
      EPD_Display(ImageBW);
      EPD_Update();
      break;
    case REFRESH_FAST:
      panelInit();
      EPD_Display(ImageBW);
      EPD_FastUpdate();
      break;
    case REFRESH_PART:
      panelInit();
      EPD_Display(ImageBW);
      EPD_PartUpdate();
      break;
  }
  return millis() - t0;
}

static void newFrame() {
  Paint_NewImage(ImageBW, EPD_W, EPD_H, DISPLAY_ROTATION, WHITE);
  Paint_Clear(WHITE);
}

static void showError(const char* headline, const char* detail) {
  newFrame();
  EPD_DrawRectangle(4, 4, SCREEN_W - 5, SCREEN_H - 5, BLACK, 0);
  EPD_ShowString(50,  70, headline, 48, BLACK);
  EPD_ShowString(50, 140, detail,   24, BLACK);
  EPD_ShowString(50, 190, "Details per: make monitor", 16, BLACK);
  flush(REFRESH_FULL);
}

// ---------------------------------------------------------------------------
// Darstellung
// ---------------------------------------------------------------------------

// Zeichnet nur in den Puffer; geschrieben wird erst durch flush().
static void renderChart(int idx, const RefreshMode& mode, int shot, unsigned long lastMs) {
  const Series& s = series[idx];
  const float* col = colVal[idx];

  newFrame();
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
  time_t day = tWindowStart;
  localtime_r(&day, &tmv);
  tmv.tm_hour = 0; tmv.tm_min = 0; tmv.tm_sec = 0; tmv.tm_mday += 1;
  day = mktime(&tmv);
  while (day < tWindowEnd) {
    const int x = PLOT_X0 + (int)(((long)(day - tWindowStart) * (PLOT_X1 - PLOT_X0))
                                  / (long)(tWindowEnd - tWindowStart));
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
    if (isnan(col[c])) continue;
    const int x = PLOT_X0 + c;
    const int y = PLOT_Y1 - (int)lroundf((col[c] - lo) / (float)(hi - lo) * (PLOT_Y1 - PLOT_Y0));
    if (lastX >= 0) {
      safeLine(lastX, lastY, x, y, BLACK);
      safeLine(lastX, lastY - 1, x, y - 1, BLACK);   // 2 px stark
    }
    lastX = x; lastY = y;
  }

  // --- Kopfzeile ---
  EPD_ShowString(PLOT_X0, 16, labels[idx], 24, BLACK);

  char now[16];
  snprintf(now, sizeof(now), "%.1f", s.current);
  int x = PLOT_X1 - textWidth(now, 24) - 34;
  EPD_ShowString(x, 16, now, 24, BLACK);
  drawDegreeSign(x + textWidth(now, 24) + 10, 21, 4, 2);
  EPD_ShowString(x + textWidth(now, 24) + 20, 16, "C", 24, BLACK);

  // --- Fusszeile links: Kennzahlen der Kurve ---
  char foot[80];
  struct tm lt;
  time_t nowT = time(nullptr);
  localtime_r(&nowT, &lt);
  char stamp[16];
  strftime(stamp, sizeof(stamp), "%H:%M", &lt);
  snprintf(foot, sizeof(foot), "%d Tage  min %.1f  max %.1f  %d Werte  %s",
           HISTORY_DAYS, s.vmin, s.vmax, s.count, stamp);
  EPD_ShowString(PLOT_X0, SCREEN_H - 24, foot, 16, BLACK);

  // --- Fusszeile rechts: der eigentliche Testbefund ---
  // Die Dauer ist die des vorherigen Wechsels; die des aktuellen steht erst
  // fest, wenn das Bild bereits geschrieben ist.
  char info[48];
  if (lastMs > 0) {
    snprintf(info, sizeof(info), "%s %d/%d  vorher %lu ms",
             mode.name, shot, SWITCHES_PER_MODE, lastMs);
  } else {
    snprintf(info, sizeof(info), "%s %d/%d", mode.name, shot, SWITCHES_PER_MODE);
  }
  EPD_ShowString(PLOT_X1 - textWidth(info, 16), SCREEN_H - 24, info, 16, BLACK);
}

// ---------------------------------------------------------------------------
// Daten holen
// ---------------------------------------------------------------------------

static bool fetchAll() {
  String body, err;

  if (!connectWiFi(20000)) {
    showError("Kein WLAN", "SSID/Passwort in secrets.h pruefen");
    return false;
  }

  const time_t tEnd   = time(nullptr);
  const time_t tStart = tEnd - (time_t)HISTORY_DAYS * 86400;
  if (tEnd < 1600000000L) {           // NTP noch nicht gelaufen
    showError("Keine Zeit", "NTP nicht erreichbar");
    return false;
  }
  tWindowStart = tStart;
  tWindowEnd   = tEnd;

  char s0[32], s1[32];
  formatIsoUtc(tStart, s0, sizeof(s0));
  formatIsoUtc(tEnd,   s1, sizeof(s1));

  for (int i = 0; i < NSENSOR; i++) {
    // Anzeigename holen
    snprintf(labels[i], sizeof(labels[i]), "%s", SENSORS[i].fallback);
    if (haGet(String("/api/states/") + SENSORS[i].entity, body, err)) {
      const int k = body.indexOf("\"friendly_name\":\"");
      if (k >= 0) {
        const int a = k + 17, b = body.indexOf('"', a);
        if (b > a) snprintf(labels[i], sizeof(labels[i]), "%s", body.substring(a, b).c_str());
      }
    }
    toAscii(labels[i]);

    // Ohne end_time liefert HA nur EINEN Tag ab start_time, nicht bis jetzt.
    String path = String("/api/history/period/") + s0 +
                  "?filter_entity_id=" + SENSORS[i].entity +
                  "&end_time=" + s1 +
                  "&minimal_response&no_attributes";

    Serial.printf("HA: GET %s\n", path.c_str());
    if (!haGet(path, body, err)) {
      showError("Fehler", err.c_str());
      return false;
    }
    Serial.printf("HA: %u Bytes fuer %s\n", (unsigned)body.length(), SENSORS[i].entity);

    series[i] = Series();
    if (!parseHistory(body, tStart, tEnd, series[i], colVal[i])) {
      char detail[64];
      snprintf(detail, sizeof(detail), "%s ohne Werte", SENSORS[i].fallback);
      showError("Keine Daten", detail);
      return false;
    }
    Serial.printf("HA: %-14s %d Punkte, min %.1f, max %.1f, aktuell %.1f\n",
                  labels[i], series[i].count, series[i].vmin, series[i].vmax,
                  series[i].current);
  }
  return true;
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n--- ha_umschalten ---");

  pinMode(PIN_DISPLAY_POWER, OUTPUT);
  digitalWrite(PIN_DISPLAY_POWER, HIGH);   // ohne das bleibt das Panel dunkel

  connectWiFi(20000);
  configTzTime(TZ_BERLIN, "pool.ntp.org", "time.nist.gov");
  struct tm t;
  getLocalTime(&t, 10000);                 // auf NTP warten

  if (fetchAll()) lastFetch = millis();
}

void loop() {
  static int  sensorIdx = 0;
  static int  modeIdx   = 0;
  static int  shot      = 0;
  static unsigned long lastMs = 0;

  if (millis() - lastFetch > REFETCH_MS) {
    if (fetchAll()) lastFetch = millis();
  }

  // Ohne Daten fuer BEIDE Sensoren gibt es nichts umzuschalten. fetchAll() hat
  // den Grund bereits auf das Display geschrieben; hier nur zuegig neu
  // versuchen, statt bis zum naechsten regulaeren Abruf zu warten.
  if (series[0].count == 0 || series[1].count == 0) {
    delay(HOLD_MS);
    if (fetchAll()) lastFetch = millis();
    return;
  }

  shot++;
  const RefreshMode& mode = MODES[modeIdx];

  renderChart(sensorIdx, mode, shot, lastMs);
  lastMs = flush(mode.kind);

  Serial.printf("%-12s  %-4s  Wechsel %d/%d  %lu ms\n",
                labels[sensorIdx], mode.name, shot, SWITCHES_PER_MODE, lastMs);

  sensorIdx = (sensorIdx + 1) % NSENSOR;
  if (shot >= SWITCHES_PER_MODE) {
    shot = 0;
    modeIdx = (modeIdx + 1) % NMODE;
    Serial.printf("--- Modus jetzt: %s ---\n", MODES[modeIdx].name);
  }

  delay(HOLD_MS);
}
