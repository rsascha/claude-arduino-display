// bedienleiste — die drei Bedienelemente der linken Gehaeusekante als Laschen,
// daneben ein Inhaltsbereich, der sich umschalten laesst.
//
// Oben EXIT ("E"), unten MENU ("M"), dazwischen der Drehschalter. Wer eine
// Taste drueckt, sieht seine Lasche schwarz werden. Der Bereich rechts zeigt
// eine von vier Seiten; das Rad blaettert, OK oeffnet, MENU fuehrt zurueck.
//
// Aufgeteilt auf fuenf Ebenen, damit die Leiste in anderen Sketches verwendbar
// bleibt:
//
//   zeichnen.cpp  safePixel(), fillRect(), textWidth() — Primitive, sonst nichts
//   tasten.cpp    GPIOs und Entprellung — weiss nichts vom Display
//   laschen.cpp   die Laschen, x = 0..45 — weiss nichts von Tasten und Seiten
//   seiten.cpp    der Inhaltsbereich, x >= 70 — weiss nichts von den Laschen
//   panel.cpp     bringt Puffer aufs Panel: Fenster, Fast-Update, Vollrefresh
//   diese Datei   Navigation, Zaehler, Refresh-Politik
//
// Der Vertrag zwischen den beiden Zeichenebenen ist eine SPALTE: Die Laschen
// fassen nur x < 46 an, die Seiten nur x >= 70. Keine von beiden ruft
// Paint_Clear(). Deshalb laesst sich der Inhalt wechseln, ohne die Laschen neu
// zu zeichnen — und die Laschen invertieren, ohne den Inhalt anzufassen.
// Nachpruefbar auf der Seite "Muster": Sie faerbt den halben Bereich schwarz,
// und die Laschen bleiben davon unberuehrt.
//
// Kein WLAN, keine secrets.h.

#include <Arduino.h>
#include "EPD.h"
#include "zeichnen.h"
#include "tasten.h"
#include "laschen.h"
#include "seiten.h"
#include "panel.h"

uint8_t ImageBW[27200];

const int      PIN_DISPLAY_POWER = 7;
const uint16_t DISPLAY_ROTATION  = 0;

// So lange bleibt das "R" scharf, danach faellt es von selbst auf "E" zurueck.
const unsigned long SCHARF_MS = 5000;

// ---------------------------------------------------------------------------
// Zustand
// ---------------------------------------------------------------------------

static Seite seite   = S_MENUE;
static int   auswahl = 0;                  // markierter Eintrag im Menue
static long  zaehler[T_ANZAHL] = { 0, 0, 0, 0, 0 };
static Taste letzte  = T_KEINE;
static int   teilbilder = 0;

// Der Vollrefresh braucht zwei Druecker: Der erste auf EXIT macht aus dem "E"
// ein "R", erst der zweite wischt. Ein Vollrefresh laesst das Panel mehrere
// Sekunden weiss stehen — wer nur antippen wollte, ob die Lasche reagiert, soll
// dabei nicht das halbe Display ausknipsen. Jede andere Taste nimmt das "R"
// zurueck, und nach SCHARF_MS verfaellt es von selbst: Ein Bedienelement, das
// dauerhaft in einem vergessenen Sonderzustand steht, loest beim naechsten
// beilaeufigen Druck etwas aus, das man nicht wollte.
//
// Dass ausgerechnet EXIT diese Rolle hat, ist eine Entscheidung DIESER
// Anwendung und steht deshalb hier und nicht in laschen.cpp. In ha_wechsel
// blaettert EXIT um.
static bool          refreshScharf = false;
static unsigned long scharfSeit    = 0;

// Der Anfangszustand. Steht in einer eigenen Funktion, weil ihn ZWEI Wege
// brauchen — der Start und der Vollrefresh. Zweimal hingeschrieben wuerde er
// beim naechsten neuen Feld auseinanderlaufen.
static void zustandZuruecksetzen() {
  seite         = S_MENUE;
  auswahl       = 0;
  letzte        = T_KEINE;
  teilbilder    = 0;
  refreshScharf = false;
  for (int i = 0; i < T_ANZAHL; i++) zaehler[i] = 0;
}

