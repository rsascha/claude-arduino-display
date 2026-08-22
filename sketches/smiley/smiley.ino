// smiley — zeichnet ein Smiley-Gesicht auf das CrowPanel 5.79" E-Paper.
//
// Zeigt nebenbei, wie man fehlende Zeichenprimitive selbst ergänzt: Elecrows
// EPD.h kann Linien, Rechtecke und Kreise — aber keine Bögen. Der Mund wird
// deshalb unten aus einzelnen Pixeln gebaut.

#include <Arduino.h>
#include <math.h>
#include "EPD.h"

uint8_t ImageBW[27200];

const int PIN_DISPLAY_POWER = 7;      // schaltet die Panel-Spannung
const uint16_t DISPLAY_ROTATION = 0;  // 0/90/180/270 — EPD.h liefert 180, das steht auf dem Kopf

// Sichtbarer Bereich. Der Puffer ist 800 breit (EPD_W), das Panel zeigt nur 792.
const int SCREEN_W = 792;
const int SCREEN_H = 272;

// --- Sicheres Setzen eines Pixels -------------------------------------------
// Paint_SetPixel() prüft die Koordinaten NICHT und schreibt bei zu großen Werten
// über den Puffer hinaus. Die Parameter sind uint16_t, ein negativer Wert würde
// zu einer riesigen Zahl. Deshalb hier signed rechnen und vorher abfangen.
static void safePixel(int x, int y, uint16_t color) {
  if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
  Paint_SetPixel((uint16_t)x, (uint16_t)y, color);
}

// --- Kreis mit Strichstärke --------------------------------------------------
// EPD_DrawCircle() zeichnet nur 1 Pixel dünn — auf diesem Panel kaum zu sehen.
// Mehrere Kreise ineinander ergeben eine sichtbare Linie.
static void thickCircle(int cx, int cy, int r, int thickness, uint16_t color) {
  for (int t = 0; t < thickness; t++) {
    EPD_DrawCircle(cx, cy, r + t, color, 0);   // mode 0 = nur Umriss
  }
}

// --- Bogen -------------------------------------------------------------------
// Fehlt in EPD.h komplett. Winkel in Grad, y zeigt nach unten: 0° ist rechts,
// 90° unten. Ein Bogen von 30° bis 150° ist also ein Lächeln.
// Die Schrittweite richtet sich nach dem Radius, damit keine Lücken entstehen.
static void drawArc(int cx, int cy, int r, float startDeg, float endDeg,
                    int thickness, uint16_t color) {
  const float step = 0.5f / (float)r;                    // Bogenmaß pro Schritt ≈ 0,5 px
  const float a0 = startDeg * (float)M_PI / 180.0f;
  const float a1 = endDeg   * (float)M_PI / 180.0f;

  for (float a = a0; a <= a1; a += step) {
    const float c = cosf(a), s = sinf(a);
    for (int t = 0; t < thickness; t++) {
      safePixel((int)lroundf(cx + (r + t) * c),
                (int)lroundf(cy + (r + t) * s), color);
    }
  }
}

void setup() {
  pinMode(PIN_DISPLAY_POWER, OUTPUT);
  digitalWrite(PIN_DISPLAY_POWER, HIGH);

  EPD_GPIOInit();
  Paint_NewImage(ImageBW, EPD_W, EPD_H, DISPLAY_ROTATION, WHITE);
  Paint_Clear(WHITE);

  // Panel physisch löschen — E-Paper behält sonst das alte Bild
  EPD_FastMode1Init();
  EPD_Display_Clear();
  EPD_Update();

  EPD_GPIOInit();
  EPD_FastMode1Init();

  // --- Das Gesicht ---
  // Mittig. Die Höhe (272) begrenzt den Radius, nicht die Breite.
  const int cx = SCREEN_W / 2;   // 396
  const int cy = SCREEN_H / 2;   // 136
  const int faceR = 124;         // 136 - 124 = 12 px Rand oben und unten

  thickCircle(cx, cy, faceR, 4, BLACK);

  // Augen: gefüllte Kreise, leicht oberhalb der Mitte
  EPD_DrawCircle(cx - 46, cy - 42, 15, BLACK, 1);   // mode 1 = gefüllt
  EPD_DrawCircle(cx + 46, cy - 42, 15, BLACK, 1);

  // Mund: Bogen in der unteren Gesichtshälfte
  drawArc(cx, cy - 10, 78, 35.0f, 145.0f, 5, BLACK);

  // --- Ausgeben ---
  EPD_Display(ImageBW);
  EPD_FastUpdate();
  EPD_DeepSleep();
}

void loop() {
}
