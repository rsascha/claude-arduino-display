#include "panel.h"
#include "EPD.h"

static uint8_t* puffer = nullptr;

// Bytes je Pufferzeile. EPD_W ist 800, sichtbar sind 792 — dazwischen liegen
// die 8 ungenutzten Spalten zwischen den beiden Controllern.
static const int STRIDE = EPD_W / 8;          // 100
static const int MASTER_BYTES = 400 / 8;      // 50: Byte 0..49 gehoeren dem Master
static const int LETZTE_ZEILE = EPD_H - 1;    // 271
static const int SICHTBAR_W   = 792;          // sichtbare Breite, EPD_W ist 800

void panelStart(uint8_t* p) { puffer = p; }

// ---------------------------------------------------------------------------
// Panel aufwecken
// ---------------------------------------------------------------------------

static void panelInit() {
  EPD_GPIOInit();
  EPD_FastMode1Init();     // enthaelt den Hardware-Reset
}

// ---------------------------------------------------------------------------
// Das "vorherige Bild" nachtragen
// ---------------------------------------------------------------------------
//
// RAM 0x26/0xA6 haelt das Bild, gegen das der Teilrefresh seine Waveform je
// Pixel waehlt. Der Controller fuehrt es nach einem TEILREFRESH selbst nach,
// nach einem Fast-Update aber nicht — wer beides mischt, muss es selbst tun.
//
// Ohne das kam am Geraet der erste Tastendruck nach einem Vollrefresh nur grau
// heraus, egal welche Taste; ab dem zweiten stimmte es.
//
// EPD_Clear_R26A6H() taugt dafuer nicht: Es setzt 0x26 auf 0xFF, also "vorher
// alles weiss". Richtig ist das nur direkt nach dem Loeschzyklus. Gebraucht
// wird nicht "weiss", sondern "genau das, was gerade zu sehen ist".
//
// Die Adressrechnung ist die aus EPD_Display() (EPD_Init.cpp), nur mit
// 0x26/0xA6 statt 0x24/0xA4. Sie steht hier und nicht dort, damit die
// Vendor-Datei unveraendert bleibt.
static void merkeAltesBild() {
  uint32_t tempcol = 0, templine = 0;

  EPD_SetRAMMP();
  EPD_SetRAMMA();
  EPD_WR_REG(0x26);
  for (uint32_t i = 0; i < ALLSCREEN_BYTES; i++) {
    EPD_WR_DATA8(*(puffer + templine * Source_BYTES * 2 + tempcol));
    if (++templine >= Gate_BITS) { tempcol++; templine = 0; }
  }

  EPD_SetRAMSP();
  EPD_SetRAMSA();
  EPD_WR_REG(0xA6);
  for (uint32_t i = 0; i < ALLSCREEN_BYTES; i++) {
    EPD_WR_DATA8(*(puffer + templine * Source_BYTES * 2 + tempcol));
    if (++templine >= Gate_BITS) { tempcol++; templine = 0; }
  }
}

// ---------------------------------------------------------------------------
// Fenster
// ---------------------------------------------------------------------------
//
// Die Registerfolge ist die aus GxEPD2 (_setPartialRamArea), die KONVENTIONEN
// sind die von Elecrows Treiber, und beide muessen zusammenpassen. Nichts davon
// steht im Datenblatt; alles ist aus EPD_Init.cpp abgelesen.
//
// MASTER (EPD_SetRAMMP / EPD_SetRAMMA):
//   0x11 = 0x05   Y decrement, X increment — Y ist die schnelle Achse, deshalb
//                 wird spaltenweise geschrieben, nicht zeilenweise.
//   0x44          X-Bereich in BYTES, Start und Ende.
//   0x45          Y-Bereich, je 16 bit little endian. Weil Y rueckwaerts laeuft,
//                 ist Start der GROESSERE Wert.
//   0x4E / 0x4F   Adresszaehler auf den Anfang des Fensters.
//
// SLAVE: dieselben Register + 0x80, aber SPIEGELVERKEHRT. EPD_SetRAMSP setzt
// 0x91 = 0x04, also Y decrement UND X decrement, und 0xC4 laeuft von 49 nach 0.
// EPD_Display() schreibt die Slave-Haelfte in Pufferreihenfolge (Spalte 50..99)
// in dieses rueckwaerts laufende Fenster — Pufferspalte 50 landet damit auf
// Slave-X 49, Spalte 99 auf Slave-X 0. Also: slaveX = 99 - Pufferspalte.
//
// Wie Pufferzeile und Panelzeile zusammenhaengen, steht ebenfalls nirgends: Der
// Treiber setzt fuer das Vollbild Ystart = 271 und Yend = 0 und schreibt die
// Zeilen aufsteigend. Pufferzeile r landet also auf Panelzeile 271 - r, und
// genau das muss ein Fenster nachbilden.

// Sichtbares x -> Pufferspalte. Zwischen den beiden Controllern liegen 8
// ungenutzte Pixel; Paint_SetPixel() ueberspringt sie mit derselben Rechnung.
static int pufferX(int x) { return (x >= 396) ? x + 8 : x; }

