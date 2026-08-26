// bedienleiste — die drei Bedienelemente an der linken Gehaeusekante als
// Laschen auf dem Panel, direkt auf ihrer Hoehe.
//
// Oben EXIT ("E"), unten MENU ("M"), dazwischen der Drehschalter. Wer eine
// Taste drueckt, sieht seine Lasche schwarz werden — damit ist am Geraet ohne
// Serial-Monitor pruefbar, welches Element wo sitzt und welchen GPIO es zieht.
//
// Die y-Positionen sind am Geraet abgelesen, mit einem Wegwerf-Sketch, der an
// beiden langen Kanten ein Massband von 0 bis 271 zeichnet. Sie sind kein
// Rechenergebnis und gelten fuer die Ausrichtung USB oben / Bedienelemente
// links, also DISPLAY_ROTATION 0.
//
// Warum Partial-Refresh: Es aendert sich pro Tastendruck nur eine Lasche —
// genau der Fall, fuer den EPD_PartUpdate() gedacht ist. Fuer einen
// Vollbildwechsel taugt er nicht (siehe ha_umschalten). EXIT ist zugleich die
// Refresh-Taste: Der erste Druck macht aus dem "E" ein "R", der zweite wischt
// das Panel durch. Zwischen den Updates
// darf NICHT neu initialisiert werden: EPD_FastMode1Init() enthaelt einen
// Hardware-Reset, und ein zurueckgesetzter Controller kennt das vorherige Bild
// nicht mehr — worauf Partial gerade aufbaut.
//
// Kein WLAN, keine secrets.h.

#include <Arduino.h>
#include "EPD.h"

uint8_t ImageBW[27200];

const int      PIN_DISPLAY_POWER = 7;
const uint16_t DISPLAY_ROTATION  = 0;

const int SCREEN_W = 792;   // sichtbar; der Puffer (EPD_W) ist 800 breit
const int SCREEN_H = 272;

// --- Bedienelemente ---------------------------------------------------------
// Alle gegen Masse, gedrueckt ist LOW. Die Pull-ups sitzen mit 4,7 kOhm auf der
// Platine (material/SCHALTPLAN.md), INPUT_PULLUP ist also nicht noetig.
// Der Drehschalter ist ein Quadratur-Encoder (TM_2024A): IO4 und IO6 sind seine
// beiden Phasen, IO5 der Tastkontakt. Hier werden die Phasen wie im
// Elecrow-Beispiel einzeln abgefragt — fuer "welche Richtung wurde gedreht"
// reicht das, ein echter Encoder-Decoder waere erst fuer Schrittzaehlung noetig.
const int PIN_EXIT     = 1;
const int PIN_RAD_HOCH = 4;
const int PIN_RAD_OK   = 5;
const int PIN_RAD_RUNT = 6;
const int PIN_MENU     = 2;

enum Taste { T_KEINE = -1, T_EXIT = 0, T_HOCH, T_OK, T_RUNTER, T_MENU, T_ANZAHL };

const int  tastePin[T_ANZAHL]  = { PIN_EXIT, PIN_RAD_HOCH, PIN_RAD_OK, PIN_RAD_RUNT, PIN_MENU };
const char* tasteName[T_ANZAHL] = { "EXIT", "Rad hoch", "Rad druecken", "Rad runter", "MENU" };

// --- Lage der Laschen -------------------------------------------------------
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
const int EXIT_MITTE = 65;
const int MENU_MITTE = 205;
const int KLEIN_H    = 32;

const int RAD_Y0 = 104, RAD_Y1 = 156;   // gezeichnete Lasche; das Rad selbst ist 110..150
const int TAB_W  = 46;    // Breite der Laschen, x = 0 .. TAB_W-1
const int ECKE   = 4;     // abgerundete Ecken an der Innenseite

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
const int RAD_PFEIL_OBEN_Y  = 110;   // Dreieck 8 px hoch, also 110..117
const int RAD_OK_Y          = 122;   // Text Groesse 16, also 122..137
const int RAD_PFEIL_UNTEN_Y = 142;   // Dreieck 8 px hoch, also 142..149
const int RAD_PFEIL_B       = 15;
const int RAD_PFEIL_H       = 8;

