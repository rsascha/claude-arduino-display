// ha_raeume — Temperaturverlauf von sechs Sensoren in EINEM Diagramm:
// Wohnzimmer, Schlafzimmer, Badezimmer, Kueche, Flur und Aussen.
//
// Aufbau wie sketches/ha_verlauf, aber mit zwei Unterschieden, die sich aus
// der Mehrfachdarstellung ergeben:
//
//   * Auf einem Schwarz-Weiss-Panel gibt es keine Farben. Statt Strichmuster
//     — die bei sechs Kurven unruhig wirken — sind alle Kurven durchgezogen
//     und werden rechts neben ihrem Endpunkt beschriftet, zusammen mit dem
//     aktuellen Wert. Enden zwei Kurven zu dicht beieinander, rueckt die
//     Beschriftung aus dem Weg und eine kurze Linie zeigt, wohin sie gehoert.
//
//   * Die y-Achse ist fuer alle sechs Kurven gemeinsam. Aussen erreicht in der
//     Sonne knapp 40 Grad, innen liegt alles zwischen 20 und 28 — die
//     Innenkurven draengen sich dadurch im unteren Drittel. Das ist bewusst so:
//     der Kontrast zwischen drinnen und draussen ist gerade der interessante
//     Teil, und eine gekappte Aussenkurve waere irrefuehrend.
//
// Faellt ein einzelner Sensor aus, zeichnet der Sketch die uebrigen trotzdem
// und markiert den fehlenden in der Legende mit "n/a". Nur wenn ALLE
// ausfallen, erscheint ein Fehlerbild.
//
// Zugangsdaten: secrets.h (per .gitignore ausgeschlossen), Vorlage secrets.h.example

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <math.h>

#include "EPD.h"
#include "series.h"
#include "rooms.h"
#include "secrets.h"

uint8_t ImageBW[27200];

const int PIN_DISPLAY_POWER = 7;
const uint16_t DISPLAY_ROTATION = 0;

const int SCREEN_W = 792;    // sichtbar; der Puffer (EPD_W) ist 800 breit
const int SCREEN_H = 272;

const int HISTORY_DAYS = 10;                      // Purge-Grenze von HA
const unsigned long UPDATE_INTERVAL_MS = 30UL * 60UL * 1000UL;
// Nach einem Fehler nicht die vollen 30 Minuten warten: ein einmaliger
// NTP- oder WLAN-Aussetzer beim Start liesse das Panel sonst eine halbe
// Stunde lang nur die Fehlermeldung zeigen.
const unsigned long RETRY_INTERVAL_MS = 60UL * 1000UL;
const char* TZ_BERLIN = "CET-1CEST,M3.5.0,M10.5.0/3";

// Die entity_ids sind ueber friendly_name verifiziert, nicht geraten. Alle vier
// SONOFF-Sensoren tragen denselben Geraetenamen, unterschieden nur durch die
// Endung _2 / _3 — wer hier raet, landet im falschen Raum. Der Flur hat einen
// korrigierten Wert; der Rohwert des Motion-Sensors liegt rund 2,5 K daneben.
// "Aussen" ist der Solarnode: 20,8 bis 39,4 Grad in zehn Tagen, also draussen.
const RoomDef ROOMS[] = {
  { "sensor.wohnzimmer_temperatur_sonoff_snzb_02d_temperatur", "Wohnzimmer"   },
  { "sensor.temperatur_sonoff_snzb_02d_temperatur_2",          "Schlafz."     },
  { "sensor.temperatur_sonoff_snzb_02d_temperatur",            "Badezimmer"   },
  { "sensor.temperatur_sonoff_snzb_02d_temperatur_3",          "Kueche"       },
  { "sensor.flur_temperatur_korrigiert",                       "Flur"         },
  { "sensor.solarnode_temperatur",                             "Aussen"       },
};
const int NROOM = sizeof(ROOMS) / sizeof(ROOMS[0]);