static void fensterMaster(int c0, int c1, int y0, int y1) {
  const int yStart = LETZTE_ZEILE - y0;   // groesserer Wert, Y laeuft abwaerts
  const int yEnde  = LETZTE_ZEILE - y1;

  EPD_WR_REG(0x11);
  EPD_WR_DATA8(0x05);

  EPD_WR_REG(0x44);
  EPD_WR_DATA8(c0);
  EPD_WR_DATA8(c1);

  EPD_WR_REG(0x45);
  EPD_WR_DATA8(yStart & 0xFF);
  EPD_WR_DATA8((yStart >> 8) & 0xFF);
  EPD_WR_DATA8(yEnde & 0xFF);
  EPD_WR_DATA8((yEnde >> 8) & 0xFF);

  EPD_WR_REG(0x4E);
  EPD_WR_DATA8(c0);

  EPD_WR_REG(0x4F);
  EPD_WR_DATA8(yStart & 0xFF);
  EPD_WR_DATA8((yStart >> 8) & 0xFF);
}

static void fensterSlave(int c0, int c1, int y0, int y1) {
  const int xStart = 99 - c0;             // X laeuft beim Slave rueckwaerts
  const int xEnde  = 99 - c1;
  const int yStart = LETZTE_ZEILE - y0;
  const int yEnde  = LETZTE_ZEILE - y1;

  EPD_WR_REG(0x91);
  EPD_WR_DATA8(0x04);                     // Y decrement, X decrement

  EPD_WR_REG(0xC4);
  EPD_WR_DATA8(xStart);
  EPD_WR_DATA8(xEnde);

  EPD_WR_REG(0xC5);
  EPD_WR_DATA8(yStart & 0xFF);
  EPD_WR_DATA8((yStart >> 8) & 0xFF);
  EPD_WR_DATA8(yEnde & 0xFF);
  EPD_WR_DATA8((yEnde >> 8) & 0xFF);

  EPD_WR_REG(0xCE);
  EPD_WR_DATA8(xStart);

  EPD_WR_REG(0xCF);
  EPD_WR_DATA8(yStart & 0xFF);
  EPD_WR_DATA8((yStart >> 8) & 0xFF);
}

// Spaltenweise, weil Y die schnelle Achse ist — dieselbe Reihenfolge wie in
// EPD_Display().
static void spaltenSchreiben(int c0, int c1, int y0, int y1) {
  for (int c = c0; c <= c1; c++)
    for (int r = y0; r <= y1; r++)
      EPD_WR_DATA8(puffer[r * STRIDE + c]);
}

// Schreibt das Fenster in die RAM-Ebene `neu` (0x24/0xA4) oder `alt`
// (0x26/0xA6). Die Fensterregister muessen dabei jedes Mal neu gesetzt werden —
// der Adresszaehler steht nach dem Schreiben am Ende des Bereichs.
static void fensterSchreiben(int c0, int c1, int y0, int y1, bool nachAlt) {
  if (c0 < MASTER_BYTES) {                       // linke Haelfte betroffen
    const int cm1 = (c1 < MASTER_BYTES) ? c1 : MASTER_BYTES - 1;
    fensterMaster(c0, cm1, y0, y1);
    EPD_WR_REG(nachAlt ? 0x26 : 0x24);
    spaltenSchreiben(c0, cm1, y0, y1);
  }

  if (c1 >= MASTER_BYTES) {                      // rechte Haelfte betroffen
    const int cs0 = (c0 > MASTER_BYTES) ? c0 : MASTER_BYTES;
    fensterSlave(cs0, c1, y0, y1);
    EPD_WR_REG(nachAlt ? 0xA6 : 0xA4);
    spaltenSchreiben(cs0, c1, y0, y1);
  }
}

void panelFenster(int x0, int y0, int x1, int y1) {
  if (!puffer) return;

  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > SICHTBAR_W - 1) x1 = SICHTBAR_W - 1;
  if (y1 > LETZTE_ZEILE)   y1 = LETZTE_ZEILE;
  if (x1 < x0 || y1 < y0) return;

  // Auf Byte-Grenzen erweitern — feiner kann der Controller nicht adressieren.
  const int c0 = pufferX(x0) / 8;
  const int c1 = pufferX(x1) / 8;

  fensterSchreiben(c0, c1, y0, y1, false);       // neues Bild nach 0x24/0xA4
  EPD_PartUpdate();                              // rechnet gegen 0x26/0xA6

  // 0x26/0xA6 wird NICHT nachgefuehrt. Der Versuch stand hier eine Weile, weil
  // er die naheliegende Erklaerung fuer den Ruecksprung war — er hat nichts
  // geaendert und die Datenmenge je Update verdoppelt. Aufgeraeumt statt
  // aufgehoben: Ein wirkungsloser Aufruf, den niemand mehr erklaeren kann, ist
  // schlimmer als sein Fehlen.
  //
  // OFFEN bleibt damit, warum ein KLEINES Fenster den vorherigen Inhaltswechsel
  // rueckgaengig macht, ein Fenster ueber das ganze Bild aber nicht. Beides ist
  // am Geraet belegt; eine Erklaerung dafuer gibt es noch nicht. Solange sie
  // fehlt, ruft bedienleiste.ino nur das volle Fenster auf.
}

// ---------------------------------------------------------------------------
// Ganze Bilder
// ---------------------------------------------------------------------------

void panelSchnell() {
  if (!puffer) return;
  EPD_Display(puffer);     // setzt die Fenster selbst wieder auf Vollbild
  EPD_FastUpdate();
  merkeAltesBild();        // sonst kaeme der naechste Teilrefresh grau
}

void panelVollrefresh() {
  if (!puffer) return;
  panelInit();
  EPD_Display_Clear();
  EPD_Update();            // Panel weiss wischen, das ist der flackernde Teil

  panelInit();
  panelSchnell();
}
