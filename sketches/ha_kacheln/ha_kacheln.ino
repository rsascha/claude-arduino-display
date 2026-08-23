// ha_kacheln — alle Raumtemperaturen plus Aussen als Kacheln, ohne Verlauf.
//
// Sechs Kacheln in einem 3x2-Raster, sortiert von warm nach kalt in
// Lesereihenfolge: oben links der waermste Raum, unten rechts der kaelteste.
// Jede Kachel zeigt Namen, aktuellen Wert in Schriftgroesse 48 und den Trend
// gegenueber dem Stand von vor drei Stunden — als Pfeil und als Zahl.
//
// Warum so:
//
//   * Drei Stunden als Vergleichszeitraum. Die SONOFF-Sensoren loesen 0.1 K
//     auf; ueber eine Stunde bewegt sich ein geschlossener Raum oft nur um
//     genau diesen einen Schritt, und dann zeigt der Pfeil Rauschen an. Ueber
//     drei Stunden ist ein geoeffnetes Fenster deutlich zu sehen, der
//     Tagesgang draussen aber noch nicht dominant.
//
//   * Pfeil UND Zahl. Der Pfeil ist quer durchs Zimmer lesbar, die Zahl sagt,
//     ob es um 0.3 oder 3 Grad geht. Der Pfeil ist selbst gezeichnet: die
//     Font-Arrays decken nur ASCII 32..126 ab, ein Pfeilzeichen gibt es nicht.
//
//   * Eine einzige History-Abfrage je Raum liefert beides. Der letzte Wert der
//     Antwort ist der aktuelle Stand, der letzte Wert vor dem Referenzzeitpunkt
//     der Vergleich — ein zusaetzliches /api/states/<id> waere eine zweite
//     Anfrage fuer eine Zahl, die schon da ist.
//
//   * Alle zehn Minuten EPD_FastUpdate(), jeder sechste Durchgang mit
//     Loeschzyklus. Der Vergleich in ha_umschalten hat gezeigt: Fast-Update ist
//     der Regelfall, der Vollrefresh dient nur dazu, Ghosting einzusammeln.
//
// Faellt ein einzelner Sensor aus, zeigt seine Kachel "n/a" und wandert ans
// Ende der Sortierung. Nur wenn ALLE ausfallen, erscheint ein Fehlerbild.
//
// Zugangsdaten: secrets.h (per .gitignore ausgeschlossen), Vorlage secrets.h.example

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <math.h>

#include "EPD.h"
#include "tiles.h"
#include "secrets.h"

uint8_t ImageBW[27200];

const int PIN_DISPLAY_POWER = 7;
const uint16_t DISPLAY_ROTATION = 0;

const int SCREEN_W = 792;    // sichtbar; der Puffer (EPD_W) ist 800 breit
const int SCREEN_H = 272;

const int   TREND_HOURS = 3;
const float TREND_FLAT  = 0.2f;   // darunter gilt es als unveraendert

const unsigned long UPDATE_INTERVAL_MS = 10UL * 60UL * 1000UL;
// Nach einem Fehler nicht die vollen zehn Minuten warten: ein einmaliger
// NTP- oder WLAN-Aussetzer beim Start liesse das Panel sonst lange nur die
// Fehlermeldung zeigen.
const unsigned long RETRY_INTERVAL_MS = 60UL * 1000UL;
// Jeder sechste Durchgang mit Loeschzyklus, bei zehn Minuten Takt also
// stuendlich. Fast-Update allein laesst mit der Zeit Schatten alter Ziffern
// stehen; ein Vollrefresh bei JEDER Aktualisierung liesse das Panel dagegen
// sechsmal pro Stunde mehrfach komplett schwarz werden.
const int FULL_REFRESH_EVERY = 6;

const char* TZ_BERLIN = "CET-1CEST,M3.5.0,M10.5.0/3";

// Die entity_ids sind ueber friendly_name verifiziert, nicht geraten. Alle vier
// SONOFF-Sensoren tragen denselben Geraetenamen, unterschieden nur durch die
// Endung _2 / _3 — wer hier raet, landet im falschen Raum. Der Flur hat einen
// korrigierten Wert; der Rohwert des Motion-Sensors liegt rund 2,5 K daneben.
// "Aussen" ist der Solarnode.
const RoomDef ROOMS[] = {
  { "sensor.wohnzimmer_temperatur_sonoff_snzb_02d_temperatur", "Wohnzimmer"   },
  { "sensor.temperatur_sonoff_snzb_02d_temperatur_2",          "Schlafzimmer" },
  { "sensor.temperatur_sonoff_snzb_02d_temperatur",            "Badezimmer"   },
  { "sensor.temperatur_sonoff_snzb_02d_temperatur_3",          "Kueche"       },
  { "sensor.flur_temperatur_korrigiert",                       "Flur"         },
  { "sensor.solarnode_temperatur",                             "Aussen"       },
};
const int NROOM = sizeof(ROOMS) / sizeof(ROOMS[0]);

