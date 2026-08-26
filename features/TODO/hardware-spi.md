# Hardware-SPI statt Bit-Banging

**Status:** offen, noch nichts umgesetzt.
**Wirkung:** betrifft *jedes* Update aller Sketches, unabhängig von Fenstergröße und
Refresh-Modus.
**Gefunden bei:** `sketches/bedienleiste`, als die Bedienung träge wirkte.

## Das Problem

`spi.cpp` (Elecrow-Vendor-Datei) überträgt jedes Byte in Software:

```c
void EPD_WR_Bus(uint8_t dat) {
    EPD_CS_Clr();
    for (i = 0; i < 8; i++) {
        EPD_SCK_Clr();
        if (dat & 0x80) EPD_MOSI_Set(); else EPD_MOSI_Clr();
        EPD_SCK_Set();
        dat <<= 1;
    }
    EPD_CS_Set();
}
```

Pro Bit **drei** `digitalWrite()`, dazu CS **je Byte**. Ein Vollbild sind 27.200 Byte:

| | |
|---|---|
| Bits | 27.200 × 8 = 217.600 |
| `digitalWrite()` für Takt und Daten | ~652.800 |
| dazu CS-Wechsel | ~54.400 |

Bei grob 0,5–1 µs je `digitalWrite()` auf dem ESP32-S3 sind das **0,3 bis 0,7 Sekunden je
Vollbild** — bevor der Controller überhaupt anfängt, die Pixel zu treiben.

**Achtung: geschätzt, nicht gemessen.** Der erste Schritt der Umsetzung ist deshalb, es zu
messen (siehe unten). Erst dann steht fest, wie groß der Anteil an der gefühlten Trägheit
wirklich ist.

## Was möglich wäre

Der SSD1683 kann **20 MHz** Schreibtakt (Datenblatt S. 5 §2, AC-Kennwerte S. 45 §12.1).
27.200 Byte wären damit rund **11 ms** statt einiger hundert. Der ESP32-S3 hat dafür
Hardware-SPI; die Leitungen liegen passend:

| Signal | GPIO |
|---|---|
| SCK | 12 |
| MOSI | 11 |
| CS | 45 |
| DC | 46 |
| RES | 47 |
| BUSY | 48 |

## Wie das aussehen könnte

Die Vendor-Dateien sollen **unverändert** bleiben (`sketches/CLAUDE.md`). `spi.cpp` also
nicht editieren, sondern **ersetzen**: eine eigene `spi_hw.cpp` im Sketch-Ordner, die
dieselben vier Funktionen aus `spi.h` bereitstellt — `EPD_GPIOInit()`, `EPD_WR_Bus()`,
`EPD_WR_REG()`, `EPD_WR_DATA8()` — und `spi.cpp` aus dem Ordner entfernen. Der übrige
Treiber merkt davon nichts, er kennt nur diese Schnittstelle.

Zwei Dinge lohnen dabei besondere Aufmerksamkeit:

- **CS je Byte oder je Transaktion?** Der Vendor-Code zieht CS für *jedes* Byte. GxEPD2
  hält CS über eine ganze Transaktion unten, was deutlich schneller ist. Ob der SSD1683 das
  hier mitmacht, ist zu prüfen — im Zweifel erst 1:1 nachbauen (CS je Byte), messen, dann
  zusammenfassen und erneut messen.
- **Blockweise schreiben.** Der eigentliche Gewinn kommt weniger aus dem höheren Takt als
  daraus, ganze Puffer am Stück zu übergeben (`SPI.writeBytes()`), statt 27.200 Einzelaufrufe
  zu machen. Dafür bräuchte es neben `EPD_WR_DATA8()` eine Blockvariante — die ruft
  allerdings niemand auf, solange `EPD_Display()` byteweise arbeitet. Entweder man
  akzeptiert das, oder die eigenen Schreibpfade (`panel.cpp`) nutzen die Blockvariante
  direkt.

## Wie sich der Gewinn belegen lässt

Vor jeder Änderung messen, sonst weiß hinterher niemand, was es gebracht hat:

```c
const unsigned long t0 = micros();
EPD_Display(ImageBW);
Serial.printf("EPD_Display: %lu us\n", micros() - t0);
```

Dasselbe um `EPD_PartUpdate()` legen. Damit trennt sich, was auf die **Übertragung** und
was auf die **Waveform** des Panels entfällt.

## Die ehrliche Grenze

Der Refresh selbst dauert ein paar hundert Millisekunden, und daran ändert schnelleres SPI
nichts — das ist Physik der Elektrophorese, nicht Software. Wenn die Übertragung heute
0,5 s kostet und der Refresh 0,4 s, wird aus 0,9 s etwa 0,4 s. Spürbar, aber kein
OLED-Verhalten. Wer ein flüssiges Menü will, ist auf E-Paper grundsätzlich falsch — das
sagt auch der GxEPD2-Autorenkreis so
([Diskussion #133](https://github.com/ZinggJM/GxEPD2/discussions/133)).

## Verwandt

- `sketches/bedienleiste/CLAUDE.md` → *Refresh* — der andere offene Hebel: warum ein
  kleines RAM-Fenster den vorherigen Inhaltswechsel rückgängig macht
- `material/CLAUDE.md` → §2 — SSD1683, Kommandos und Puffergeometrie
- `../../CLAUDE.md` → *Harte Fakten*
