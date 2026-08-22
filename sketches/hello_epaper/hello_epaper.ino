// hello_epaper — erster eigener Sketch für das CrowPanel ESP32 5.79" E-Paper
//
// Zeigt Text in mehreren Größen, einen Rahmen und ein paar Formen.
// Board: ESP32S3 Dev Module, Einstellungen siehe README.md im Projekt.

#include <Arduino.h>
#include "EPD.h"

// Der Bildpuffer. Hier wird gemalt — das Display sieht davon erst mal nichts.
// Größe: EPD_W (800) * EPD_H (272) / 8 Pixel pro Bit = 27200 Byte.
//
// Warum 800 und nicht 792? Das Panel hat zwei SSD1683-Controller, je einer für
// eine Hälfte. Zwischen den Hälften liegen im Speicher 8 ungenutzte Pixel.
// Paint_SetPixel() rechnet das heraus ("if (Xpoint >= 396) Xpoint += 8").
// Für dich heißt das: gültige x-Werte sind 0..791, wie erwartet.
uint8_t ImageBW[27200];

// Pin 7 schaltet die Stromversorgung des Panels. Ohne HIGH bleibt es dunkel,
// egal wie korrekt der Rest ist — das ist die häufigste Stolperfalle.
const int PIN_DISPLAY_POWER = 7;

// Ausrichtung des Bildes: 0, 90, 180 oder 270.
// EPD.h bringt ein #define Rotation 180 mit — das stand bei uns auf dem Kopf.
// Der Wert wird hier bewusst im Sketch gesetzt, damit die Vendor-Datei
// unverändert bleibt und die Stellschraube in deinem Code sichtbar ist.
const uint16_t DISPLAY_ROTATION = 0;

void setup() {
  pinMode(PIN_DISPLAY_POWER, OUTPUT);
  digitalWrite(PIN_DISPLAY_POWER, HIGH);

  EPD_GPIOInit();                                          // SPI-Pins konfigurieren

  // ---- Puffer anlegen und komplett weiß füllen ----
  Paint_NewImage(ImageBW, EPD_W, EPD_H, DISPLAY_ROTATION, WHITE);
  Paint_Clear(WHITE);

  // ---- Panel einmal physisch löschen ----
  // E-Paper behält sein Bild ohne Strom. Was vorher drauf war, muss aktiv weg,
  // sonst überlagern sich altes und neues Bild.
  EPD_FastMode1Init();
  EPD_Display_Clear();
  EPD_Update();

  // Nach dem Löschen neu initialisieren (so machen es auch die Elecrow-Beispiele)
  EPD_GPIOInit();
  EPD_FastMode1Init();

  // ---- Jetzt in den Puffer malen ----

  // Rahmen: (x1,y1) oben links, (x2,y2) unten rechts, Farbe, mode 0 = nur Umriss
  EPD_DrawRectangle(4, 4, 787, 267, BLACK, 0);

  // Text: (x, y, Text, Schriftgröße, Farbe)
  // Erlaubte Größen sind nur 8, 12, 16, 24 und 48 — andere Werte zeichnen nichts.
  // Jedes Zeichen ist size/2 breit, "Hallo E-Paper!" in 48 also ca. 14*24 = 336 px.
  EPD_ShowString(40,  40, "Hallo E-Paper!",       48, BLACK);
  EPD_ShowString(40, 110, "CrowPanel ESP32 5.79 Zoll",  24, BLACK);
  EPD_ShowString(40, 145, "272 x 792 Pixel, schwarz/weiss", 16, BLACK);

  // Trennlinie: (x1,y1) nach (x2,y2)
  EPD_DrawLine(40, 175, 500, 175, BLACK);

  // Gefüllter Kasten: mode 1 = ausgefüllt
  EPD_DrawRectangle(40, 195, 140, 245, BLACK, 1);

  // Kreis: (Mittelpunkt x, y, Radius, Farbe, mode 0 = Umriss)
  EPD_DrawCircle(200, 220, 25, BLACK, 0);
  EPD_DrawCircle(280, 220, 25, BLACK, 1);

  // Zahl: (x, y, Wert, Anzahl Stellen, Schriftgröße, Farbe)
  EPD_ShowNum(600, 200, 2026, 4, 48, BLACK);

  // ---- Puffer ans Display schicken ----
  EPD_Display(ImageBW);   // Daten übertragen
  EPD_FastUpdate();       // Panel anweisen, das Bild sichtbar zu machen

  // Display schlafen legen. Das Bild bleibt stehen — E-Paper braucht nur zum
  // Umschalten Strom, nicht zum Halten.
  EPD_DeepSleep();
}

void loop() {
  // Bleibt leer: das Bild steht, es gibt nichts zu wiederholen.
}