// Die drei Zonen. Gedreht wird oben/unten, gedrueckt in der Mitte — invertiert
// wird nur die betroffene Zone, nicht die ganze Lasche. Jede Zone reicht 2 px
// ueber ihren Inhalt hinaus, damit der invertierte Inhalt ringsum Rand hat.
const int RAD_ZONE1 = 120;   // erste Zeile der Mittelzone (OK 122..137 + 2)
const int RAD_ZONE2 = 140;   // erste Zeile der unteren Zone (Pfeil 142..149 + 2)

// --- Textspalte rechts der Laschen ------------------------------------------
const int TEXT_X = 120;

// So lange bleibt das "R" scharf, danach faellt es von selbst auf "E" zurueck.
const unsigned long SCHARF_MS = 5000;

// Der Vollrefresh laeuft nicht nach Zaehler, sondern auf Zuruf: Loslassen von
// EXIT wischt das Panel durch. E-Paper baut im Teilrefresh Schatten auf, und
// dagegen hilft nur der volle Loeschzyklus — der laesst das Panel aber mehrere
// Sekunden weiss. Nach einem Zaehler getaktet trifft das einen mitten im
// Bedienen; auf Zuruf entscheidet der, der hinschaut.

// ---------------------------------------------------------------------------
// Zeichenhilfen
// ---------------------------------------------------------------------------

// Paint_SetPixel() prueft seine Koordinaten NICHT und rechnet mit uint16_t: ein
// negativer Wert wird zu einer riesigen Zahl und schreibt irgendwohin in den
// Speicher. Deshalb signed rechnen und vorher abfangen.
static void safePixel(int x, int y, uint16_t color) {
  if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
  Paint_SetPixel((uint16_t)x, (uint16_t)y, color);
}

static void fillRect(int x0, int y0, int x1, int y1, uint16_t color) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++)
      safePixel(x, y, color);
}

// EPD_ShowString() bricht nicht um; Zeichenbreite ist size/2.
static int textWidth(const char* s, int size) { return (int)strlen(s) * (size / 2); }

// ---------------------------------------------------------------------------
// Laschen
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

// Grundform einer Lasche: Flaeche in `fuellung`, 2 px starker schwarzer Umriss
// entlang der Aussenkontur. Die linke Kante bleibt offen — dort sitzt der echte
// Taster, die Lasche soll aus dem Rand herauswachsen.
static void laschenRahmen(int y0, int y1, uint16_t fuellung) {
  for (int y = y0; y <= y1; y++) {
    const int xr = TAB_W - 1 - eckenversatz(y, y0, y1);
    fillRect(0, y, xr, y, fuellung);
    // Aussenkontur: die zwei Spalten an der Innenkante ...
    safePixel(xr,     y, BLACK);
    safePixel(xr - 1, y, BLACK);
  }
  // ... und je zwei Zeilen oben und unten.
  for (int y = y0; y <= y0 + 1; y++)
    for (int x = 0; x <= TAB_W - 1 - eckenversatz(y, y0, y1); x++) safePixel(x, y, BLACK);
  for (int y = y1 - 1; y <= y1; y++)
    for (int x = 0; x <= TAB_W - 1 - eckenversatz(y, y0, y1); x++) safePixel(x, y, BLACK);
}

// Ein gleichschenkliges Dreieck, Spitze oben (rauf = true) oder unten.
static void dreieck(int xm, int y, int breite, int hoehe, bool rauf, uint16_t color) {
  for (int i = 0; i < hoehe; i++) {
    const int zeile = rauf ? y + i : y + hoehe - 1 - i;
    const int halb  = (breite / 2) * i / (hoehe - 1);
    fillRect(xm - halb, zeile, xm + halb, zeile, color);
  }
}

