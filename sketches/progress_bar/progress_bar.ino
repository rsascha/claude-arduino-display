// progress_bar — Fortschrittsanzeige mit fuenf Segmenten, gross und zentriert.
// Alle 3 Sekunden kommt ein Segment dazu; nach dem fuenften ist Schluss, und
// ein Druck auf EXIT startet den Durchlauf neu.
//
// Zugleich der praktische Test fuer EPD_PartUpdate(). Der Vergleich in
// sketches/ha_umschalten hatte gezeigt, dass Partial fuer einen Vollbildwechsel
// nichts taugt — hier aendert sich pro Schritt nur ein Segment, also genau der
// Fall, fuer den der Modus gedacht ist.
//
// Anders als in ha_umschalten laeuft zwischen den Schritten KEIN Hardware-Reset:
// EPD_FastMode1Init() enthaelt einen, und ein zurueckgesetzter Controller kennt
// das vorherige Bild nicht mehr — worauf der Partial-Modus gerade aufbaut.
// Initialisiert wird deshalb nur einmal je Durchlauf.
//
// Kein WLAN, keine secrets.h — der Sketch braucht nichts ausser dem Panel.

#include <Arduino.h>
#include "EPD.h"

uint8_t ImageBW[27200];

const int PIN_DISPLAY_POWER = 7;
const uint16_t DISPLAY_ROTATION = 0;

// Gegen Masse, gedrueckt ist LOW. Position am Geraet geprueft: bei USB oben und
// Bedienelementen links ist EXIT der obere der beiden Taster.
const int PIN_EXIT = 1;

const int SCREEN_W = 792;    // sichtbar; der Puffer (EPD_W) ist 800 breit
const int SCREEN_H = 272;

const int STEPS = 5;
const unsigned long STEP_MS = 3000;

// --- Layout -----------------------------------------------------------------
// Vertikal von oben: Titel 30..54, Balken 78..188, Prozentzeile 212..236,
// Rahmen bei 269. Eine Textzeile braucht `size` Pixel Hoehe, nicht weniger —
// in ha_verlauf hatten sich zwei Zeilen um 6 px ueberlappt, weil das zu knapp
// gerechnet war.
const int TITLE_Y   = 30;
const int TITLE_SZ  = 24;
const int BAR_X0    = 96,  BAR_X1 = 696;      // 600 px breit, zentriert
const int BAR_Y0    = 78,  BAR_Y1 = 188;      // 110 px hoch
const int BAR_PAD   = 10;                     // Abstand Rahmen -> Segmente
const int SEG_GAP   = 12;
const int FOOT_Y    = 212;
const int FOOT_SZ   = 24;

// ---------------------------------------------------------------------------
// Zeichenhilfen
// ---------------------------------------------------------------------------

// Paint_SetPixel() prueft seine Koordinaten NICHT und schreibt sonst ueber den
// Puffer hinaus; die Parameter sind uint16_t, ein negativer Wert wuerde zu einer
// riesigen Zahl. Deshalb signed rechnen und vorher abfangen.
static void safePixel(int x, int y, uint16_t color) {
  if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
  Paint_SetPixel((uint16_t)x, (uint16_t)y, color);
}

// EPD_DrawRectangle() zieht im gefuellten Modus Linien von Ystart bis Yend-1,
// die letzte Zeile fehlt also. Hier selbst fuellen, dann stimmt die Hoehe.
static void fillRect(int x0, int y0, int x1, int y1, uint16_t color) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++)
      safePixel(x, y, color);
}

// Fuer sichtbare Linien mehrere Rechtecke ineinander: ein 1 px duenner Umriss
// ist auf dem Panel kaum zu erkennen.
static void frameRect(int x0, int y0, int x1, int y1, int thickness) {
  for (int t = 0; t < thickness; t++)
    EPD_DrawRectangle(x0 - t, y0 - t, x1 + t, y1 + t, BLACK, 0);
}

// EPD_ShowString() bricht nicht um; Zeichenbreite ist size/2.
static int textWidth(const char* s, int size) { return (int)strlen(s) * (size / 2); }

static void centerText(int y, const char* s, int size) {
  EPD_ShowString((SCREEN_W - textWidth(s, size)) / 2, y, s, size, BLACK);
}

// ---------------------------------------------------------------------------
// Panel
// ---------------------------------------------------------------------------

static void newFrame() {
  Paint_NewImage(ImageBW, EPD_W, EPD_H, DISPLAY_ROTATION, WHITE);
  Paint_Clear(WHITE);
}