// --- Layout -----------------------------------------------------------------
// Vertikal von oben: Titel 8..24, Plot 36..228, Datumsachse 233..249, Rahmen
// bei 269. Eine Textzeile braucht `size` Pixel Hoehe, nicht weniger — in
// ha_verlauf hatten sich zwei Zeilen um 6 px ueberlappt, weil das zu knapp
// gerechnet war.
//
// Rechts vom Diagramm bleiben 128 px fuer die Beschriftungen: "Wohnzimmer 22.9"
// ist der laengste Fall mit 15 Zeichen zu je 8 px, also 120 px. Zwischen
// Diagramm und Beschriftung liegen 18 px fuer die Fuehrungslinie.
//
// Die Legende und die Fusszeile mit min/max aus ha_verlauf entfallen beide —
// bei sechs Kurven waere ein einzelnes min/max mehrdeutig, und die gewonnene
// Hoehe kommt dem Diagramm zugute, das sie wegen der gemeinsamen y-Achse
// dringend braucht.
const int TITLE_Y   = 8;
const int TEXT_SZ   = 16;                         // auf dem Panel gut lesbar
const int PLOT_X0   = 62,  PLOT_X1 = 626;
const int LABEL_X   = PLOT_X1 + 18;               // 644; bis 772 sind es 128 px
const int PLOT_Y0   = 36,  PLOT_Y1 = 228;
const int NCOL = PLOT_X1 - PLOT_X0;               // 564 Pixelspalten

// Ein Wert je Pixelspalte und Raum. 6 * 710 * 4 Byte = 17 KB — unabhaengig
// davon, wie viele Messpunkte HA liefert.
float  colVal[NROOM][NCOL];
Series series[NROOM];
bool   ok[NROOM];
time_t tWindowStart = 0, tWindowEnd = 0;
bool   lastOk = false;

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

// Hat NTP eine plausible Zeit geliefert? Vor 2020 heisst: noch nicht gelaufen.
static bool timeIsSet() { return time(nullptr) >= 1600000000L; }

// Wartet auf NTP und stoesst die Synchronisation noetigenfalls neu an.
// getLocalTime() allein genuegt nicht: Ist der erste Versuch verstrichen,
// laeuft von selbst kein zweiter, und ohne Zeit gibt es keine History-Abfrage.
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

// Direkt mit strstr() scannen statt mit Arduino_JSON: 10 Tage History sind
// ~19 KB je Sensor, ein Objektbaum daraus braucht ein Vielfaches an RAM.
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

// ---------------------------------------------------------------------------
// Darstellung
// ---------------------------------------------------------------------------

// Beschriftet die Kurven rechts neben ihrem Endpunkt.
//
// Am rechten Rand liegen die fuenf Innenkurven oft nur wenige Pixel
// auseinander; uebereinandergeschriebene Namen waeren unlesbar. Deshalb werden
// die Beschriftungen von oben nach unten durchgegangen und bei Bedarf nach
// unten geschoben, bis jede ihren Platz hat. Eine kurze Linie verbindet sie
// danach mit dem tatsaechlichen Kurvenende, sonst ginge durch das Verschieben
// gerade die Zuordnung verloren, um die es hier geht.
static void drawLabels(Label* labels, int n) {
  // nach Kurvenende sortieren, oberste zuerst (Insertion Sort, n <= 6)
  for (int i = 1; i < n; i++) {
    Label key = labels[i];
    int j = i - 1;
    while (j >= 0 && labels[j].yEnd > key.yEnd) { labels[j + 1] = labels[j]; j--; }
    labels[j + 1] = key;
  }

  const int minGap = TEXT_SZ + 2;
  for (int i = 0; i < n; i++) {
    labels[i].y = labels[i].yEnd - TEXT_SZ / 2;
    if (i > 0 && labels[i].y < labels[i - 1].y + minGap)
      labels[i].y = labels[i - 1].y + minGap;
  }

  // Passt der Stapel nicht mehr unter den Rand, alles nach oben schieben.
  const int overflow = (n > 0) ? (labels[n - 1].y + TEXT_SZ) - PLOT_Y1 : 0;
  if (overflow > 0)
    for (int i = 0; i < n; i++) labels[i].y -= overflow;

  for (int i = 0; i < n; i++) {
    if (labels[i].y < PLOT_Y0) labels[i].y = PLOT_Y0;

    const int r = labels[i].room;
    char t[32];
    snprintf(t, sizeof(t), "%s %.1f", ROOMS[r].label, series[r].current);
    EPD_ShowString(LABEL_X, labels[i].y, t, TEXT_SZ, BLACK);

    // Fuehrungslinie vom Kurvenende zur Beschriftung
    safeLine(PLOT_X1 + 1, labels[i].yEnd,
             LABEL_X - 3, labels[i].y + TEXT_SZ / 2, BLACK);
  }
}