// Lasche mit einem einzelnen Buchstaben — EXIT oben, MENU unten.
static void zeichneBuchstabenLasche(int mitte, const char* buchstabe, bool aktiv) {
  const int y0 = mitte - KLEIN_H / 2;
  const int y1 = y0 + KLEIN_H - 1;
  laschenRahmen(y0, y1, aktiv ? BLACK : WHITE);
  // Groesse 24: 12 px breit, 24 px hoch. In der 32 px hohen Lasche bleiben oben
  // und unten 4 px, davon 2 px Umriss — die Schrift stoesst also nicht an.
  EPD_ShowString((TAB_W - 4 - textWidth(buchstabe, 24)) / 2, mitte - 12,
                 buchstabe, 24, aktiv ? WHITE : BLACK);
}

// Lasche des Drehschalters: hoch, druecken, runter in einer Form. Die Pfeile
// stehen fuer die beiden Drehrichtungen, "OK" fuer den Druck auf das Rad.
static void zeichneRadLasche(Taste aktiv) {
  laschenRahmen(RAD_Y0, RAD_Y1, WHITE);

  // Nur die betroffene Zone invertieren, damit man sieht, WELCHE der drei
  // Funktionen ausgeloest hat.
  int zy0 = -1, zy1 = -1;
  if (aktiv == T_HOCH)   { zy0 = RAD_Y0 + 2;   zy1 = RAD_ZONE1 - 1; }
  if (aktiv == T_OK)     { zy0 = RAD_ZONE1;    zy1 = RAD_ZONE2 - 1; }
  if (aktiv == T_RUNTER) { zy0 = RAD_ZONE2;    zy1 = RAD_Y1 - 2;    }
  if (zy0 >= 0)
    for (int y = zy0; y <= zy1; y++)
      fillRect(0, y, TAB_W - 3 - eckenversatz(y, RAD_Y0, RAD_Y1), y, BLACK);

  const int xm = (TAB_W - 4) / 2;
  dreieck(xm, RAD_PFEIL_OBEN_Y,  RAD_PFEIL_B, RAD_PFEIL_H, true,
          aktiv == T_HOCH   ? WHITE : BLACK);
  EPD_ShowString(xm - 8, RAD_OK_Y, "OK", 16, aktiv == T_OK ? WHITE : BLACK);
  dreieck(xm, RAD_PFEIL_UNTEN_Y, RAD_PFEIL_B, RAD_PFEIL_H, false,
          aktiv == T_RUNTER ? WHITE : BLACK);
}

// ---------------------------------------------------------------------------
// Bild
// ---------------------------------------------------------------------------

static long zaehler[T_ANZAHL] = { 0, 0, 0, 0, 0 };

// Teilbilder seit dem letzten Vollrefresh — steht mit auf dem Display, damit
// ablesbar ist, wie viel Schatten sich seither aufbauen konnte.
static int teilbilder = 0;

// Der Vollrefresh braucht zwei Druecker: der erste auf EXIT macht aus dem "E"
// ein "R", erst der zweite wischt. Ein Vollrefresh laesst das Panel mehrere
// Sekunden weiss stehen — er soll niemanden ueberraschen, der nur schauen
// wollte, ob die Lasche reagiert. Jede andere Taste nimmt das "R" zurueck, und
// nach SCHARF_MS verfaellt es von selbst: Ein Bedienelement, das dauerhaft in
// einem Sonderzustand steht, den man vergessen hat, loest beim naechsten
// beilaeufigen Druck etwas aus, das man nicht wollte.
static bool          refreshScharf = false;
static unsigned long scharfSeit    = 0;

