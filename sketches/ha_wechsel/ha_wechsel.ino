// ha_wechsel — zeigt abwechselnd die Raumtemperaturen und die Wetterlage.
//
//   automatisch   alle 60 Sekunden eine Seite weiter
//   EXIT (GPIO 1) blaettert sofort um, die Minute laeuft danach von vorn
//   MENU (GPIO 2) holt die Daten neu und zeichnet die aktuelle Seite frisch
//
// Die beiden Bildschirme stecken in screen_temperaturen.cpp und
// screen_wetter.cpp, die gemeinsamen Zeichenhilfen in draw.cpp. Diese Datei
// kuemmert sich nur um Daten, Tasten und Taktung. Der Zuschnitt hat einen
// Grund: in einer einzigen .ino waeren es rund 900 Zeilen, und die Haelfte
// davon stuende doppelt da — beide Seiten brauchen dieselben Primitive, weil
// EPD.h nur Linie, Rechteck und Kreis kennt.
//
// Warum die Daten NICHT bei jedem Wechsel geholt werden: sonst maesse man beim
// Umblaettern die Netzwerklatenz statt das Panel, und jede Minute lief eine
// Runde HTTP-Anfragen. Geholt wird alle zehn Minuten, gezeichnet wird aus dem
// Zwischenspeicher — dieselbe Aufteilung wie in ha_umschalten.
//
// Zugangsdaten: secrets.h (per .gitignore ausgeschlossen), Vorlage secrets.h.example

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <math.h>

#include "EPD.h"
#include "draw.h"
#include "screens.h"
#include "secrets.h"

uint8_t ImageBW[27200];

const int PIN_DISPLAY_POWER = 7;
// Querformat, USB oben, Bedienelemente an der linken Kante: EXIT ist der
// obere Taster, MENU der untere. Beide liegen gegen Masse, gedrueckt ist LOW.
// Zuordnung am 23.08.2026 am Geraet verifiziert, siehe README -> Bedienelemente.
const int PIN_EXIT = 1;
const int PIN_MENU = 2;

const uint16_t DISPLAY_ROTATION = 0;

const int TREND_HOURS = 3;

const unsigned long SWITCH_INTERVAL_MS = 60UL * 1000UL;
const unsigned long FETCH_INTERVAL_MS  = 10UL * 60UL * 1000UL;
const unsigned long RETRY_INTERVAL_MS  = 60UL * 1000UL;

// Jeder 60. Bildaufbau mit Loeschzyklus, bei einem Wechsel pro Minute also
// stuendlich. Fast-Update allein laesst mit der Zeit Schatten stehen; ein
// Vollrefresh bei JEDEM Wechsel liesse das Panel dagegen sechzigmal pro Stunde
// mehrfach komplett schwarz werden — das faellt mehr auf als das Ghosting.
const int FULL_REFRESH_EVERY = 60;

// Welche Seite nach dem Start zuerst kommt. 0 = Temperaturen, 1 = Wetter.
// Der Simulator ruft nur setup() auf und zeigt deshalb genau diese Seite.
const int START_SCREEN = 0;

const char* TZ_BERLIN = "CET-1CEST,M3.5.0,M10.5.0/3";

// Die entity_ids sind ueber friendly_name verifiziert, nicht geraten. Alle vier
// SONOFF-Sensoren tragen denselben Geraetenamen, unterschieden nur durch die
// Endung _2 / _3 — wer hier raet, landet im falschen Raum.
const RoomDef ROOMS[] = {
  { "sensor.wohnzimmer_temperatur_sonoff_snzb_02d_temperatur", "Wohnzimmer"   },
  { "sensor.temperatur_sonoff_snzb_02d_temperatur_2",          "Schlafzimmer" },
  { "sensor.temperatur_sonoff_snzb_02d_temperatur",            "Badezimmer"   },
  { "sensor.temperatur_sonoff_snzb_02d_temperatur_3",          "Kueche"       },
  { "sensor.flur_temperatur_korrigiert",                       "Flur"         },
  { "sensor.solarnode_temperatur",                             "Aussen"       },
};
const int NROOM = sizeof(ROOMS) / sizeof(ROOMS[0]);

const char* ENT_WIND_SPEED  = "sensor.windgeschwindigkeit";
const char* ENT_WIND_DIR    = "sensor.windrichtung";
const char* ENT_WIND_SECTOR = "sensor.windrichtung_sektor";
const char* ENT_PRESSURE    = "sensor.solarnode_luftdruck";
const char* ENT_CONDITION   = "weather.forecast_home";