// ---------------------------------------------------------------------------
// Zeichnen
// ---------------------------------------------------------------------------

static void laschenZeichnen(Taste aktiv) {
  LaschenZustand z;
  z.aktiv      = aktiv;
  z.exitText   = refreshScharf ? "R" : "E";
  z.exitInvers = refreshScharf;
  zeichneLaschen(z);
}

static void seiteZeichnen() {
  SeitenDaten d;
  d.seite         = seite;
  d.auswahl       = auswahl;
  d.letzte        = letzte;
  d.zaehler       = zaehler;
  d.teilbilder    = teilbilder;
  d.refreshScharf = refreshScharf;

  seitenBereichLoeschen();     // nur rechts, die Laschen bleiben stehen
  zeichneSeite(d);
}

// ---------------------------------------------------------------------------
// Anzeigen
// ---------------------------------------------------------------------------
//
// Welcher der drei Wege faellig ist, haengt an der GEAENDERTEN FLAECHE:
//
//   nur eine Lasche      -> panelFenster() ueber den linken Streifen
//   die ganze Seite      -> panelSchnell()
//   Schatten wegraeumen  -> panelVollrefresh()
//
// Das Fenster ist der Grund fuer den ganzen Umbau: Ein EPD_Display() schreibt
// immer 27200 Byte ueber software-gebangtes SPI. Der linke Streifen sind
// 6 Byte je Zeile mal 272 Zeilen = 1632 — Faktor 17.

// Ein Inhaltswechsel aendert beide Bereiche: die Seite UND die Lasche der
// gedrueckten Taste. Zwei Fenster waeren zwei Refresh-Zyklen — also eines ueber
// alles. Daten spart das nicht, es geht um etwas anderes: Es bleibt bei EINEM
// Update-Modus.
//
// Ein Fast-Update mitten im Betrieb sieht harmlos aus, trieb das Panel am Geraet
// aber schrittweise ins Schwarze: Beim ersten Inhaltswechsel kam der neue
// schwarze Block nur grau, beim zweiten war fast alles schwarz. 0xC7 laedt keine
// LUT nach und benutzt die des Teilrefreshs. GxEPD2 fuehrt fuer diesen Fall ein
// Flag _using_partial_mode und initialisiert bei jedem Moduswechsel neu — wir
// vermeiden den Wechsel stattdessen ganz.
static void zeigeAlles() {
  teilbilder++;
  panelFenster(0, 0, SCREEN_W - 1, SCREEN_H - 1);
}

// Zeigt eine Seite Live-Zahlen? Dann aendert sich dort bei jedem Tastendruck
// auch der Inhalt, nicht nur die Lasche.
static bool seiteIstLive(Seite s) { return s == S_TASTEN || s == S_REFRESH; }

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

// Verarbeitet einen Tastendruck. Liefert true, wenn sich der INHALT geaendert
// hat — davon haengt ab, welcher Refresh faellig ist.
static bool navigieren(Taste t) {
  if (t == T_MENU) {
    const bool geaendert = (seite != S_MENUE);
    seite = S_MENUE;
    return geaendert;
  }

  if (seite == S_MENUE) {
    const int letzterEintrag = S_ANZAHL - 2;      // ohne das Menue selbst
    if (t == T_HOCH)   { auswahl = (auswahl > 0) ? auswahl - 1 : letzterEintrag; return true; }
    if (t == T_RUNTER) { auswahl = (auswahl < letzterEintrag) ? auswahl + 1 : 0; return true; }
    if (t == T_OK)     { seite = (Seite)(auswahl + 1); return true; }
    return false;
  }

  // Auf einer Inhaltsseite blaettert das Rad direkt weiter, OK fuehrt zurueck.
  if (t == T_HOCH)   { seite = (seite > S_TASTEN) ? (Seite)(seite - 1) : (Seite)(S_ANZAHL - 1); return true; }
  if (t == T_RUNTER) { seite = (seite < S_ANZAHL - 1) ? (Seite)(seite + 1) : S_TASTEN; return true; }
  if (t == T_OK)     { auswahl = seite - 1; seite = S_MENUE; return true; }
  return false;
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n--- bedienleiste ---");

  pinMode(PIN_DISPLAY_POWER, OUTPUT);
  digitalWrite(PIN_DISPLAY_POWER, HIGH);   // ohne das bleibt das Panel dunkel

  tastenInit();
  zustandZuruecksetzen();

  // Einmal den ganzen Puffer aufsetzen. Danach zeichnet jede Ebene nur noch in
  // ihre Spalte — Paint_NewImage() und Paint_Clear() kommen nicht wieder vor.
  Paint_NewImage(ImageBW, EPD_W, EPD_H, DISPLAY_ROTATION, WHITE);
  Paint_Clear(WHITE);
  seiteZeichnen();
  laschenZeichnen(T_KEINE);

  panelStart(ImageBW);
  panelVollrefresh();
}

