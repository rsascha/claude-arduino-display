// ha_temperatur — holt einen Sensorwert aus Home Assistant und zeigt ihn
// auf dem CrowPanel 5.79" E-Paper an.
//
// Zugangsdaten stehen in secrets.h (per .gitignore ausgeschlossen).
// Vorlage dafür: secrets.h.example

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <time.h>

#include "EPD.h"
#include "secrets.h"

uint8_t ImageBW[27200];

const int PIN_DISPLAY_POWER = 7;
const uint16_t DISPLAY_ROTATION = 0;

const int SCREEN_W = 792;   // sichtbar; der Puffer (EPD_W) ist 800 breit
const int SCREEN_H = 272;

// Wie oft neu geholt wird. E-Paper-Refreshs sind nicht unbegrenzt oft möglich,
// und die Sensoren melden ohnehin nur alle paar Minuten.
const unsigned long UPDATE_INTERVAL_MS = 15UL * 60UL * 1000UL;

// Zeitzone Berlin inkl. Sommerzeit-Regel, für den "Aktualisiert"-Zeitstempel
const char* TZ_BERLIN = "CET-1CEST,M3.5.0,M10.5.0/3";

// ---------------------------------------------------------------------------
// Zeichnen
// ---------------------------------------------------------------------------

// Paint_SetPixel() prüft seine Koordinaten NICHT und schreibt sonst über den
// Puffer hinaus. Die Parameter sind uint16_t, ein negativer Wert würde zu
// einer riesigen Zahl. Deshalb signed rechnen und vorher abfangen.
static void safePixel(int x, int y, uint16_t color) {
  if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
  Paint_SetPixel((uint16_t)x, (uint16_t)y, color);
}

// Das Grad-Zeichen fehlt im Font: die Arrays haben 95 Einträge für ASCII 32..126,
// '°' (176) läge weit dahinter. Also selbst zeichnen.
static void drawDegreeSign(int cx, int cy, int r, int thickness) {
  for (int t = 0; t < thickness; t++) {
    EPD_DrawCircle(cx, cy, r + t, BLACK, 0);
  }
}

// Breite eines Strings in Pixeln. EPD_ShowString() bricht nicht um und prüft
// nichts — wer über den Rand schreibt, merkt es nur am abgeschnittenen Bild.
static int textWidth(const char* s, int size) {
  return (int)strlen(s) * (size / 2);
}

static void beginFrame() {
  EPD_GPIOInit();
  Paint_NewImage(ImageBW, EPD_W, EPD_H, DISPLAY_ROTATION, WHITE);
  Paint_Clear(WHITE);

  // Panel physisch löschen — E-Paper behält sonst das alte Bild
  EPD_FastMode1Init();
  EPD_Display_Clear();
  EPD_Update();

  EPD_GPIOInit();
  EPD_FastMode1Init();
}

static void endFrame() {
  EPD_Display(ImageBW);
  EPD_FastUpdate();
}

// Ganzseitige Fehlermeldung. Wichtig: ohne die sieht man bei einem Problem
// nur ein leeres Display und weiß nicht, woran es liegt.
static void showError(const char* headline, const char* detail) {
  beginFrame();
  EPD_DrawRectangle(4, 4, SCREEN_W - 5, SCREEN_H - 5, BLACK, 0);
  EPD_ShowString(50,  70, headline, 48, BLACK);
  EPD_ShowString(50, 140, detail,   24, BLACK);
  EPD_ShowString(50, 190, "Details per: make monitor", 16, BLACK);
  endFrame();
}

static void showMeasurement(const char* label, const char* value,
                            bool isCelsius, const char* stamp) {
  beginFrame();
  EPD_DrawRectangle(4, 4, SCREEN_W - 5, SCREEN_H - 5, BLACK, 0);

  // Überschrift
  EPD_ShowString(50, 40, label, 24, BLACK);
  EPD_DrawLine(50, 78, SCREEN_W - 50, 78, BLACK);

  // Messwert groß. 48 ist die größte verfügbare Schrift.
  const int valueX = 50, valueY = 110;
  EPD_ShowString(valueX, valueY, value, 48, BLACK);

  int cursor = valueX + textWidth(value, 48);
  if (isCelsius) {
    cursor += 14;
    drawDegreeSign(cursor + 7, valueY + 9, 6, 3);
    cursor += 26;
    EPD_ShowString(cursor, valueY, "C", 48, BLACK);
  }

  // Zeitstempel unten
  EPD_ShowString(50, SCREEN_H - 50, stamp, 16, BLACK);

  endFrame();
}

