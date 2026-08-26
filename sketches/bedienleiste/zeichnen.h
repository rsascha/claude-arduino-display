// zeichnen — die Primitive, die EPD.h fehlen oder die man nicht direkt
// benutzen darf. Von hier hängt nichts ab: kein Tastenwissen, kein Layout.

#ifndef ZEICHNEN_H
#define ZEICHNEN_H

#include <Arduino.h>

const int SCREEN_W = 792;   // sichtbar; der Puffer (EPD_W) ist 800 breit
const int SCREEN_H = 272;

// Paint_SetPixel() prueft seine Koordinaten NICHT und rechnet mit uint16_t: ein
// negativer Wert wird zu einer riesigen Zahl und schreibt irgendwohin in den
// Speicher. Jeder eigene Zeichencode braucht deshalb diesen Wrapper, der in
// int rechnet und vorher abfaengt.
void safePixel(int x, int y, uint16_t color);

// Gefuelltes Rechteck, beide Ecken einschliesslich. EPD_DrawRectangle() zieht im
// gefuellten Modus nur bis Yend-1 — die letzte Zeile fehlt dort.
void fillRect(int x0, int y0, int x1, int y1, uint16_t color);

// EPD_ShowString() bricht nicht um; Zeichenbreite ist size/2.
int textWidth(const char* s, int size);

#endif
