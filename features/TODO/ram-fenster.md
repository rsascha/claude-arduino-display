# RAM-Fenster: warum trägt nur das volle?

**Status:** offen. Die Registerfolge steht und funktioniert, die *Wirkung* eines kleinen
Fensters ist ungeklärt.
**Gewinn, wenn geklärt:** Faktor 17 bei jedem Tastendruck — 1.632 statt 27.200 Byte.
**Code:** `sketches/bedienleiste/panel.cpp`, Funktion `panelFenster()`.

## Was belegt ist

Ein Teilrefresh muss nicht das ganze RAM beschreiben. Der Adressbereich lässt sich
einengen, und die Registerfolge dafür ist erarbeitet und am Gerät bestätigt:

| | Master | Slave |
|---|---|---|
| Entry Mode | `0x11` = `0x05` (Y−, X+) | `0x91` = `0x04` (Y−, X−) |
| X-Bereich, in Bytes | `0x44` | `0xC4` |
| Y-Bereich, 16 bit LE | `0x45` | `0xC5` |
| Adresszähler | `0x4E` / `0x4F` | `0xCE` / `0xCF` |
| RAM schreiben | `0x24` | `0xA4` |

Dazu zwei Zuordnungen, die in keinem Datenblatt stehen und aus `EPD_Init.cpp` abgelesen
sind: Der Slave zählt X **rückwärts** (`slaveX = 99 - Pufferspalte`), und Pufferzeile `r`
landet auf Panelzeile `271 - r`.

**Am Gerät gemessen:**

| Fenster beim Teilrefresh | Ergebnis |
|---|---|
| ganzes Bild (x 0–791) | funktioniert einwandfrei |
| Laschenstreifen (x 0–45) | schreibt korrekt **und** macht den vorherigen Inhaltswechsel rückgängig |

Das Schreiben selbst stimmt also: Der Streifen erscheint richtig, an der richtigen Stelle,
und **spürbar schneller** — das war der ganze Zweck. Kaputt ist nur, dass dabei der Rest
des Bildes auf einen älteren Stand zurückfällt.

## Was widerlegt ist

- **„`0x26` ist veraltet."** Der Teilrefresh rechnet jedes Pixel aus dem Übergang *alt →
  neu*, und „alt" steht in `0x26`/`0xA6`. Naheliegend war: Außerhalb des kleinen Fensters
  steht dort noch das Bild vom letzten Vollrefresh. Gegenprobe: nach jedem Update dasselbe
  Fenster **auch** nach `0x26` schreiben. Änderte nichts, verdoppelte nur die Datenmenge —
  wieder ausgebaut.
- **„Ein zweiter Teilrefresh kurz nach dem ersten stört."** Gegenprobe: das zweite Update
  mit **vollem** Fenster fahren, sonst alles gleich. Funktioniert einwandfrei. Es liegt
  also an der Fenster*größe*, nicht am zweiten Update.

Isoliert wurde der Fehler so: Beim Loslassen der Taste **gar kein** Update auslösen.
Dann bleibt der Inhalt stehen. Damit war klar, dass das Streifen-Update der Auslöser ist
und nicht der Inhaltswechsel davor.

## Was als Nächstes zu prüfen wäre

1. **Begrenzen `0x44`/`0x45` überhaupt den Update-Bereich?** Möglich ist, dass sie nur den
   RAM-*Schreibzugriff* steuern und der Refresh immer alle Gates fährt. Dann würde ein
   kleines Fenster bedeuten: neues Bild nur im Streifen, alter Inhalt außerhalb — und der
   Controller träfe außerhalb auf eine Kombination, die er als Änderung deutet.
   Datenblatt `material/ssd1683-datasheet.pdf`, S. 34–35.
2. **Gilt ein Fenster im kaskadierten Betrieb?** Beim Streifen bekommt nur der **Master**
   ein Fenster gesetzt, der Slave behält seines vom letzten Vollbild. Was `0x20` (Master
   Activation) dann am Slave auslöst, ist unklar. Gegenprobe: dem Slave ein
   entartetes Fenster geben (etwa 1 Byte) und sehen, ob sich das Verhalten ändert.
3. **Wie macht es GxEPD2?** `~/development/tmp/GxEPD2` ist geklont;
   `src/epd/GxEPD2_154_D67.cpp` enthält `_setPartialRamArea()` und `_Update_Part()`.
   Interessant ist vor allem, ob dort zwischen Voll- und Teilmodus umgeschaltet wird
   (`_Init_Part()` / `_using_partial_mode`) und was das genau tut.
4. **Zwischenschritt messen.** Statt der zwei Extreme (46 px und 792 px) ein mittleres
   Fenster probieren — etwa nur die Zeilen des Menüs. Wenn es ab einer bestimmten Größe
   kippt, ist das ein Hinweis auf einen Adress- oder Rundungsfehler; kippt es sofort, auf
   etwas Grundsätzliches.

## Warum es sich lohnt

Der Tastendruck ist die einzige Interaktion, bei der es auf Reaktionszeit ankommt — und
genau dort ändert sich am wenigsten Bild. Mit Fenster war das Drücken am Gerät **deutlich**
schneller; das ist keine Rechnung, das war zu sehen. Der Rest des Bildes muss dafür nicht
angefasst werden.

Der zweite Hebel für dasselbe Ziel ist unabhängig davon und in
[`hardware-spi.md`](hardware-spi.md) beschrieben. Beide zusammen wären deutlich mehr als
jeder für sich: weniger Bytes **und** schneller übertragen.

## Verwandt

- `sketches/bedienleiste/CLAUDE.md` → *Refresh* — dieselbe Sache aus Sicht des Sketches
- `sketches/progress_bar/CLAUDE.md` — `0x26`/`0xA6` und der erste Teilrefresh
- `../../CLAUDE.md` → *Harte Fakten*