// Einmal je Durchlauf: Panel weiss loeschen und den Controller aufsetzen.
// Enthaelt den Hardware-Reset, danach faengt der Partial-Modus bei null an.
static void panelReset() {
  EPD_GPIOInit();
  EPD_FastMode1Init();
  EPD_Display_Clear();
  EPD_Update();
  EPD_GPIOInit();
  EPD_FastMode1Init();

  // Unverzichtbar vor dem Partial-Betrieb, und leicht zu uebersehen:
  // Der Partial-Modus waehlt seine Waveform pro Pixel aus dem Uebergang
  // "altes Bild -> neues Bild"; das alte Bild liest er aus RAM 0x26/0xA6.
  // EPD_Display_Clear() hinterlaesst dort 0x00, EPD_Display() schreibt nur
  // 0x24/0xA4 und korrigiert das nie. Ohne diesen Aufruf rechnet das ERSTE
  // Partial-Update gegen einen falschen Ausgangszustand und treibt die Pixel
  // zu schwach — das erste Segment wird hellgrau statt schwarz. Ab dem
  // zweiten Update fuehrt der Controller 0x26 selbst nach, deshalb faellt es
  // nur beim ersten auf. Elecrow macht es in 5.79_key genauso (Zeile 36-38).
  EPD_Clear_R26A6H();
}

// ---------------------------------------------------------------------------
// Darstellung
// ---------------------------------------------------------------------------

static void renderBar(int done) {
  newFrame();
  EPD_DrawRectangle(2, 2, SCREEN_W - 3, SCREEN_H - 3, BLACK, 0);

  // --- Titel ---
  char title[24];
  if (done >= STEPS) snprintf(title, sizeof(title), "Fertig");
  else               snprintf(title, sizeof(title), "Schritt %d von %d", done, STEPS);
  centerText(TITLE_Y, title, TITLE_SZ);

  // --- Rahmen des Balkens, 3 px stark ---
  frameRect(BAR_X0, BAR_Y0, BAR_X1, BAR_Y1, 3);

  // --- Segmente ---
  const int innerX0 = BAR_X0 + BAR_PAD;
  const int innerX1 = BAR_X1 - BAR_PAD;
  const int innerY0 = BAR_Y0 + BAR_PAD;
  const int innerY1 = BAR_Y1 - BAR_PAD;
  const int innerW = innerX1 - innerX0 + 1;
  const int segW = (innerW - (STEPS - 1) * SEG_GAP) / STEPS;
  // Die Ganzzahldivision laesst ein paar Pixel uebrig; sie gleichmaessig auf
  // beide Seiten legen, sonst sitzt der Block sichtbar links vom Rahmenmittelpunkt.
  const int offset = (innerW - (STEPS * segW + (STEPS - 1) * SEG_GAP)) / 2;

  for (int i = 0; i < done && i < STEPS; i++) {
    const int x = innerX0 + offset + i * (segW + SEG_GAP);
    fillRect(x, innerY0, x + segW - 1, innerY1, BLACK);
  }

  // --- Fusszeile ---
  char foot[48];
  const int pct = done * 100 / STEPS;
  if (done >= STEPS) snprintf(foot, sizeof(foot), "100 %%   EXIT startet neu");
  else               snprintf(foot, sizeof(foot), "%d %%", pct);
  centerText(FOOT_Y, foot, FOOT_SZ);
}

// ---------------------------------------------------------------------------
// EXIT-Taste
// ---------------------------------------------------------------------------

// Gedrueckt ist LOW. Wartet die Freigabe ab, damit ein Druck nicht mehrfach
// zaehlt, und entprellt beide Flanken.
static bool exitPressed() {
  if (digitalRead(PIN_EXIT) != LOW) return false;
  delay(30);
  if (digitalRead(PIN_EXIT) != LOW) return false;
  while (digitalRead(PIN_EXIT) == LOW) delay(10);
  delay(30);
  return true;
}

// Wartet `ms` ab, bricht aber sofort ab, wenn EXIT gedrueckt wird.
static bool waitOrExit(unsigned long ms) {
  const unsigned long t0 = millis();
  while (millis() - t0 < ms) {
    if (exitPressed()) return true;
    delay(10);
  }
  return false;
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n--- progress_bar ---");

  pinMode(PIN_DISPLAY_POWER, OUTPUT);
  digitalWrite(PIN_DISPLAY_POWER, HIGH);   // ohne das bleibt das Panel dunkel
  pinMode(PIN_EXIT, INPUT);
}

void loop() {
  // Neuer Durchlauf: sauberer Untergrund, danach nur noch Partial-Updates.
  panelReset();
  renderBar(0);
  EPD_Display(ImageBW);
  EPD_FastUpdate();
  Serial.println("Durchlauf gestartet");

  for (int done = 1; done <= STEPS; done++) {
    if (waitOrExit(STEP_MS)) {
      Serial.println("EXIT waehrend des Durchlaufs - Neustart");
      return;                              // loop() beginnt von vorn
    }

    renderBar(done);
    const unsigned long t0 = millis();
    EPD_Display(ImageBW);
    EPD_PartUpdate();                      // kein Reset dazwischen
    Serial.printf("Schritt %d/%d  %lu ms\n", done, STEPS, millis() - t0);
  }

  Serial.println("Fertig - warte auf EXIT");
  while (!exitPressed()) delay(10);
}
