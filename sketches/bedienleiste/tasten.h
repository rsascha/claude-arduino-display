// tasten — die fuenf Bedienelemente des CrowPanel, entprellt gelesen.
//
// Weiss nichts vom Display: Wer diese Datei benutzt, bekommt Tastendruecke und
// sonst nichts. Das ist Absicht — die Zuordnung "Taste X faerbt Lasche Y" ist
// eine Entscheidung der Anwendung, nicht dieser Ebene.
//
// Alle Taster liegen gegen Masse, gedrueckt ist LOW. Die Pull-ups sitzen mit
// 4,7 kOhm auf der Platine (material/SCHALTPLAN.md), INPUT_PULLUP ist also
// nicht noetig.
//
// Der Drehschalter ist laut Schaltplan ein Quadratur-Encoder (TM_2024A): IO4 und
// IO6 sind seine beiden Phasen, IO5 der Tastkontakt. Am Geraet verhaelt er sich
// aber wie ZWEI GETRENNTE TASTER, und das ist gemessen, nicht vermutet:
//
//   Ruhezustand        A=1 B=1 (beide Pull-ups)
//   runter betaetigt   nur B geht auf 0, A bleibt 1
//   hoch betaetigt     nur A geht auf 0, B bleibt 1
//   Dauer              250..640 ms je Betaetigung
//   Prellen            ein Vorkommnis auf 19 Flanken, 8 us lang
//
// Die Phasen ueberlappen sich NIE. Ein Quadratur-Dekoder waere also nicht nur
// unnoetig, er haette nichts zu dekodieren — es gibt keine Phasenverschiebung,
// aus der sich eine Richtung ergeben koennte. Die Richtung steckt darin, WELCHE
// Leitung zieht.
//
// Gemessen mit einem Mitschnitt in setup(): digitalRead() auf beide Phasen ohne
// Entprellung, Flanken mit micros() in ein Array, Ausgabe erst danach. Waehrend
// der Messung zu drucken haette genau die Flanken verschluckt, um die es ging —
// eine Serial-Zeile dauert bei 115200 Baud rund 3 ms.

#ifndef TASTEN_H
#define TASTEN_H

#include <Arduino.h>

enum Taste { T_KEINE = -1, T_EXIT = 0, T_HOCH, T_OK, T_RUNTER, T_MENU, T_ANZAHL };

// Pin und Name gehoeren zusammen und stehen deshalb in EINER Tabelle. Als zwei
// Parallel-Arrays koennten sie auseinanderlaufen, ohne dass es auffaellt.
struct TasteDef {
  uint8_t     pin;
  const char* name;
};

extern const TasteDef TASTEN[T_ANZAHL];

// Setzt die fuenf Pins auf INPUT.
void tastenInit();

// Liefert die gedrueckte Taste, entprellt — und zwar in BEIDE Richtungen:
// Gemeldet wird erst, was zweimal im Abstand von TASTEN_ENTPRELL_MS gleich
// gelesen wurde. Waehrend eines Prellers bleibt der letzte stabile Wert stehen.
//
// Sind mehrere Tasten gleichzeitig gedrueckt, gewinnt die mit dem kleinsten
// Index (Reihenfolge des enums). Das Panel kann ohnehin nur einen Zustand
// darstellen, und ein Griff, der zwei Taster gleichzeitig trifft, ist an dieser
// Kante nicht vorgesehen.
//
// Der Aufruf kostet TASTEN_ENTPRELL_MS, weil er die zweite Messung abwartet.
Taste tasteStabil();

// Name der Taste, auch fuer T_KEINE gueltig.
const char* tasteName(Taste t);

// GPIO der Taste; 0 fuer T_KEINE.
uint8_t tastePin(Taste t);

const unsigned long TASTEN_ENTPRELL_MS = 15;

#endif
