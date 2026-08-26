#include "laschen.h"
#include "zeichnen.h"
#include "EPD.h"

// ---------------------------------------------------------------------------
// Lage der Laschen
// ---------------------------------------------------------------------------
// Am Geraet abgelesen: oberer Taster 60..70, Drehschalter 110..150, unterer
// Taster 200..210. KEINE der drei Laschen hat genau die Hoehe ihres Elements —
// entscheidend ist, dass sie auf dessen MITTE sitzt, und die Hoehe richtet sich
// danach, was hineinpassen muss:
//
//   E und M   Die Taster sind nur 11 px hoch, ein Buchstabe in Groesse 24
//             braucht 24 px. Also 32 px, zentriert auf 65 bzw. 205.
//   Rad       Das Rad ist 41 px hoch; mit Pfeil, OK und Pfeil darin stiessen
//             die Pfeilspitzen an den Umriss und aneinander. Also 53 px,
//             zentriert auf 130: 6 px Luft nach aussen, 4 px zwischen den drei
//             Inhalten.
//
// Die Werte gelten fuer die Ausrichtung USB oben / Bedienelemente links, also
// Paint_NewImage(..., 0, ...). Bei jeder anderen Rotation sitzen die Laschen
// nicht mehr neben ihren Tastern.
static const int EXIT_MITTE = 65;
static const int MENU_MITTE = 205;
static const int KLEIN_H    = 32;

static const int RAD_Y0 = 104, RAD_Y1 = 156;  // gezeichnet; das Rad ist 110..150
static const int TAB_W  = LASCHEN_BREITE;
static const int ECKE   = 4;                  // abgerundete Ecken an der Innenseite

// Inhalt der Radlasche, auf Mitte 130 ausgerichtet und unabhaengig von RAD_Y0/Y1:
// Wer die Lasche hoeher oder flacher macht, verschiebt damit nur den Rand.
//
//   Pfeil oben  110..117
//      Luecke   118..121
//   OK          122..137
//      Luecke   138..141
//   Pfeil unten 142..149
//
// Die 4 px Luecken sind nicht Kosmetik. Vorher lagen die drei Inhalte ohne
// Abstand aneinander (zwischen OK und dem unteren Pfeil sogar 0 px). Passiv
// faellt das nicht auf, weil alles dieselbe Farbe hat — invertiert schon:
// Die Zonengrenze lag genau auf der Inhaltskante, der weisse Pfeil stiess mit
// seiner breitesten Zeile an die weisse Flaeche daneben und las sich als KERBE
// im Balken statt als Pfeil. Dasselbe gespiegelt beim unteren, und in der
// OK-Zone wirkten beide schwarzen Pfeile abgeschnitten.
static const int RAD_PFEIL_OBEN_Y  = 110;   // Dreieck 8 px hoch, also 110..117
static const int RAD_OK_Y          = 122;   // Text Groesse 16, also 122..137
static const int RAD_PFEIL_UNTEN_Y = 142;   // Dreieck 8 px hoch, also 142..149
static const int RAD_PFEIL_B       = 15;
static const int RAD_PFEIL_H       = 8;
static const int RAD_OK_TEXT_SZ    = 16;
static const int LASCHEN_TEXT_SZ   = 24;    // Groesse von "E" und "M"

// Die drei Zonen. Gedreht wird oben/unten, gedrueckt in der Mitte — invertiert
// wird nur die betroffene Zone, nicht die ganze Lasche. Jede Zone reicht 2 px
// ueber ihren Inhalt hinaus, damit der invertierte Inhalt ringsum Rand hat.
static const int RAD_ZONE1 = 120;   // erste Zeile der Mittelzone (OK 122..137 + 2)
static const int RAD_ZONE2 = 140;   // erste Zeile der unteren Zone (Pfeil 142..149 + 2)

static const int UMRISS = 2;        // Strichstaerke der Laschenkontur

// ---------------------------------------------------------------------------
// Zeichenhilfen
// ---------------------------------------------------------------------------

// Wie weit die Lasche in dieser Zeile hinter der vollen Breite zurueckbleibt.
// Ergibt die abgerundete Innenkante — eine Lasche mit scharfen Ecken sieht aus
// wie ein abgeschnittenes Rechteck, nicht wie ein Reiter.
static int eckenversatz(int y, int y0, int y1) {
  const int dOben  = y - y0;
  const int dUnten = y1 - y;
  const int d      = (dOben < dUnten) ? dOben : dUnten;
  if (d >= ECKE) return 0;
  // Viertelkreis mit Radius ECKE, ganzzahlig: groesstes b mit b^2 + a^2 <= ECKE^2.
  const int a = ECKE - 1 - d;              // vertikaler Abstand vom Kreismittelpunkt
  int b = 0;
  while ((b + 1) * (b + 1) + a * a <= ECKE * ECKE) b++;
  return ECKE - b;
}