void loop() {
  static Taste angezeigt = T_KEINE;

  // tasteStabil() wartet die zweite Messung ab und gibt dem Durchlauf damit
  // seinen Takt — ein zusaetzliches delay() braucht die Schleife nicht.
  const Taste jetzt = tasteStabil();

  if (jetzt == angezeigt) {
    // Nichts Neues an den Tasten — hier laeuft nur die Verfallszeit des "R".
    // Sie zaehlt erst, wenn keine Taste mehr gedrueckt ist: Wer EXIT festhaelt,
    // ist noch am Bedienen, und ihm den Zustand unter der Hand wegzunehmen waere
    // das Gegenteil dessen, was die Frist bezwecken soll.
    if (refreshScharf && jetzt == T_KEINE && millis() - scharfSeit >= SCHARF_MS) {
      refreshScharf = false;
      Serial.println("R verfallen");
      laschenZeichnen(angezeigt);
      zeigeAlles();
    }
    return;
  }

  bool wischen      = false;
  bool inhaltNeu    = false;

  if (jetzt != T_KEINE) {
    zaehler[jetzt]++;
    letzte = jetzt;
    Serial.printf("%s (GPIO %d)\n", tasteName(jetzt), tastePin(jetzt));

    if (jetzt == T_EXIT) {
      // Erster Druck macht scharf, zweiter wischt.
      wischen       = refreshScharf;
      refreshScharf = !refreshScharf;
      if (refreshScharf) scharfSeit = millis();
    } else {
      if (refreshScharf) refreshScharf = false;   // andere Taste bricht ab
      inhaltNeu = navigieren(jetzt);
    }
  }

  // Gewischt wird das RUHEBILD, nicht die gedrueckte Lasche. Sonst friert der
  // Vollrefresh die schwarze Lasche ein, und das Loslassen muesste sie per
  // Teilrefresh wieder wegnehmen — ein grosser Schwarz-nach-Weiss-Sprung, genau
  // das, was Partial am schlechtesten kann. So ist nach dem Wischen nichts mehr
  // zu tun: `angezeigt` steht bereits auf T_KEINE.
  angezeigt = wischen ? T_KEINE : jetzt;

  // Eine Live-Seite aendert sich bei jedem Druck mit, nicht nur die Lasche.
  if (jetzt != T_KEINE && seiteIstLive(seite)) inhaltNeu = true;

  if (wischen) {
    // Der Vollrefresh raeumt nicht nur das Panel auf, sondern auch den Zustand:
    // danach steht alles wie nach dem Flashen — Menue, Auswahl oben, Zaehler
    // auf null. Ein sauberes Panel mit halb gelaufenen Zaehlern waere ein
    // Zwischending, das beim Messen nur verwirrt.
    Serial.println("Vollrefresh - Zustand zurueckgesetzt");
    zustandZuruecksetzen();
    seiteZeichnen();
    laschenZeichnen(angezeigt);
    panelVollrefresh();
  } else if (inhaltNeu) {
    seiteZeichnen();
    laschenZeichnen(angezeigt);
    zeigeAlles();
  } else {
    laschenZeichnen(angezeigt);
    zeigeAlles();
  }
}