// --- Layout -----------------------------------------------------------------
// Das Raster teilt die Flaeche zwischen dem 2 px starken Aussenrahmen in drei
// Spalten und zwei Zeilen; unten bleibt ein Streifen fuer die Fusszeile.
//
// Eine Kachel ist 262 x 122 px gross und enthaelt zwei Zeilen: den Namen und
// darunter den Wert. Der Trend steht RECHTS neben dem Wert, nicht darunter —
// als dritte Zeile kostete er 30 px Hoehe, neben dem Wert kostet er nichts:
// "22,9 C" belegt 148 px, in den restlichen 94 px der Kachel hat der Trend
// bequem Platz. Die gewonnene Hoehe geht an Name und Trend, die dadurch von
// Groesse 16 auf 24 wachsen konnten.
//
// Der Wert selbst bleibt bei 48 — das ist die groesste Groesse, die der Font
// kennt (12, 16, 24, 48 und sonst nichts, siehe EPD_ShowChar()).
//
// Breitester Fall ist eine zweistellige Minustemperatur: "-12,3 C" belegt
// 4*24 + 12 (enges Komma) + 40 + 24 = 172 px, der Trendblock hoechstens 74 px,
// zusammen 246 px = genau der Inhaltsbreite einer Kachel. Enger wird es nicht.
//
// Vertikal braucht jede Textzeile `size` Pixel Hoehe, nicht weniger:
// 24 (Name) + 12 + 48 (Wert) = 84, der Rest verteilt sich als Rand oben und
// unten. In ha_verlauf hatten sich zwei Zeilen um 6 px ueberlappt, weil genau
// diese Rechnung fehlte.
const int FRAME_X0 = 2,   FRAME_X1 = SCREEN_W - 3;   // 789
const int FRAME_Y0 = 2,   FRAME_Y1 = SCREEN_H - 3;   // 269
const int GRID_Y1  = 247;                            // untere Rasterlinie
const int COL_X[]  = { FRAME_X0, 264, 526, FRAME_X1 };
const int ROW_Y[]  = { FRAME_Y0, 124, GRID_Y1 };
const int NCOL = 3, NROW = 2;

// Oberkante des Kachelinhalts. Zeile 2 beginnt unter der 2 px starken
// Trennlinie, nicht auf ihr — sonst waere die untere Kachel 2 px flacher als
// die obere und die Werte staenden nicht auf einer Linie.
const int ROW_TOP[] = { FRAME_Y0, 126 };
const int TILE_H    = 122;

const int NAME_SZ = 24, VALUE_SZ = 48, TREND_SZ = 24;
const int PAD_X   = 8;
const int NAME_DY  = 0;                              // relativ zum Kachelinhalt
const int VALUE_DY = NAME_DY + NAME_SZ + 12;         // 36
const int CONTENT_H = VALUE_DY + VALUE_SZ;           // 84
const int PAD_Y     = (TILE_H - CONTENT_H) / 2;      // 19, oben wie unten

const int ARROW_W = 20, ARROW_H = 16;                // passend zu Groesse 24
// Die Fusszeile hat eine eigene Groesse, auch wenn sie zufaellig mal mit der
// des Trends uebereinstimmte: unter dem Raster sind nur 20 px frei, eine
// 24er-Zeile liefe hier unten aus dem Bild.
const int FOOTER_SZ = 16;
const int FOOTER_Y  = GRID_Y1 + 5;                   // 252, Textzeile 252..268

Tile tiles[NROOM];
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

// Das Grad-Zeichen fehlt im Font: die Arrays haben 95 Eintraege fuer
// ASCII 32..126, '°' (176) laege weit dahinter. Also selbst zeichnen — und
// weil EPD_DrawCircle() nur 1 px duenn zeichnet und das auf dem Panel kaum zu
// sehen ist, mehrere Radien uebereinander.
static void drawDegreeSign(int cx, int cy, int r, int thickness) {
  for (int t = 0; t < thickness; t++) EPD_DrawCircle(cx, cy, r + t, BLACK, 0);
}

