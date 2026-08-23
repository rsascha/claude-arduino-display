#pragma once
#include <Arduino.h>
#include "EPD.h"

// Zeichenhilfen, die beide Bildschirme brauchen. Als eigene Uebersetzungseinheit
// und nicht in der .ino, weil sie sonst in ha_kacheln und ha_wetter doppelt
// stuenden — und weil eine .ino mit beiden Bildschirmen darin 900 Zeilen haette.
//
// Zur Erinnerung, warum es diese Funktionen ueberhaupt gibt: EPD.h kennt Linie,
// Rechteck und Kreis. Keine Bereichspruefung, keine Strichstaerke, keine
// gefuellten Dreiecke, keine Boegen.

const int SCREEN_W = 792;    // sichtbar; der Puffer (EPD_W) ist 800 breit
const int SCREEN_H = 272;

// Aussenrahmen und Fusszeile sind auf beiden Bildschirmen gleich.
const int FRAME_X0 = 2, FRAME_X1 = SCREEN_W - 3;   // 789
const int FRAME_Y0 = 2, FRAME_Y1 = SCREEN_H - 3;   // 269
const int GRID_Y1  = 247;                          // untere Rasterlinie
const int PAD_X     = 8;
const int FOOTER_SZ = 16;
const int FOOTER_Y  = GRID_Y1 + 5;                 // 252, Textzeile 252..268

// Paint_SetPixel() prueft seine Koordinaten NICHT und schreibt sonst ueber den
// Puffer hinaus; die Parameter sind uint16_t, ein negativer Wert wuerde zu einer
// riesigen Zahl. Deshalb signed rechnen und vorher abfangen.
void safePixel(int x, int y, uint16_t color);
void fillRect(int x0, int y0, int x1, int y1, uint16_t color);
void safeLine(int x0, int y0, int x1, int y1, uint16_t color);

// Eine 1 px starke Linie ist auf diesem Panel kaum zu sehen.
void thickLine(int x0, int y0, int x1, int y1, int th, uint16_t color);

// Eigene Kreisfunktionen statt EPD_DrawCircle(): das prueft seine Koordinaten
// nicht, kennt keine Strichstaerke und kann nichts in Weiss aus einer Flaeche
// herausschneiden — beides braucht der Icon-Code.
void fillDisc(int cx, int cy, int r, uint16_t color);
void drawRing(int cx, int cy, int r, int th, uint16_t color);
void fillTriangle(float x0, float y0, float x1, float y1,
                  float x2, float y2, uint16_t color);

// Bildschirmkoordinaten zu einem Winkel in Grad, 0 = oben.
void polar(int cx, int cy, float deg, float r, float& x, float& y);

// Das Grad-Zeichen fehlt im Font (ASCII 32..126), also selbst zeichnen.
void drawDegreeSign(int cx, int cy, int r, int thickness);

// dir > 0 steigend, dir < 0 fallend, dir == 0 unveraendert.
void drawTrendArrow(int x, int y, int w, int h, int dir);

// EPD_ShowString() bricht nicht um; Zeichenbreite ist size/2.
int  textWidth(const char* s, int size);
void showCentered(int cx, int y, const char* s, int size);

// Zahlen: Dezimalkomma statt Punkt, und nach Komma oder Punkt nur size/4
// vorruecken statt size/2. Der Font ist dickte-gleich, die Tinte des Kommas
// belegt aber nur die ersten rund 30 % der Zelle — ohne das sieht "23,1" aus,
// als stuende dort ein Leerzeichen. Weiter zusammenruecken geht nicht:
// EPD_ShowChar() malt die ganze Zelle inklusive Hintergrund und wuerde das
// Komma sonst wieder ausradieren.
void commaDecimal(char* s);
int  numberWidth(const char* s, int size);
void showNumber(int x, int y, const char* s, int size, uint16_t color);
void showNumberCentered(int cx, int y, const char* s, int size);

// Rahmen mit senkrechten Trennlinien an den Spaltengrenzen colX[1..ncol-1].
void drawFrame(const int* colX, int ncol);

// Fusszeile: links der Zeitstempel, mittig der Hinweis auf EXIT, rechts der
// bildschirmeigene Text.
void drawFooter(const char* rightHint);