static void render(Taste aktiv) {
  Paint_NewImage(ImageBW, EPD_W, EPD_H, DISPLAY_ROTATION, WHITE);
  Paint_Clear(WHITE);

  // Das scharfe "R" wird invertiert dargestellt, auch wenn die Taste laengst
  // losgelassen ist: Es ist ein ZUSTAND, kein Tastendruck. Nur den Buchstaben zu
  // tauschen war zu leise — E und R sind beide schmal und stehen an derselben
  // Stelle, der Wechsel ging im Blick auf das ganze Panel unter. Eine schwarze
  // Lasche sieht man aus dem Augenwinkel.
  zeichneBuchstabenLasche(EXIT_MITTE, refreshScharf ? "R" : "E",
                          aktiv == T_EXIT || refreshScharf);
  zeichneRadLasche(aktiv);
  zeichneBuchstabenLasche(MENU_MITTE, "M", aktiv == T_MENU);

  EPD_ShowString(TEXT_X,  24, "Bedienleiste", 24, BLACK);
  EPD_ShowString(TEXT_X,  60, "Die Laschen liegen auf der Hoehe der echten Taster:", 16, BLACK);
  EPD_ShowString(TEXT_X,  80, "E = EXIT (GPIO 1), Rad = GPIO 4/5/6, M = MENU (GPIO 2).", 16, BLACK);
  EPD_ShowString(TEXT_X, 100, "Druecken faerbt die Lasche schwarz - Partial-Refresh.", 16, BLACK);

  char zeile[64];
  if (aktiv == T_KEINE) snprintf(zeile, sizeof(zeile), "bereit");
  else                  snprintf(zeile, sizeof(zeile), "%s", tasteName[aktiv]);
  EPD_ShowString(TEXT_X, 140, "Zuletzt:", 16, BLACK);
  EPD_ShowString(TEXT_X, 164, zeile, 24, BLACK);

  char zaehlzeile[112];
  snprintf(zaehlzeile, sizeof(zaehlzeile),
           "EXIT %ld   hoch %ld   OK %ld   runter %ld   MENU %ld   Teilbilder %d",
           zaehler[T_EXIT], zaehler[T_HOCH], zaehler[T_OK], zaehler[T_RUNTER], zaehler[T_MENU],
           teilbilder);
  EPD_ShowString(TEXT_X, 208, zaehlzeile, 16, BLACK);

  EPD_ShowString(TEXT_X, 232,
                 refreshScharf ? "R wischt das Panel durch - andere Taste bricht ab, nach 5 s verfaellt es"
                               : "E einmal druecken macht daraus R - R loest den Vollrefresh aus",
                 16, BLACK);

}

// ---------------------------------------------------------------------------
// Panel
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Vollrefresh
// ---------------------------------------------------------------------------
//
// Loeschzyklus, dann das Bild neu aufbauen. Danach ist das Panel schattenfrei.
//
// Die zwei Zeilen des Neuaufbaus sind am Geraet ermittelt, nicht hergeleitet:
// RAM 0x26/0xA6 heisst im Datenblatt "Write RAM (RED)", der Elecrow-Treiber
// benutzt es als "vorheriges Bild" fuer den Teilrefresh, und was bei einem
// vollen Update gilt, steht nirgends. Fuenf Rezepte wurden deshalb nacheinander
// auf das Panel geschickt und angesehen:
//
//   Neuaufbau                       ohne EPD_Clear_R26A6H()   mit EPD_Clear_R26A6H()
//   EPD_Update()     (0xF7)         Text unvollstaendig       sauber, flackert
//   EPD_FastUpdate() (0xC7)         sauber, kein Flackern     Panel bleibt weiss
//
// Die beiden Zutaten muessen also ueber Kreuz zusammenpassen. Gewaehlt ist die
// ruhige Kombination — EPD_FastUpdate() ohne Clear_R26A6H — sie entspricht zugleich
// Elecrows eigenem Beispiel 5.79_Global_refresh. Das Flackern uebernimmt der
// Loeschzyklus davor, der Neuaufbau muss es nicht wiederholen.
//
// Nicht gefolgt ist daraus, dass EPD_Clear_R26A6H() falsch waere: Vor dem ERSTEN
// Teilrefresh ist er weiterhin noetig (siehe PROGRESS_BAR.md). Er gehoert nur
// nicht vor ein volles oder schnelles Update.

