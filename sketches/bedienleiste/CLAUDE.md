# bedienleiste

Die drei Bedienelemente der linken Gehäusekante als Laschen auf dem Panel, daneben ein
Inhaltsbereich mit vier umschaltbaren Seiten. Oben **E** (EXIT), unten **M** (MENU),
dazwischen der Drehschalter. Wer eine Taste drückt, sieht seine Lasche schwarz werden.

Kein WLAN, keine `secrets.h`.

```bash
make flash SKETCH=sketches/bedienleiste
```

## Aufbau

Fünf Ebenen, jede mit einer Zuständigkeit:

| Datei | Inhalt | weiß nichts von |
|---|---|---|
| `zeichnen.h/.cpp` | `safePixel()`, `fillRect()`, `textWidth()` | allem übrigen |
| `tasten.h/.cpp` | GPIOs, Entprellung, `enum Taste` | dem Display |
| `laschen.h/.cpp` | die drei Laschen, **x = 0..45** | Tasten, Seiten, Puffer |
| `seiten.h/.cpp` | der Inhaltsbereich, **x ≥ 70** | den Laschen |
| `panel.h/.cpp` | bringt den Puffer aufs Panel | dem Bildinhalt |
| `bedienleiste.ino` | Navigation, Zähler, Refresh-Politik | — verbindet alles |

**Der Vertrag zwischen den beiden Zeichenebenen ist eine Spalte.** Die Laschen fassen nur
`x < 46` an, die Seiten nur `x >= 70`, und **keine von beiden ruft `Paint_Clear()`**.
`Paint_NewImage()` und `Paint_Clear()` kommen genau einmal in `setup()` vor; danach löscht
`seitenBereichLoeschen()` ausschließlich rechts. Nur deshalb lässt sich der Inhalt
wechseln, ohne die Laschen neu zu zeichnen — und die Leiste wäre in einem fremden Sketch
brauchbar, der den Rest des Bildes besitzt.

**Der Vertrag gilt auch auf sechs Pixel genau.** Die Menü-Markierung begann anfangs bei
`SEITEN_X0 - 6`, also links außerhalb des gelöschten Bereichs. Die sechs Pixel wurden nie
wieder weiß; jede Position, auf der die Markierung einmal stand, ließ dort einen schwarzen
Stummel zurück, und weil die Zeilen 22 px auseinanderliegen und der Balken 22 px hoch ist,
wuchsen die Stummel zu einem durchgehenden Balken über die ganze Liste zusammen. Bei einem
`Paint_Clear()` wäre das nie aufgefallen. **Wer partiell zeichnet, muss jede Grenze exakt
einhalten.**

Dass ausgerechnet **EXIT** den Vollrefresh scharf macht, ist eine Entscheidung dieser
Anwendung und steht deshalb in der `.ino`. In `../ha_wechsel` blättert EXIT um — ein
Widget, das die Taste für sich beansprucht, wäre dort im Weg.

## Bedienung

| Eingabe | im Menü | auf einer Seite |
|---|---|---|
| Rad hoch / runter | Auswahl bewegen | direkt blättern |
| Rad drücken (`OK`) | markierte Seite öffnen | zurück ins Menü |
| MENU | — | zurück ins Menü |
| EXIT | `E` → `R`, zweiter Druck wischt | dito |
| 5 s ohne Tastendruck | `R` verfällt (`SCHARF_MS`), Log: `R verfallen` | |

Die vier Seiten: **Menue** (Liste), **Tasten** (Diagnose mit Zählern), **Refresh** (was
beim Bedienen mit dem Panel passiert), **Muster** (große Flächen — der Sichttest, ob die
Laschen beim Seitenwechsel unberührt bleiben).

**Der Vollrefresh setzt auch den Zustand zurück:** Menü, Auswahl oben, Zähler auf null.
Danach steht alles wie nach dem Flashen. Ein sauberes Panel mit halb gelaufenen Zählern
wäre ein Zwischending, das beim Messen nur verwirrt. Der Anfangszustand steht in **einer**
Funktion, die `setup()` und der Vollrefresh gemeinsam aufrufen — zweimal hingeschrieben
liefe er beim nächsten neuen Feld auseinander.

**Das scharfe `R` ist ein Zustand, kein Tastendruck** — die Lasche bleibt invertiert, auch
wenn die Taste längst losgelassen ist. Nur den Buchstaben zu tauschen war zu leise: `E` und
`R` sind beide schmal, stehen an derselben Stelle und in derselben Größe. Die Verfallszeit
zählt erst, wenn keine Taste mehr gedrückt ist; wer EXIT festhält, ist noch am Bedienen.

## Der Drehschalter ist kein Encoder — gemessen, nicht vermutet

