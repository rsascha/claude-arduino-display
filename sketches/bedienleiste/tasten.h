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
// Der Drehschalter ist ein Quadratur-Encoder (TM_2024A): IO4 und IO6 sind seine
// beiden Phasen, IO5 der Tastkontakt. Die Phasen werden wie im Elecrow-Beispiel
// einzeln abgefragt — fuer "welche Richtung wurde gedreht" reicht das, ein
// echter Encoder-Decoder waere erst fuer Schrittzaehlung noetig.

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
