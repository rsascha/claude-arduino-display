// seiten — der rechte Bereich des Panels, umschaltbar.
//
// Die Laschen links gehoeren `laschen.cpp`, alles rechts davon gehoert hierher.
// Das ist der Punkt der ganzen Aufteilung: Zwei Ebenen teilen sich ein Panel,
// ohne voneinander zu wissen, und keine loescht der anderen das Bild.
//
// Der Vertrag lautet: Diese Ebene fasst ausschliesslich x >= SEITEN_X0 an, ruft
// weder Paint_NewImage() noch Paint_Clear() und haelt keinen eigenen Zustand.
// Was gezeichnet wird, kommt als SeitenDaten herein.

#ifndef SEITEN_H
#define SEITEN_H

#include <Arduino.h>
#include "tasten.h"

enum Seite { S_MENUE = 0, S_TASTEN, S_REFRESH, S_MUSTER, S_ANZAHL };

extern const char* const SEITEN_TITEL[S_ANZAHL];

struct SeitenDaten {
  Seite       seite;
  int         auswahl;        // markierter Eintrag auf der Menueseite (0..S_ANZAHL-2)
  Taste       letzte;         // zuletzt gedrueckte Taste, T_KEINE = noch keine
  const long* zaehler;        // T_ANZAHL Werte, in der Reihenfolge des enums
  int         teilbilder;     // Teilrefreshs seit dem letzten Vollrefresh
  bool        refreshScharf;  // EXIT ist scharf, zeigt "R"
};

// Zeichnet den rechten Bereich. Loescht ihn NICHT — das macht der Aufrufer mit
// seitenBereichLoeschen(), und zwar nur dann, wenn der Inhalt wechselt.
void zeichneSeite(const SeitenDaten& d);

// Weisst den Inhaltsbereich, ohne die Laschen anzufassen.
void seitenBereichLoeschen();

// Linke Kante des Inhaltsbereichs. Alles links davon gehoert den Laschen.
const int SEITEN_X0 = 70;

#endif