// Dezimaltrennzeichen auf Deutsch. snprintf() schreibt immer einen Punkt: die
// Locale-Umschaltung, die das aendern wuerde, gibt es in der Arduino-Laufzeit
// nicht — und selbst wenn, waere sie fuer ein einzelnes Zeichen zu viel
// Maschinerie. Das Komma steht im Font (ASCII 44), das Grad-Zeichen nicht.
static void commaDecimal(char* s) {
  for (char* p = s; *p; p++) if (*p == '.') *p = ',';
}

// EPD_ShowString() bricht nicht um; Zeichenbreite ist size/2.
static int textWidth(const char* s, int size) { return (int)strlen(s) * (size / 2); }

// Zahlen mit engerem Komma.
//
// Der Font ist dickte-gleich: EPD_ShowString() rueckt je Zeichen size/2 vor,
// auch beim Komma. Dessen Tinte belegt aber nur die ersten rund 30 % der Zelle
// — bei Groesse 48 die Spalten 0..6 von 24 —, waehrend eine Ziffer bis Spalte 23
// reicht. Hinter dem Komma klaffen dadurch fast 20 leere Pixel, und "23,1" sieht
// aus, als stuende dort ein Leerzeichen.
//
// Deshalb wird zeichenweise gezeichnet und nach Komma und Punkt nur size/4
// vorgerueckt. Weiter geht es nicht: EPD_ShowChar() malt die ganze Zelle
// einschliesslich Hintergrund, eine zu weit nach links gezogene Folgezelle
// wuerde die Tinte des Kommas wieder ausradieren. size/4 laesst sie bei beiden
// hier verwendeten Groessen (48 und 16) sicher stehen.
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

