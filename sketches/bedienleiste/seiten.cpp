#include "seiten.h"
#include "zeichnen.h"
#include "EPD.h"

const char* const SEITEN_TITEL[S_ANZAHL] = {
  "Menue", "Tasten", "Refresh", "Muster",
};

// --- Layout des Inhaltsbereichs ---------------------------------------------
static const int TITEL_Y  = 20, TITEL_SZ = 24;
static const int ZEILE_Y0 = 62;          // erste Textzeile unter dem Titel
static const int ZEILE_H  = 22;          // Groesse 16 braucht 16 px, 6 px Luft
static const int TEXT_SZ  = 16;
static const int FUSS_Y   = 244;
static const int RAND_X   = SCREEN_W - 10;   // rechte Kante des Inhaltsbereichs
static const int EINZUG   = 6;               // Text innerhalb einer Markierung

// Wie viele Textzeilen zwischen Trennlinie und Fusszeile Platz haben. Eine
// Zeile braucht TEXT_SZ Pixel Hoehe, nicht weniger — in ha_verlauf hatten sich
// zwei Zeilen um 6 px ueberlappt, weil das zu knapp gerechnet war, und beim
// ersten Entwurf dieser Seite passierte es prompt wieder (Zeile 8 lief in die
// Fusszeile). Deshalb steht die Grenze hier als Zahl, statt sie zu schaetzen.
static const int ZEILEN_MAX = (FUSS_Y - 6 - ZEILE_Y0 - TEXT_SZ) / ZEILE_H;   // = 7

static int zeileY(int n) { return ZEILE_Y0 + n * ZEILE_H; }

static void zeile(int n, const char* s) {
  EPD_ShowString(SEITEN_X0, zeileY(n), s, TEXT_SZ, BLACK);
}

// ---------------------------------------------------------------------------

void seitenBereichLoeschen() {
  // Erst ab SEITEN_X0: Die Laschen stehen schon im Puffer und sollen dort
  // bleiben. Genau das ist der Unterschied zu einem Paint_Clear(), das alles
  // wegnimmt — und der Grund, warum die Leiste ueberhaupt wiederverwendbar ist.
  fillRect(SEITEN_X0, 0, SCREEN_W - 1, SCREEN_H - 1, WHITE);
}

// --- Kopf- und Fusszeile, auf allen Seiten gleich ---------------------------

static void kopf(const SeitenDaten& d) {
  EPD_ShowString(SEITEN_X0, TITEL_Y, SEITEN_TITEL[d.seite], TITEL_SZ, BLACK);
  // Trennlinie unter dem Titel, 2 px — 1 px ist auf dem Panel kaum zu sehen.
  fillRect(SEITEN_X0, TITEL_Y + TITEL_SZ + 8, RAND_X, TITEL_Y + TITEL_SZ + 9, BLACK);
}

static void fuss(const char* s) {
  EPD_ShowString(SEITEN_X0, FUSS_Y, s, TEXT_SZ, BLACK);
}

// --- Die einzelnen Seiten ---------------------------------------------------

// Menue: die uebrigen Seiten als Liste, der markierte Eintrag invertiert.
// Invertiert und nicht mit einem Pfeil davor, weil ein einzelnes Zeichen am
// Zeilenanfang auf dem Panel aus zwei Metern nicht zu finden ist.
static void seiteMenue(const SeitenDaten& d) {
  for (int i = 1; i < S_ANZAHL; i++) {
    const int n  = i - 1;
    const int y  = zeileY(n);
    const bool markiert = (d.auswahl == n);

    // Der Balken beginnt bei SEITEN_X0, NICHT davor. Er ragte anfangs 6 px nach
    // links heraus — dorthin reicht seitenBereichLoeschen() nicht, und jede
    // Position, auf der die Markierung einmal stand, liess dort einen schwarzen
    // Stummel zurueck. Weil die Zeilen 22 px auseinanderliegen und der Balken
    // 22 px hoch ist, wuchsen die Stummel zu einem durchgehenden Balken ueber
    // die ganze Liste zusammen. Der Vertrag dieser Ebene lautet x >= SEITEN_X0,
    // und er gilt auch fuer 6 Pixel.
    if (markiert) fillRect(SEITEN_X0, y - 3, SEITEN_X0 + 240, y + TEXT_SZ + 2, BLACK);
    EPD_ShowString(SEITEN_X0 + EINZUG, y, SEITEN_TITEL[i], TEXT_SZ,
                   markiert ? WHITE : BLACK);
  }

  zeile(S_ANZAHL, "Rad hoch/runter waehlt, OK oeffnet.");
  fuss("MENU fuehrt von jeder Seite hierher zurueck.");
}