const ConditionDef CONDITIONS[] = {
  { "sunny",           ICON_SUN,    "sonnig"      },
  { "clear-night",     ICON_MOON,   "klar"        },
  { "partlycloudy",    ICON_PARTLY, "heiter"      },
  { "cloudy",          ICON_CLOUD,  "bewoelkt"    },
  { "fog",             ICON_CLOUD,  "Nebel"       },
  { "windy",           ICON_CLOUD,  "windig"      },
  { "windy-variant",   ICON_CLOUD,  "windig"      },
  { "rainy",           ICON_RAIN,   "Regen"       },
  { "pouring",         ICON_RAIN,   "Starkregen"  },
  { "lightning",       ICON_RAIN,   "Gewitter"    },
  { "lightning-rainy", ICON_RAIN,   "Gewitter"    },
  { "hail",            ICON_SNOW,   "Hagel"       },
  { "snowy",           ICON_SNOW,   "Schnee"      },
  { "snowy-rainy",     ICON_SNOW,   "Schneeregen" },
  { "exceptional",     ICON_CLOUD,  "Unwetter"    },
};
const int NCONDITION = sizeof(CONDITIONS) / sizeof(CONDITIONS[0]);

Tile    tiles[NROOM];
Reading windSpeed, windDir, windSector, condition;
Trend   pressure;

int  screenIndex = START_SCREEN;
int  drawCount   = 0;
bool haveData    = false;
unsigned long lastSwitchMs = 0, lastFetchMs = 0;

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

// getLocalTime() allein genuegt nicht: ist der erste Versuch verstrichen,
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

// Holt /api/states/<entity>. 'unavailable' und 'unknown' sind gueltige
// Zustaende, keine Fehler — sie gelten hier aber als "kein Wert", damit
// atof() sie nicht klaglos zu 0.0 macht.
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

// Aktueller Wert und Vergleichswert aus EINER History-Antwort — fuer die
// Raumtemperaturen wie fuer den Luftdruck dieselbe Funktion.
//
// Gesucht ist der letzte Messwert VOR tRef, nicht der erste der Antwort: so
// stimmt der Vergleich auch, wenn der Funktion ein laengerer Zeitraum
// vorgesetzt wird — der Simulator legt genau das bereit. Liegt gar kein Wert
// vor tRef, dient der erste danach als Vergleich; dann ist der Zeitraum
// kuerzer als TREND_HOURS, was die Anzeige nicht unterscheidet.
//
// Direkt mit strstr() scannen statt mit Arduino_JSON: der Objektbaum kostet ein
// Vielfaches des RAM, und gesucht sind ohnehin nur zwei Zahlen.
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

    if (ts <= tRef)         { t.ref = v; t.hasRef = true; }
    else if (!haveFallback) { fallback = v; haveFallback = true; }
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

static void drawCurrent() {
  const bool full = (drawCount % FULL_REFRESH_EVERY) == 0;
  newFrame();
  if (screenIndex == 0)
    drawTemperatureScreen(ROOMS, tiles, NROOM, TREND_HOURS);
  else
    drawWeatherScreen(CONDITIONS, NCONDITION, windSpeed, windDir, windSector,
                      condition, pressure, TREND_HOURS);
  Serial.printf("Panel: Seite %d, %s\n", screenIndex + 1,
                full ? "Vollrefresh" : "Fast-Update");
  flush(full);
  drawCount++;
}

// ---------------------------------------------------------------------------
// Tasten
// ---------------------------------------------------------------------------

// Entprellt und wartet das Loslassen ab — sonst zaehlt ein Druck mehrfach.
// Waehrend eines Bildaufbaus (rund eine Sekunde) wird nicht gelesen; ein
// Tastendruck genau in diesem Moment geht verloren.
static bool pressed(int pin) {
  if (digitalRead(pin) != LOW) return false;
  delay(20);
  if (digitalRead(pin) != LOW) return false;
  while (digitalRead(pin) == LOW) delay(10);
  return true;
}

// ---------------------------------------------------------------------------
// Daten
// ---------------------------------------------------------------------------