// Trendpfeil: dir > 0 steigend, dir < 0 fallend, dir == 0 unveraendert.
// Ein gefuelltes Dreieck statt eines Umrisses — bei 16 x 12 px waere ein
// 1 px starker Umriss auf dem Panel kaum zu erkennen.
static void drawTrendArrow(int x, int y, int w, int h, int dir) {
  const int half = w / 2;
  if (dir == 0) {
    fillRect(x, y + h / 2 - 1, x + w - 1, y + h / 2 + 1, BLACK);
    return;
  }
  for (int i = 0; i < h; i++) {
    // i == 0 ist die Spitze, i == h-1 die Basis
    const int spread = (i * half) / (h - 1);
    const int yy = (dir > 0) ? (y + i) : (y + h - 1 - i);
    fillRect(x + half - spread, yy, x + half + spread, yy, BLACK);
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

// Zieht aus einer History-Antwort den aktuellen Wert und den Vergleichswert.
//
// Direkt mit strstr() scannen statt mit Arduino_JSON: der Objektbaum kostet ein
// Vielfaches des RAM, und gesucht sind ohnehin nur zwei Zahlen.
//
// `tRef` ist der Referenzzeitpunkt. Gesucht ist der LETZTE Messwert davor —
// nicht der erste der Antwort: HA stellt zwar den zum Startzeitpunkt gueltigen
// Zustand voran, aber dieselbe Funktion soll auch mit einer laengeren Antwort
// arbeiten (der Simulator legt zehn Tage bereit, nicht drei Stunden).
// Liegt gar kein Wert vor `tRef`, dient der erste Wert danach als Vergleich —
// dann ist der Zeitraum kuerzer als TREND_HOURS, was in der Anzeige nicht
// unterschieden wird: fuer einen frisch angelernten Sensor ist ein leicht zu
// kurzer Trend besser als gar keiner.
static bool parseTrend(const String& payload, time_t tRef, Tile& t) {
  const char* p = payload.c_str();
  bool haveFallback = false;
  float fallback = 0;

  while ((p = strstr(p, "\"state\":\"")) != nullptr) {
    p += 9;
    const char* stateEnd = strchr(p, '"');
    if (!stateEnd) break;

    // 'unavailable' und 'unknown' sind gueltige Zustaende, keine Fehler.
    // atof() macht daraus 0.0 — das waere ein Sprung auf 0 Grad.
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

    if (ts <= tRef) {
      t.ref = v;
      t.hasRef = true;
    } else if (!haveFallback) {
      fallback = v;
      haveFallback = true;
    }
  }

  if (!t.hasRef && haveFallback) { t.ref = fallback; t.hasRef = true; }
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

// `full` = mit Loeschzyklus. Ohne ihn ist der Bildwechsel schnell und leise,
// mit ihm wird das Panel mehrfach komplett schwarz — das raeumt Ghosting weg.
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
  flush(true);   // Fehlerbild immer sauber, egal was vorher auf dem Panel stand
}

// ---------------------------------------------------------------------------
// Darstellung
// ---------------------------------------------------------------------------

// Rahmen und Rasterlinien, je 2 px stark: eine einzelne Pixelreihe ist auf
// diesem Panel kaum zu sehen (dieselbe Beobachtung wie bei EPD_DrawCircle()).
static void drawGrid() {
  EPD_DrawRectangle(FRAME_X0,     FRAME_Y0,     FRAME_X1,     FRAME_Y1,     BLACK, 0);
  EPD_DrawRectangle(FRAME_X0 + 1, FRAME_Y0 + 1, FRAME_X1 - 1, FRAME_Y1 - 1, BLACK, 0);

  for (int c = 1; c < NCOL; c++)
    fillRect(COL_X[c], FRAME_Y0, COL_X[c] + 1, GRID_Y1, BLACK);
  fillRect(FRAME_X0, ROW_Y[1], FRAME_X1, ROW_Y[1] + 1, BLACK);
  fillRect(FRAME_X0, GRID_Y1,  FRAME_X1, GRID_Y1 + 1,  BLACK);
}

static void drawTile(int col, int row, const Tile& t) {
  const int x       = COL_X[col] + PAD_X;
  const int y       = ROW_TOP[row] + PAD_Y;
  const int rightX  = COL_X[col + 1] - PAD_X;   // rechte Kante des Inhalts

  EPD_ShowString(x, y + NAME_DY, ROOMS[t.room].label, NAME_SZ, BLACK);

  if (!t.ok) {
    EPD_ShowString(x, y + VALUE_DY, "n/a", VALUE_SZ, BLACK);
    const char* msg = "kein Messwert";
    EPD_ShowString(rightX - textWidth(msg, 16), y + VALUE_DY + VALUE_SZ / 2 - 8,
                   msg, 16, BLACK);
    return;
  }

  char value[16];
  snprintf(value, sizeof(value), "%.1f", t.value);
  commaDecimal(value);
  showNumber(x, y + VALUE_DY, value, VALUE_SZ, BLACK);

  int cursor = x + numberWidth(value, VALUE_SZ) + 14;
  drawDegreeSign(cursor + 7, y + VALUE_DY + 9, 6, 3);
  cursor += 26;
  EPD_ShowString(cursor, y + VALUE_DY, "C", VALUE_SZ, BLACK);

  // Der Trend steht rechtsbuendig an der Kachelkante, nicht in festem Abstand
  // hinter dem Wert: die Werte sind verschieden breit ("23,1" gegen "-3,5"),
  // und ein mitwanderender Trend liesse die sechs Kacheln unruhig wirken.
  // Senkrecht sitzt er auf der Mitte des Wertes.
  const int midY = y + VALUE_DY + VALUE_SZ / 2;

  if (!t.hasRef) {
    const char* msg = "kein Vergleich";
    EPD_ShowString(rightX - textWidth(msg, 16), midY - 8, msg, 16, BLACK);
    return;
  }

  const float d = t.value - t.ref;
  const int dir = (d > TREND_FLAT) ? 1 : (d < -TREND_FLAT ? -1 : 0);

  // Ohne Einheit: die Differenz zweier Temperaturen waere korrekt in Kelvin
  // anzugeben, auf einem Wohnzimmer-Display ist ein "K" hinter der Zahl aber
  // eher raetselhaft als hilfreich. Pfeil und Fusszeile sagen, was gemeint ist.
  char trend[16];
  // "%+.1f" macht aus einer Differenz von -0,04 ein "-0,0" — ein Vorzeichen
  // vor einer Null sieht nach einem Fehler aus. Genau null bekommt deshalb
  // gar keins.
  if (fabsf(d) < 0.05f) snprintf(trend, sizeof(trend), "0.0");
  else                  snprintf(trend, sizeof(trend), "%+.1f", d);
  commaDecimal(trend);

  const int trendW = ARROW_W + 8 + numberWidth(trend, TREND_SZ);
  const int trendX = rightX - trendW;
  drawTrendArrow(trendX, midY - ARROW_H / 2, ARROW_W, ARROW_H, dir);
  showNumber(trendX + ARROW_W + 8, midY - TREND_SZ / 2, trend, TREND_SZ, BLACK);
}

static void drawFooter() {
  char stamp[48];
  struct tm lt;
  const time_t nowT = time(nullptr);
  localtime_r(&nowT, &lt);
  strftime(stamp, sizeof(stamp), "Stand %d.%m. %H:%M", &lt);
  EPD_ShowString(COL_X[0] + PAD_X, FOOTER_Y, stamp, FOOTER_SZ, BLACK);

  char hint[64];
  snprintf(hint, sizeof(hint), "Pfeil und Zahl: Veraenderung in Grad seit %d h", TREND_HOURS);
  EPD_ShowString(FRAME_X1 - PAD_X - textWidth(hint, FOOTER_SZ), FOOTER_Y,
                 hint, FOOTER_SZ, BLACK);
}

// Warm nach kalt, Kacheln ohne Messwert ans Ende. Insertion Sort, n = 6.
static void sortTiles() {
  for (int i = 1; i < NROOM; i++) {
    const Tile key = tiles[i];
    int j = i - 1;
    while (j >= 0) {
      const bool keyFirst = key.ok && (!tiles[j].ok || key.value > tiles[j].value);
      if (!keyFirst) break;
      tiles[j + 1] = tiles[j];
      j--;
    }
    tiles[j + 1] = key;
  }
}

static void drawScreen(bool full) {
  newFrame();
  drawGrid();
  for (int i = 0; i < NROOM && i < NCOL * NROW; i++)
    drawTile(i % NCOL, i / NCOL, tiles[i]);
  drawFooter();
  flush(full);
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

  const time_t tEnd = time(nullptr);
  const time_t tRef = tEnd - (time_t)TREND_HOURS * 3600;

  char s0[32], s1[32];
  formatIsoUtc(tRef, s0, sizeof(s0));
  formatIsoUtc(tEnd, s1, sizeof(s1));

  int good = 0;
  for (int i = 0; i < NROOM; i++) {
    tiles[i] = Tile();
    tiles[i].room = i;

    // Ohne end_time liefert HA nur EINEN Tag ab start_time, nicht bis jetzt.
    // Hier faellt das nicht auf — das Fenster ist ohnehin kuerzer —, aber der
    // Parameter bleibt drin, damit die Abfrage bei einem groesseren
    // TREND_HOURS nicht stillschweigend etwas anderes liefert.
    const String path = String("/api/history/period/") + s0 +
                        "?filter_entity_id=" + ROOMS[i].entity +
                        "&end_time=" + s1 +
                        "&minimal_response&no_attributes";

    if (!haGet(path, body, err)) {
      Serial.printf("HA: %-14s FEHLER %s\n", ROOMS[i].label, err.c_str());
      continue;
    }
    if (!parseTrend(body, tRef, tiles[i])) {
      Serial.printf("HA: %-14s keine Werte\n", ROOMS[i].label);
      continue;
    }
    good++;
    Serial.printf("HA: %-14s %6.1f C, vor %d h %6.1f C (%+.1f K), %d Punkte\n",
                  ROOMS[i].label, tiles[i].value, TREND_HOURS,
                  tiles[i].hasRef ? tiles[i].ref : NAN,
                  tiles[i].hasRef ? tiles[i].value - tiles[i].ref : NAN,
                  tiles[i].points);
  }

  // Einzelne Ausfaelle sind kein Grund, das ganze Bild wegzuwerfen — die
  // betroffene Kachel zeigt "n/a" und steht hinten.
  if (good == 0) {
    showError("Keine Daten", "Kein Sensor lieferte Werte");
    return false;
  }
  Serial.printf("HA: %d von %d Sensoren\n", good, NROOM);

  sortTiles();

  const bool full = (updateCount % FULL_REFRESH_EVERY) == 0;
  Serial.printf("Panel: %s\n", full ? "Vollrefresh" : "Fast-Update");
  drawScreen(full);
  updateCount++;
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n--- ha_kacheln ---");

  pinMode(PIN_DISPLAY_POWER, OUTPUT);
  digitalWrite(PIN_DISPLAY_POWER, HIGH);   // ohne das bleibt das Panel dunkel

  connectWiFi(20000);
  waitForTime(20000);

  lastOk = update();
}

void loop() {
  // Nach einem Fehler zuegig erneut versuchen statt zehn Minuten warten.
  delay(lastOk ? UPDATE_INTERVAL_MS : RETRY_INTERVAL_MS);
  lastOk = update();
}
