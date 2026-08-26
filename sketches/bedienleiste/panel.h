// panel — alles, was das Bild ans Display bringt. Kennt weder Tasten noch
// Laschen noch Seiten, nur den Puffer und die drei Wege, ihn sichtbar zu machen.
//
// Der Unterschied zwischen den Wegen ist die GEAENDERTE FLAECHE, nicht der
// Anlass:
//
//   panelFenster()     ein Ausschnitt hat sich geaendert  -> Teilrefresh
//   panelSchnell()     das ganze Bild                     -> Fast-Update
//   panelVollrefresh() dazu Schatten wegraeumen           -> Loeschzyklus
//
// panelFenster() ist der Grund, warum es diese Datei gibt: Es engt vor dem
// Schreiben den RAM-Adressbereich des Controllers ein und uebertraegt nur den
// Ausschnitt. Ein EPD_Display() schreibt dagegen IMMER 27200 Byte, und das
// ueber software-gebangtes SPI (spi.cpp: drei digitalWrite() je Bit, CS je
// Byte) — rund 650.000 digitalWrite-Aufrufe, bevor das Panel ueberhaupt
// anfaengt. Fuer eine Lasche sind es mit Fenster gut 1600 Byte.

#ifndef PANEL_H
#define PANEL_H

#include <Arduino.h>

// Muss einmal in setup() aufgerufen werden, vor allen anderen Funktionen.
void panelStart(uint8_t* puffer);

// Nur dieser Ausschnitt hat sich geaendert. Koordinaten in Bildschirmpixeln
// (0..791), beide Ecken einschliesslich; x wird auf Byte-Grenzen erweitert.
// Fenster ueber beide Panelhaelften werden unterstuetzt — die rechte gehoert
// dem zweiten SSD1683, der seine X-Adressen spiegelverkehrt zaehlt.
//
// Das ist der EINZIGE Weg fuer laufende Aenderungen. Fast-Update mitten im
// Betrieb sieht harmlos aus, treibt das Panel aber schrittweise ins Schwarze:
// 0xC7 laedt keine LUT nach und benutzt die des Teilrefreshs. GxEPD2 fuehrt
// dafuer ein Flag _using_partial_mode und initialisiert bei jedem Moduswechsel
// neu; wir vermeiden den Wechsel stattdessen ganz.
void panelFenster(int x0, int y0, int x1, int y1);

// Das ganze Bild, ohne Loeschzyklus. Ruhig und schnell, raeumt aber keinen
// Schatten weg.
void panelSchnell();

// Loeschzyklus, dann das Bild neu aufbauen. Das Panel steht dabei mehrere
// Sekunden weiss.
void panelVollrefresh();

#endif