Der Schaltplan sagt Quadratur-Encoder (`TM_2024A`, zwei Phasen). Am Gerät verhält er sich
wie **zwei getrennte Taster**: Ruhezustand `A=1 B=1`, runter zieht nur B, hoch zieht nur A,
250–640 ms je Betätigung, die Phasen überlappen sich **nie**. Ein Dekoder hätte hier nichts
zu dekodieren — die Richtung steckt darin, *welche* Leitung zieht. Messwerte und
Messmethode stehen im Kopf von `tasten.h`.

Der Umweg dorthin ist die Lehre: Aus dem Log des normalen Betriebs schien hervorzugehen,
dass eine Betätigung beide Phasen auslöst. Dieses Log war aber nicht das Signal, sondern
die Ausgabe unseres Filters — 15 ms Entprellung, bei mehreren LOW-Pins gewinnt der
kleinste Index. Ein für **Taster** gebauter Filter, auf einen vermeintlichen Encoder
losgelassen. Erst der ungefilterte Mitschnitt mit `micros()` hat die Frage beantwortet.

## Refresh — was gilt und was offen ist

Zur Laufzeit gibt es **einen** Modus: Teilrefresh mit RAM-Fenster (`panelFenster()`).
`EPD_FastUpdate()` kommt nur im Vollrefresh vor, direkt hinter dem Hardware-Reset.

**Fast-Update mitten im Betrieb ist unbrauchbar.** `0xC7` lädt keine LUT nach und benutzt
die des Teilrefreshs. Am Gerät trieb das Panel dadurch schrittweise ins Schwarze: Beim
ersten Inhaltswechsel kam der neue schwarze Block nur grau, beim zweiten war fast alles
schwarz. GxEPD2 führt für diesen Fall ein Flag `_using_partial_mode` und initialisiert bei
jedem Moduswechsel neu — hier wird der Wechsel stattdessen ganz vermieden.

**Offen: Warum trägt nur das volle Fenster?** Am Gerät belegt:

| Fenster beim Teilrefresh | Ergebnis |
|---|---|
| ganzes Bild (x 0–791) | funktioniert |
| Laschenstreifen (x 0–45) | macht den vorherigen Inhaltswechsel rückgängig |

Zwei Erklärungsversuche sind widerlegt: ein veraltetes `0x26` (Nachführen half nicht) und
„ein zweiter Teilrefresh stört" (mit vollem Fenster stört er nicht). Solange die Erklärung
fehlt, ruft die `.ino` nur das volle Fenster auf — die 17-fache Ersparnis des kleinen
Fensters liegt also noch auf der Straße. Nächster Ansatz wäre zu klären, ob `0x44`/`0x45`
im **kaskadierten** Betrieb den Update-Bereich überhaupt begrenzen oder nur den
Schreibzugriff steuern.

Die Registerfolge selbst ist erarbeitet und dokumentiert (`panel.cpp`): `0x11 = 0x05`,
`0x44`/`0x45`, `0x4E`/`0x4F` für den Master; `0x91 = 0x04`, `0xC4`/`0xC5`, `0xCE`/`0xCF`
für den Slave, dessen X-Adressen **rückwärts** zählen (`slaveX = 99 - Pufferspalte`).
Pufferzeile `r` landet auf Panelzeile `271 - r`. Nichts davon steht im Datenblatt; alles
ist aus `EPD_Init.cpp` abgelesen und am Gerät bestätigt.

**Der zweite Hebel liegt woanders:** `spi.cpp` bit-bangt das SPI (drei `digitalWrite()` je
Bit, CS je Byte) — ein Vollbild sind rund 650.000 `digitalWrite`-Aufrufe, bevor das Panel
überhaupt anfängt. Der SSD1683 kann laut Datenblatt 20 MHz Hardware-SPI. Das wirkt auf
*jedes* Update, unabhängig von der Fenstergröße.

## Layout prüfen ohne Gerät

```bash
make sim SKETCH=sketches/bedienleiste
```

Der Simulator führt nur `setup()` aus, zeigt also das Menü im Ruhezustand. Für andere
Seiten `static Seite seite = S_MENUE;` vorübergehend ändern, für gedrückte Zustände den
Aufruf von `laschenZeichnen(...)`. Alle Zustände nebeneinanderlegen — so sind die Kerben in
den invertierten Pfeilen und die 9-px-Überlappung auf der Refresh-Seite aufgefallen, die
einzeln jeweils plausibel aussahen:

```bash
magick /tmp/z_*.png -crop 60x70+0+94 +repage -scale 600% +append /tmp/vergleich.png
```

Kontrast und Reaktionszeit entscheidet trotzdem das Panel, nicht das PNG.

## Weiterführend

- `../progress_bar/CLAUDE.md` — Partial-Refresh im Detail, `EPD_Clear_R26A6H()`
- `../../CLAUDE.md` → *Harte Fakten* — Zeichen-API, Refresh-Modi, Board