// ---------------------------------------------------------------------------
// Netzwerk
// ---------------------------------------------------------------------------

static bool connectWiFi(unsigned long timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.printf("WLAN: verbinde mit '%s' ...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(250);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WLAN: fehlgeschlagen.");
    return false;
  }
  Serial.printf("WLAN: verbunden, IP %s, RSSI %d dBm\n",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
  return true;
}

// Holt eine Entität aus Home Assistant.
// Rückgabe true bei Erfolg; state und unit werden dann gefüllt.
static bool fetchEntity(const char* entityId, String& state, String& unit,
                        String& friendlyName, String& errorOut) {
  WiFiClient client;
  HTTPClient http;

  String url = String("http://") + HA_HOST + ":" + String(HA_PORT) +
               "/api/states/" + entityId;

  if (!http.begin(client, url)) {
    errorOut = "http.begin fehlgeschlagen";
    return false;
  }
  // Ohne diesen Header antwortet Home Assistant mit 401.
  http.addHeader("Authorization", String("Bearer ") + HA_TOKEN);

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    errorOut = String("HTTP ") + code;
    if (code == HTTP_CODE_UNAUTHORIZED) errorOut += " (Token?)";
    if (code == HTTP_CODE_NOT_FOUND)    errorOut += " (entity_id?)";
    Serial.printf("HA: %s\n", errorOut.c_str());
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();

  JSONVar obj = JSON.parse(payload);
  if (JSON.typeof(obj) == "undefined") {
    errorOut = "JSON nicht lesbar";
    return false;
  }
  if (!obj.hasOwnProperty("state")) {
    errorOut = "Feld 'state' fehlt";
    return false;
  }

  state = (const char*)obj["state"];

  // 'unavailable' und 'unknown' sind gültige HA-Zustände, keine Fehler –
  // aber als Messwert unbrauchbar.
  if (state == "unavailable" || state == "unknown") {
    errorOut = String("Sensor: ") + state;
    return false;
  }

  unit = "";
  friendlyName = entityId;
  if (obj.hasOwnProperty("attributes")) {
    JSONVar a = obj["attributes"];
    if (a.hasOwnProperty("unit_of_measurement")) unit = (const char*)a["unit_of_measurement"];
    if (a.hasOwnProperty("friendly_name"))       friendlyName = (const char*)a["friendly_name"];
  }

  Serial.printf("HA: %s = %s %s\n", friendlyName.c_str(), state.c_str(), unit.c_str());
  return true;
}

static String localTimestamp() {
  struct tm t;
  if (!getLocalTime(&t, 2000)) return "Aktualisiert: Zeit unbekannt";
  char buf[48];
  strftime(buf, sizeof(buf), "Aktualisiert: %d.%m.%Y %H:%M", &t);
  return String(buf);
}

// ---------------------------------------------------------------------------

static void updateDisplay() {
  String state, unit, name, err;

  if (!connectWiFi(20000)) {
    showError("Kein WLAN", "SSID/Passwort in secrets.h pruefen");
    return;
  }

  if (!fetchEntity(HA_ENTITY_ID, state, unit, name, err)) {
    showError("Fehler", err.c_str());
    return;
  }

  // Der Anzeigename kommt aus HA und kann Umlaute enthalten — der Font kann
  // nur ASCII 32..126. Alles andere durch '?' ersetzen, sonst wird über das
  // Font-Array hinaus gelesen.
  String label = name;
  for (unsigned int i = 0; i < label.length(); i++) {
    if (label[i] < 32 || label[i] > 126) label.setCharAt(i, '?');
  }

  const bool celsius = unit.indexOf("C") >= 0;
  showMeasurement(label.c_str(), state.c_str(), celsius, localTimestamp().c_str());
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n--- ha_temperatur ---");

  pinMode(PIN_DISPLAY_POWER, OUTPUT);
  digitalWrite(PIN_DISPLAY_POWER, HIGH);   // ohne das bleibt das Panel dunkel

  connectWiFi(20000);
  configTzTime(TZ_BERLIN, "pool.ntp.org", "time.nist.gov");

  updateDisplay();
}

void loop() {
  delay(UPDATE_INTERVAL_MS);
  updateDisplay();
}