// Tasten: wofuer der Sketch urspruenglich da war — welches Element sitzt wo und
// zieht welchen GPIO.
static void seiteTasten(const SeitenDaten& d) {
  zeile(0, "Die Laschen liegen auf der Hoehe der echten Taster.");
  zeile(1, "Druecken faerbt die betroffene Lasche schwarz.");

  char z[80];
  for (int i = 0; i < T_ANZAHL; i++) {
    snprintf(z, sizeof(z), "%-14s GPIO %-3d %ld mal",
             TASTEN[i].name, TASTEN[i].pin, d.zaehler[i]);
    zeile(3 + i, z);
  }

  snprintf(z, sizeof(z), "Zuletzt: %s",
           d.letzte == T_KEINE ? "noch nichts" : tasteName(d.letzte));
  fuss(z);
}

// Refresh: was beim Bedienen tatsaechlich mit dem Panel passiert.
static void seiteRefresh(const SeitenDaten& d) {
  zeile(0, "Nur eine Lasche: RAM-Fenster x 0..45  ->  1632 Byte");
  zeile(1, "Ganze Seite:     EPD_Display()        -> 27200 Byte");
  zeile(2, "E, dann R:       Loeschzyklus, mehrere Sekunden weiss");

  char z[80];
  snprintf(z, sizeof(z), "Teilbilder seit dem letzten Vollrefresh: %d", d.teilbilder);
  zeile(4, z);
  zeile(5, d.refreshScharf ? "EXIT ist scharf - R wischt, andere Taste bricht ab."
                           : "EXIT einmal druecken macht daraus R.");

  zeile(7, "Diese Seite zeigt Live-Zahlen: hier kostet jeder Druck die");
  fuss("volle Breite. Auf Menue und Muster genuegt das Fenster.");
}

// Muster: grosse Flaechen, damit zwei Dinge sichtbar werden — ob die Laschen
// beim Seitenwechsel unangetastet bleiben, und wie viel Schatten ein Wechsel
// von Schwarz auf Weiss hinterlaesst.
static void seiteMuster(const SeitenDaten&) {
  const int y0 = 62, y1 = 214;
  const int x0 = SEITEN_X0, x1 = RAND_X;
  const int felder = 6;
  const int breite = (x1 - x0) / felder;

  for (int i = 0; i < felder; i++) {
    const int fx0 = x0 + i * breite;
    const int fx1 = (i == felder - 1) ? x1 : fx0 + breite - 1;
    if (i % 2 == 0) {
      fillRect(fx0, y0, fx1, y1, BLACK);
    } else {
      // 3 px stark: Ein 1-px-Umriss ist auf diesem Panel kaum zu erkennen —
      // dieselbe Beobachtung wie bei EPD_DrawCircle().
      for (int t = 0; t < 3; t++) EPD_DrawRectangle(fx0 + t, y0 + t, fx1 - t, y1 - t, BLACK, 0);
    }
  }

  fuss("Bleiben die Laschen links unberuehrt? Dann haelt die Grenze.");
}

// ---------------------------------------------------------------------------

void zeichneSeite(const SeitenDaten& d) {
  kopf(d);
  switch (d.seite) {
    case S_MENUE:   seiteMenue(d);   break;
    case S_TASTEN:  seiteTasten(d);  break;
    case S_REFRESH: seiteRefresh(d); break;
    case S_MUSTER:  seiteMuster(d);  break;
    default: break;
  }
}