// Grundform einer Lasche: Flaeche in `fuellung`, UMRISS px starker schwarzer
// Rand entlang der Aussenkontur. Die linke Kante bleibt offen — dort sitzt der
// echte Taster, die Lasche soll aus dem Rand herauswachsen.
static void laschenRahmen(int y0, int y1, uint16_t fuellung) {
  for (int y = y0; y <= y1; y++) {
    const int xr = TAB_W - 1 - eckenversatz(y, y0, y1);
    fillRect(0, y, xr, y, fuellung);
    for (int t = 0; t < UMRISS; t++) safePixel(xr - t, y, BLACK);   // Innenkante
  }
  for (int t = 0; t < UMRISS; t++) {                                // oben, unten
    const int yo = y0 + t, yu = y1 - t;
    fillRect(0, yo, TAB_W - 1 - eckenversatz(yo, y0, y1), yo, BLACK);
    fillRect(0, yu, TAB_W - 1 - eckenversatz(yu, y0, y1), yu, BLACK);
  }
}

// Ein gleichschenkliges Dreieck, Spitze oben (rauf = true) oder unten.
static void dreieck(int xm, int y, int breite, int hoehe, bool rauf, uint16_t color) {
  if (hoehe < 1) return;
  if (hoehe == 1) {                       // sonst teilt die Interpolation durch 0
    safePixel(xm, y, color);
    return;
  }
  for (int i = 0; i < hoehe; i++) {
    const int zeile = rauf ? y + i : y + hoehe - 1 - i;
    const int halb  = (breite / 2) * i / (hoehe - 1);
    fillRect(xm - halb, zeile, xm + halb, zeile, color);
  }
}

// Die nutzbare Breite innerhalb des Umrisses — Bezug fuer alles, was mittig in
// einer Lasche sitzt.
static int inhaltsBreite() { return TAB_W - 2 * UMRISS; }

// ---------------------------------------------------------------------------
// Die einzelnen Laschen
// ---------------------------------------------------------------------------

// Lasche mit einem einzelnen Buchstaben — EXIT oben, MENU unten.
static void zeichneBuchstabenLasche(int mitte, const char* buchstabe, bool aktiv) {
  const int y0 = mitte - KLEIN_H / 2;
  const int y1 = y0 + KLEIN_H - 1;
  laschenRahmen(y0, y1, aktiv ? BLACK : WHITE);
  // Groesse 24: 12 px breit, 24 px hoch. In der 32 px hohen Lasche bleiben oben
  // und unten 4 px, davon 2 px Umriss — die Schrift stoesst also nicht an.
  EPD_ShowString((inhaltsBreite() - textWidth(buchstabe, LASCHEN_TEXT_SZ)) / 2,
                 mitte - LASCHEN_TEXT_SZ / 2,
                 buchstabe, LASCHEN_TEXT_SZ, aktiv ? WHITE : BLACK);
}

// Lasche des Drehschalters: hoch, druecken, runter in EINER Form. Er ist ein
// Bedienelement mit drei Kontakten, nicht drei Elemente — drei getrennte
// Laschen wuerden drei Bedienelemente vortaeuschen.
static void zeichneRadLasche(Taste aktiv) {
  laschenRahmen(RAD_Y0, RAD_Y1, WHITE);

  // Nur die betroffene Zone invertieren, damit man sieht, WELCHE der drei
  // Funktionen ausgeloest hat.
  int zy0 = -1, zy1 = -1;
  if (aktiv == T_HOCH)   { zy0 = RAD_Y0 + UMRISS; zy1 = RAD_ZONE1 - 1;    }
  if (aktiv == T_OK)     { zy0 = RAD_ZONE1;       zy1 = RAD_ZONE2 - 1;    }
  if (aktiv == T_RUNTER) { zy0 = RAD_ZONE2;       zy1 = RAD_Y1 - UMRISS;  }
  if (zy0 >= 0)
    for (int y = zy0; y <= zy1; y++)
      fillRect(0, y, TAB_W - 1 - UMRISS - eckenversatz(y, RAD_Y0, RAD_Y1), y, BLACK);

  const int xm = inhaltsBreite() / 2;
  dreieck(xm, RAD_PFEIL_OBEN_Y,  RAD_PFEIL_B, RAD_PFEIL_H, true,
          aktiv == T_HOCH   ? WHITE : BLACK);
  EPD_ShowString(xm - textWidth("OK", RAD_OK_TEXT_SZ) / 2, RAD_OK_Y,
                 "OK", RAD_OK_TEXT_SZ, aktiv == T_OK ? WHITE : BLACK);
  dreieck(xm, RAD_PFEIL_UNTEN_Y, RAD_PFEIL_B, RAD_PFEIL_H, false,
          aktiv == T_RUNTER ? WHITE : BLACK);
}

// ---------------------------------------------------------------------------

void zeichneLaschen(const LaschenZustand& z) {
  zeichneBuchstabenLasche(EXIT_MITTE, z.exitText,
                          z.aktiv == T_EXIT || z.exitInvers);
  zeichneRadLasche(z.aktiv);
  zeichneBuchstabenLasche(MENU_MITTE, "M", z.aktiv == T_MENU);
}
