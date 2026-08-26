// bedienleiste — die drei Bedienelemente an der linken Gehaeusekante als
// Laschen auf dem Panel, direkt auf ihrer Hoehe.
//
// Oben EXIT ("E"), unten MENU ("M"), dazwischen der Drehschalter. Wer eine
// Taste drueckt, sieht seine Lasche schwarz werden — damit ist am Geraet ohne
// Serial-Monitor pruefbar, welches Element wo sitzt und welchen GPIO es zieht.
//
// Aufgeteilt auf drei Ebenen, damit die Leiste in anderen Sketches verwendbar
// bleibt:
//
//   tasten.cpp    GPIOs und Entprellung — weiss nichts vom Display
//   laschen.cpp   zeichnet die Laschen in den aktiven Puffer — weiss nichts
//                 von Tasten und legt selbst keinen Puffer an
//   diese Datei   was beides bedeutet: Beschriftung, Zaehler, Refresh-Politik
//
// Die Trennung ist keine Formsache. Wuerde die Zeichenebene wie frueher selbst
// Paint_NewImage() und Paint_Clear() aufrufen, koennte man die Leiste nicht
// ueber ein bestehendes Bild legen — sie wuerde es loeschen.
//
// Kein WLAN, keine secrets.h.

#include <Arduino.h>
#include "EPD.h"
#include "tasten.h"
#include "laschen.h"

uint8_t ImageBW[27200];

const int      PIN_DISPLAY_POWER = 7;
const uint16_t DISPLAY_ROTATION  = 0;

// Textspalte rechts der Laschen.
const int TEXT_X = LASCHEN_BREITE + 74;

// So lange bleibt das "R" scharf, danach faellt es von selbst auf "E" zurueck.
const unsigned long SCHARF_MS = 5000;

// ---------------------------------------------------------------------------
// Zustand der Anzeige
// ---------------------------------------------------------------------------

static long zaehler[T_ANZAHL] = { 0, 0, 0, 0, 0 };

// Teilbilder seit dem letzten Vollrefresh — steht mit auf dem Display, damit
// ablesbar ist, wie viel Schatten sich seither aufbauen konnte.
static int teilbilder = 0;

// Der Vollrefresh laeuft nicht nach Zaehler, sondern auf Zuruf, und er braucht
// zwei Druecker: Der erste auf EXIT macht aus dem "E" ein "R", erst der zweite
// wischt. Ein Vollrefresh laesst das Panel mehrere Sekunden weiss stehen — nach
// einem Zaehler getaktet traefe das einen mitten im Bedienen, und wer nur
// antippen wollte, ob die Lasche reagiert, soll dabei nicht das halbe Display
// ausknipsen. Jede andere Taste nimmt das "R" zurueck, und nach SCHARF_MS
// verfaellt es von selbst: Ein Bedienelement, das dauerhaft in einem
// Sonderzustand steht, den man vergessen hat, loest beim naechsten
// beilaeufigen Druck etwas aus, das man nicht wollte.
//
// Dass ausgerechnet EXIT diese Rolle hat, ist eine Entscheidung DIESER
// Anwendung und steht deshalb hier und nicht in laschen.cpp. In ha_wechsel
// blaettert EXIT um.
static bool          refreshScharf = false;
static unsigned long scharfSeit    = 0;

// ---------------------------------------------------------------------------
// Bild
// ---------------------------------------------------------------------------

static void render(Taste aktiv) {
  Paint_NewImage(ImageBW, EPD_W, EPD_H, DISPLAY_ROTATION, WHITE);
  Paint_Clear(WHITE);

  // Das scharfe "R" wird invertiert dargestellt, auch wenn die Taste laengst
  // losgelassen ist: Es ist ein ZUSTAND, kein Tastendruck. Nur den Buchstaben zu
  // tauschen war zu leise — E und R sind beide schmal und stehen an derselben
  // Stelle, der Wechsel ging im Blick auf das ganze Panel unter. Eine schwarze
  // Lasche sieht man aus dem Augenwinkel.
  LaschenZustand z;
  z.aktiv      = aktiv;
  z.exitText   = refreshScharf ? "R" : "E";
  z.exitInvers = refreshScharf;
  zeichneLaschen(z);

  EPD_ShowString(TEXT_X,  24, "Bedienleiste", 24, BLACK);
  EPD_ShowString(TEXT_X,  60, "Die Laschen liegen auf der Hoehe der echten Taster:", 16, BLACK);
  EPD_ShowString(TEXT_X,  80, "E = EXIT (GPIO 1), Rad = GPIO 4/5/6, M = MENU (GPIO 2).", 16, BLACK);
  EPD_ShowString(TEXT_X, 100, "Druecken faerbt die Lasche schwarz - Partial-Refresh.", 16, BLACK);

  EPD_ShowString(TEXT_X, 140, "Zuletzt:", 16, BLACK);
  EPD_ShowString(TEXT_X, 164, aktiv == T_KEINE ? "bereit" : tasteName(aktiv), 24, BLACK);

  char zaehlzeile[112];
  snprintf(zaehlzeile, sizeof(zaehlzeile),
           "EXIT %ld   hoch %ld   OK %ld   runter %ld   MENU %ld   Teilbilder %d",
           zaehler[T_EXIT], zaehler[T_HOCH], zaehler[T_OK], zaehler[T_RUNTER], zaehler[T_MENU],
           teilbilder);
  EPD_ShowString(TEXT_X, 208, zaehlzeile, 16, BLACK);

  char fuss[96];
  if (refreshScharf)
    snprintf(fuss, sizeof(fuss),
             "R wischt das Panel durch - andere Taste bricht ab, nach %lu s verfaellt es",
             SCHARF_MS / 1000);
  else
    snprintf(fuss, sizeof(fuss),
             "E einmal druecken macht daraus R - R loest den Vollrefresh aus");
  EPD_ShowString(TEXT_X, 232, fuss, 16, BLACK);
}

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
// ruhige Kombination — EPD_FastUpdate() ohne Clear_R26A6H — sie entspricht
// zugleich Elecrows eigenem Beispiel 5.79_Global_refresh. Das Flackern
// uebernimmt der Loeschzyklus davor, der Neuaufbau muss es nicht wiederholen.
//
// Nicht gefolgt ist daraus, dass EPD_Clear_R26A6H() falsch waere: Vor dem ERSTEN
// Teilrefresh ist er weiterhin noetig (siehe ../progress_bar/CLAUDE.md). Er
// gehoert nur nicht vor ein volles oder schnelles Update.