static void drawChart() {
  beginFrame();
  EPD_DrawRectangle(2, 2, SCREEN_W - 3, SCREEN_H - 3, BLACK, 0);

  // --- Gemeinsame y-Achse ueber ALLE Kurven ---
  float vmin = 0, vmax = 0;
  bool any = false;
  for (int i = 0; i < NROOM; i++) {
    if (!ok[i]) continue;
    if (!any) { vmin = series[i].vmin; vmax = series[i].vmax; any = true; }
    if (series[i].vmin < vmin) vmin = series[i].vmin;
    if (series[i].vmax > vmax) vmax = series[i].vmax;
  }
  int lo = (int)floorf(vmin) - 1;
  int hi = (int)ceilf(vmax) + 1;
  if (hi - lo < 4) hi = lo + 4;
  // Bei rund 20 Grad Spanne waeren Schritte von 1 Grad zu dicht; auf hoechstens
  // sechs Linien begrenzen, sonst wird das Gitter zur Flaeche.
  int stepY = (hi - lo + 5) / 6;
  if (stepY < 1) stepY = 1;

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
      EPD_ShowString(x - textWidth(d, TEXT_SZ) / 2, PLOT_Y1 + 5, d, TEXT_SZ, BLACK);
    }
    day += 2 * 86400;
  }

  EPD_DrawRectangle(PLOT_X0, PLOT_Y0, PLOT_X1, PLOT_Y1, BLACK, 0);

  // --- y-Beschriftung ---
  for (int v = lo; v <= hi; v += stepY) {
    const int y = PLOT_Y1 - (int)lroundf((float)(v - lo) / (hi - lo) * (PLOT_Y1 - PLOT_Y0));
    char t[8];
    snprintf(t, sizeof(t), "%d", v);
    EPD_ShowString(PLOT_X0 - 8 - textWidth(t, TEXT_SZ), y - TEXT_SZ / 2, t, TEXT_SZ, BLACK);
  }

  // --- Kurven ---
  Label labels[NROOM];
  int nLabel = 0;

  for (int i = 0; i < NROOM; i++) {
    if (!ok[i]) continue;
    const float* col = colVal[i];
    int lastX = -1, lastY = -1, lastC = -1;

    for (int c = 0; c < NCOL; c++) {
      if (isnan(col[c])) continue;
      const int x = PLOT_X0 + c;
      const int y = PLOT_Y1 - (int)lroundf((col[c] - lo) / (float)(hi - lo) * (PLOT_Y1 - PLOT_Y0));

      // Lange Luecken (fehlende Messwerte) nicht ueberbruecken: sonst entstuende
      // eine gerade Linie, die es so nie gab. Bei sechs Sensoren mit
      // unterschiedlichen Sendeintervallen faellt das eher an als bei einem.
      if (lastX >= 0 && c - lastC <= 8) {
        safeLine(lastX, lastY, x, y, BLACK);
        safeLine(lastX, lastY - 1, x, y - 1, BLACK);   // 2 px stark
      }
      lastX = x; lastY = y; lastC = c;
    }

    if (lastY >= 0) {
      labels[nLabel].room = i;
      labels[nLabel].yEnd = lastY;
      labels[nLabel].y    = lastY;
      nLabel++;
    }
  }

  drawLabels(labels, nLabel);

  // --- Kopfzeile ---
  // Die Einheit gehoert an den Titel, nicht hinter den Zeitstempel — dort
  // stand sie zunaechst, uebernommen aus ha_verlauf, wo sie einem Messwert
  // folgte. "Stand 16:05 °C" ergibt keinen Sinn.
  EPD_ShowString(PLOT_X0, TITLE_Y, "Temperaturen", TEXT_SZ, BLACK);
  int hx = PLOT_X0 + textWidth("Temperaturen", TEXT_SZ) + 10;
  drawDegreeSign(hx + 3, TITLE_Y + 4, 3, 2);
  EPD_ShowString(hx + 10, TITLE_Y, "C", TEXT_SZ, BLACK);

  char head[32];
  snprintf(head, sizeof(head), "%d Tage", HISTORY_DAYS);
  EPD_ShowString(hx + 34, TITLE_Y, head, TEXT_SZ, BLACK);

  char stamp[32];
  struct tm lt;
  time_t nowT = time(nullptr);
  localtime_r(&nowT, &lt);
  strftime(stamp, sizeof(stamp), "Stand %d.%m. %H:%M", &lt);
  EPD_ShowString(PLOT_X1 - textWidth(stamp, TEXT_SZ), TITLE_Y, stamp, TEXT_SZ, BLACK);

  endFrame();
}