static void panelInit() {
  EPD_GPIOInit();
  EPD_FastMode1Init();     // enthaelt den Hardware-Reset
}

static void vollrefresh() {
  panelInit();
  EPD_Display_Clear();
  EPD_Update();            // Panel weiss wischen, das ist der flackernde Teil

  panelInit();
  EPD_Display(ImageBW);
  EPD_FastUpdate();        // ruhiger Neuaufbau, ohne Clear_R26A6H davor
}

// ---------------------------------------------------------------------------
// Tasten
// ---------------------------------------------------------------------------

// Entprellt: ein Zustand gilt erst, wenn er zweimal im Abstand von 15 ms
// gleich gelesen wurde.
static Taste gedrueckteTaste() {
  for (int i = 0; i < T_ANZAHL; i++) {
    if (digitalRead(tastePin[i]) == LOW) {
      delay(15);
      if (digitalRead(tastePin[i]) == LOW) return (Taste)i;
    }
  }
  return T_KEINE;
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n--- bedienleiste ---");

  pinMode(PIN_DISPLAY_POWER, OUTPUT);
  digitalWrite(PIN_DISPLAY_POWER, HIGH);   // ohne das bleibt das Panel dunkel

  for (int i = 0; i < T_ANZAHL; i++) pinMode(tastePin[i], INPUT);

  render(T_KEINE);
  vollrefresh();
}

void loop() {
  static Taste angezeigt = T_KEINE;

  const Taste jetzt = gedrueckteTaste();

  if (jetzt == angezeigt) {
    // Nichts Neues an den Tasten — hier laeuft nur die Verfallszeit des "R".
    // Sie zaehlt erst, wenn keine Taste mehr gedrueckt ist: Wer EXIT festhaelt,
    // ist noch am Bedienen, und ihm den Zustand unter der Hand wegzunehmen waere
    // das Gegenteil dessen, was die Frist bezwecken soll.
    if (refreshScharf && jetzt == T_KEINE && millis() - scharfSeit >= SCHARF_MS) {
      refreshScharf = false;
      Serial.println("R verfallen");
      teilbilder++;
      render(angezeigt);
      EPD_Display(ImageBW);
      EPD_PartUpdate();
    }
    delay(20);
    return;
  }

  bool wischen = false;

  if (jetzt != T_KEINE) {
    zaehler[jetzt]++;
    Serial.printf("%s (GPIO %d)\n", tasteName[jetzt], tastePin[jetzt]);

    if (jetzt == T_EXIT) {
      // Erster Druck macht scharf, zweiter wischt.
      wischen       = refreshScharf;
      refreshScharf = !refreshScharf;
      scharfSeit    = millis();
    } else {
      refreshScharf = false;      // jede andere Taste nimmt das "R" zurueck
    }
  }
  // Gewischt wird das RUHEBILD, nicht die gedrueckte Lasche. Sonst friert der
  // Vollrefresh die schwarze Lasche ein, und das Loslassen muesste sie per
  // Teilrefresh wieder wegnehmen — ein grosser Schwarz-nach-Weiss-Sprung, genau
  // das, was der Teilrefresh am schlechtesten kann. So ist nach dem Wischen
  // nichts mehr zu tun: `angezeigt` steht bereits auf T_KEINE, das Loslassen
  // loest kein weiteres Update aus.
  angezeigt = wischen ? T_KEINE : jetzt;

  // Zaehler vor dem Zeichnen setzen, er steht mit im Bild.
  if (wischen) teilbilder = 0; else teilbilder++;

  render(angezeigt);

  if (wischen) {
    Serial.println("Vollrefresh");
    vollrefresh();
  } else {
    EPD_Display(ImageBW);
    EPD_PartUpdate();     // kein Reset dazwischen
  }
}