static void panelInit() {
  EPD_GPIOInit();
  EPD_FastMode1Init();     // enthaelt den Hardware-Reset
}

// Schreibt den Puffer in RAM 0x26/0xA6 — das "vorherige Bild", aus dem der
// Controller beim Teilrefresh seine Waveform je Pixel waehlt.
//
// Ohne das kam am Geraet der ERSTE Tastendruck nach einem Vollrefresh nur grau
// heraus, egal welche Taste; ab dem zweiten stimmte es. Der Grund: Nach dem
// Wischen steht auf dem Panel das Ruhebild, in 0x26 aber noch, was
// EPD_Display_Clear() dort hinterlassen hat. Der erste Teilrefresh rechnet dann
// gegen einen Ausgangszustand, den es nicht gibt, und treibt die Pixel zu
// schwach. Ab dem zweiten fuehrt der Controller 0x26 selbst nach — deshalb
// sieht der Fehler nach einem Kontrastproblem des Panels aus und nicht nach
// einem Zustandsfehler.
//
// EPD_Clear_R26A6H() waere das Naheliegende, taugt hier aber nicht: Es setzt
// 0x26 auf 0xFF, also "vorher alles weiss" — richtig direkt nach dem
// Loeschzyklus, falsch, sobald das Ruhebild schon steht. Gebraucht wird nicht
// "weiss", sondern "genau das, was gerade zu sehen ist".
//
// Die Adressrechnung ist die aus EPD_Display() (EPD_Init.cpp), nur mit
// 0x26/0xA6 statt 0x24/0xA4. Sie steht hier und nicht dort, damit die
// Vendor-Datei unveraendert bleibt.
static void merkeAltesBild(const uint8_t* img) {
  uint32_t tempcol = 0, templine = 0;

  EPD_SetRAMMP();
  EPD_SetRAMMA();
  EPD_WR_REG(0x26);
  for (uint32_t i = 0; i < ALLSCREEN_BYTES; i++) {
    EPD_WR_DATA8(*(img + templine * Source_BYTES * 2 + tempcol));
    if (++templine >= Gate_BITS) { tempcol++; templine = 0; }
  }

  EPD_SetRAMSP();
  EPD_SetRAMSA();
  EPD_WR_REG(0xA6);
  for (uint32_t i = 0; i < ALLSCREEN_BYTES; i++) {
    EPD_WR_DATA8(*(img + templine * Source_BYTES * 2 + tempcol));
    if (++templine >= Gate_BITS) { tempcol++; templine = 0; }
  }
}

static void vollrefresh() {
  panelInit();
  EPD_Display_Clear();
  EPD_Update();            // Panel weiss wischen, das ist der flackernde Teil

  panelInit();
  EPD_Display(ImageBW);
  EPD_FastUpdate();        // ruhiger Neuaufbau, ohne Clear_R26A6H davor

  // Der Controller weiss jetzt nicht, was er gerade angezeigt hat. Nachtragen,
  // sonst kommt der naechste Teilrefresh grau.
  merkeAltesBild(ImageBW);
}

// Ein Teilbild ans Panel schicken. Zwischen Teilrefreshs darf NICHT neu
// initialisiert werden: EPD_FastMode1Init() enthaelt einen Hardware-Reset, und
// ein zurueckgesetzter Controller kennt das vorherige Bild nicht mehr — worauf
// Partial gerade aufbaut.
//
// Zaehlt bewusst NICHT selbst mit: `teilbilder` steht im Bild und muss deshalb
// schon vor render() stimmen.
static void teilbild() {
  EPD_Display(ImageBW);
  EPD_PartUpdate();
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n--- bedienleiste ---");

  pinMode(PIN_DISPLAY_POWER, OUTPUT);
  digitalWrite(PIN_DISPLAY_POWER, HIGH);   // ohne das bleibt das Panel dunkel

  tastenInit();

  render(T_KEINE);
  vollrefresh();
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
      teilbilder++;
      render(angezeigt);
      teilbild();
    }
    return;
  }

  bool wischen = false;

  if (jetzt != T_KEINE) {
    zaehler[jetzt]++;
    Serial.printf("%s (GPIO %d)\n", tasteName(jetzt), tastePin(jetzt));

    if (jetzt == T_EXIT) {
      // Erster Druck macht scharf, zweiter wischt.
      wischen       = refreshScharf;
      refreshScharf = !refreshScharf;
      if (refreshScharf) scharfSeit = millis();   // nur beim Scharfmachen
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

  if (wischen) {
    teilbilder = 0;              // vor render(), der Zaehler steht mit im Bild
    render(angezeigt);
    Serial.println("Vollrefresh");
    vollrefresh();
  } else {
    teilbilder++;                // dito: erst zaehlen, dann zeichnen
    render(angezeigt);
    teilbild();
  }
}