static bool fetchAll() {
  if (!connectWiFi(20000)) {
    showError("Kein WLAN", "SSID/Passwort in secrets.h pruefen");
    return false;
  }
  if (!waitForTime(20000)) {
    // Die Meldung nennt den WLAN-Zustand mit: ist die IP da, liegt es nicht am
    // Netz, sondern daran, dass der NTP-Server nicht antwortet.
    char detail[64];
    snprintf(detail, sizeof(detail), "NTP stumm - WLAN ok, IP %s",
             WiFi.localIP().toString().c_str());
    showError("Keine Zeit", detail);
    return false;
  }

  const time_t tEnd = time(nullptr);
  const time_t tRef = tEnd - (time_t)TREND_HOURS * 3600;
  int good = 0;

  for (int i = 0; i < NROOM; i++) {
    Trend t;
    tiles[i] = Tile();
    tiles[i].room = i;
    if (!fetchTrend(ROOMS[i].entity, tRef, tEnd, t)) {
      Serial.printf("HA: %-32s keine Werte\n", ROOMS[i].entity);
      continue;
    }
    tiles[i].ok     = t.ok;
    tiles[i].hasRef = t.hasRef;
    tiles[i].value  = t.value;
    tiles[i].ref    = t.ref;
    tiles[i].points = t.points;
    good++;
    Serial.printf("HA: %-32s %5.1f C (vor %d h %5.1f), %d Punkte\n",
                  ROOMS[i].label, t.value, TREND_HOURS,
                  t.hasRef ? t.ref : NAN, t.points);
  }

  if (fetchState(ENT_WIND_SPEED,  windSpeed))  good++;
  if (fetchState(ENT_WIND_DIR,    windDir))    good++;
  if (fetchState(ENT_WIND_SECTOR, windSector)) good++;
  if (fetchState(ENT_CONDITION,   condition))  good++;
  if (fetchTrend(ENT_PRESSURE, tRef, tEnd, pressure)) {
    good++;
    Serial.printf("HA: %-32s %.1f hPa (vor %d h %.1f), %d Punkte\n",
                  ENT_PRESSURE, pressure.value, TREND_HOURS,
                  pressure.hasRef ? pressure.ref : NAN, pressure.points);
  }

  // Einzelne Ausfaelle sind kein Grund, das ganze Bild wegzuwerfen — die
  // betroffene Kachel oder Spalte zeigt "n/a".
  if (good == 0) {
    showError("Keine Daten", "Kein Sensor lieferte Werte");
    return false;
  }
  Serial.printf("HA: %d von %d Werten\n", good, NROOM + 5);
  lastFetchMs = millis();
  haveData = true;
  return true;
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n--- ha_wechsel ---");

  pinMode(PIN_DISPLAY_POWER, OUTPUT);
  digitalWrite(PIN_DISPLAY_POWER, HIGH);   // ohne das bleibt das Panel dunkel
  pinMode(PIN_EXIT, INPUT);
  pinMode(PIN_MENU, INPUT);

  connectWiFi(20000);
  waitForTime(20000);

  if (fetchAll()) drawCurrent();
  lastSwitchMs = millis();
}

void loop() {
  // Tasten haben Vorrang vor der Uhr: wer drueckt, will sofort etwas sehen.
  if (pressed(PIN_EXIT)) {
    screenIndex = 1 - screenIndex;
    Serial.println("EXIT: umgeblaettert");
    if (haveData) drawCurrent();
    lastSwitchMs = millis();      // die Minute laeuft von vorn, damit das
    return;                       // aufgerufene Bild in Ruhe lesbar bleibt
  }

  if (pressed(PIN_MENU)) {
    Serial.println("MENU: Daten neu holen");
    if (fetchAll()) drawCurrent();
    lastSwitchMs = millis();
    return;
  }

  const unsigned long now = millis();

  // Nach einem Fehler zuegig erneut versuchen statt zehn Minuten warten.
  const unsigned long fetchDue = haveData ? FETCH_INTERVAL_MS : RETRY_INTERVAL_MS;
  if (now - lastFetchMs >= fetchDue) {
    // Bewusst ohne Neuzeichnen: der naechste Wechsel steht ohnehin binnen
    // einer Minute an und bringt die frischen Zahlen mit. Ein zusaetzlicher
    // Bildaufbau waere ein Refresh-Zyklus fuer nichts.
    const bool wasEmpty = !haveData;
    if (fetchAll() && wasEmpty) { drawCurrent(); lastSwitchMs = millis(); }
    return;
  }

  if (haveData && now - lastSwitchMs >= SWITCH_INTERVAL_MS) {
    screenIndex = 1 - screenIndex;
    drawCurrent();
    lastSwitchMs = millis();
    return;
  }

  delay(20);   // Tastenabfrage laeuft damit rund 50-mal je Sekunde
}
