#include "tasten.h"

const TasteDef TASTEN[T_ANZAHL] = {
  { 1, "EXIT"         },   // oberer Taster an der Kante
  { 4, "Rad hoch"     },   // Drehschalter, Phase 1
  { 5, "Rad druecken" },   // Drehschalter, Tastkontakt
  { 6, "Rad runter"   },   // Drehschalter, Phase 2
  { 2, "MENU"         },   // unterer Taster an der Kante
};

void tastenInit() {
  for (int i = 0; i < T_ANZAHL; i++) pinMode(TASTEN[i].pin, INPUT);
}

// Eine einzelne Momentaufnahme, ungefiltert.
static Taste rohLesen() {
  for (int i = 0; i < T_ANZAHL; i++)
    if (digitalRead(TASTEN[i].pin) == LOW) return (Taste)i;
  return T_KEINE;
}

Taste tasteStabil() {
  static Taste stabil = T_KEINE;

  const Taste erste = rohLesen();
  delay(TASTEN_ENTPRELL_MS);
  if (rohLesen() == erste) stabil = erste;   // zwei gleiche Messungen gelten

  return stabil;                             // sonst bleibt der letzte Stand
}

const char* tasteName(Taste t) {
  if (t < 0 || t >= T_ANZAHL) return "keine";
  return TASTEN[t].name;
}

uint8_t tastePin(Taste t) {
  if (t < 0 || t >= T_ANZAHL) return 0;
  return TASTEN[t].pin;
}