// ---------------------------------------------------------------------------

static bool update() {
  String body, err;

  if (!connectWiFi(20000)) {
    showError("Kein WLAN", "SSID/Passwort in secrets.h pruefen");
    return false;
  }

  if (!waitForTime(20000)) {
    // Die Meldung nennt den WLAN-Zustand mit: Ist die IP da, liegt es nicht am
    // Netz, sondern daran, dass der NTP-Server nicht antwortet.
    char detail[64];
    snprintf(detail, sizeof(detail), "NTP stumm - WLAN ok, IP %s",
             WiFi.localIP().toString().c_str());
    showError("Keine Zeit", detail);
    return false;
  }

  const time_t tEnd   = time(nullptr);
  const time_t tStart = tEnd - (time_t)HISTORY_DAYS * 86400;
  tWindowStart = tStart;
  tWindowEnd   = tEnd;

  char s0[32], s1[32];
  formatIsoUtc(tStart, s0, sizeof(s0));
  formatIsoUtc(tEnd,   s1, sizeof(s1));

  int good = 0;
  for (int i = 0; i < NROOM; i++) {
    ok[i] = false;
    series[i] = Series();

    // Ohne end_time liefert HA nur EINEN Tag ab start_time, nicht bis jetzt.
    String path = String("/api/history/period/") + s0 +
                  "?filter_entity_id=" + ROOMS[i].entity +
                  "&end_time=" + s1 +
                  "&minimal_response&no_attributes";

    if (!haGet(path, body, err)) {
      Serial.printf("HA: %-14s FEHLER %s\n", ROOMS[i].label, err.c_str());
      continue;
    }
    if (!parseHistory(body, tStart, tEnd, series[i], colVal[i])) {
      Serial.printf("HA: %-14s keine Werte\n", ROOMS[i].label);
      continue;
    }
    ok[i] = true;
    good++;
    Serial.printf("HA: %-14s %4d Punkte, min %.1f, max %.1f, aktuell %.1f\n",
                  ROOMS[i].label, series[i].count, series[i].vmin,
                  series[i].vmax, series[i].current);
  }

  // Einzelne Ausfaelle sind kein Grund, das ganze Bild wegzuwerfen — die
  // fehlende Kurve steht in der Legende als "n/a".
  if (good == 0) {
    showError("Keine Daten", "Kein Sensor lieferte Werte");
    return false;
  }
  Serial.printf("HA: %d von %d Sensoren\n", good, NROOM);

  drawChart();
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n--- ha_raeume ---");

  pinMode(PIN_DISPLAY_POWER, OUTPUT);
  digitalWrite(PIN_DISPLAY_POWER, HIGH);   // ohne das bleibt das Panel dunkel

  connectWiFi(20000);
  waitForTime(20000);

  lastOk = update();
}

void loop() {
  // Nach einem Fehler zuegig erneut versuchen statt eine halbe Stunde warten.
  delay(lastOk ? UPDATE_INTERVAL_MS : RETRY_INTERVAL_MS);
  lastOk = update();
}
