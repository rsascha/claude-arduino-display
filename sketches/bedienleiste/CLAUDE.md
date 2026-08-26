# bedienleiste

Zeichnet die drei Bedienelemente der linken Gehäusekante als Laschen auf das Panel:
oben **E** (EXIT), unten **M** (MENU), dazwischen der Drehschalter mit Pfeil hoch,
`OK`, Pfeil runter. Drücken färbt die Lasche schwarz.

Kein WLAN, keine `secrets.h`. Bauen und flashen:

```bash
make flash SKETCH=sketches/bedienleiste
```

## Bedienung

| Eingabe | Wirkung |
|---|---|
| EXIT (GPIO 1) | Lasche wird **schwarz und bleibt es**, der Buchstabe wechselt `E` → `R` |
| **R** drücken | Vollrefresh: Panel wird gewischt, danach steht wieder das `E` |
| Rad hoch / drücken / runter (GPIO 4/5/6) | die betroffene Zone der Radlasche wird schwarz |
| MENU (GPIO 2) | Lasche schwarz |
| jede Taste außer EXIT | bricht ein scharfes `R` ab |
| 5 s ohne Tastendruck | `R` verfällt von selbst (`SCHARF_MS`), Log: `R verfallen` |

Der Vollrefresh braucht zwei Drücker, weil er das Panel mehrere Sekunden weiß stehen
lässt. Wer nur antippen will, ob die Lasche reagiert, soll dabei nicht das halbe
Display ausknipsen.

**Das scharfe `R` ist ein Zustand, kein Tastendruck** — die Lasche bleibt deshalb
invertiert, auch wenn die Taste längst losgelassen ist. Nur den Buchstaben zu tauschen war
zu leise: `E` und `R` sind beide schmal, stehen an derselben Stelle und in derselben Größe,
und die Lasche wurde beim Loslassen wieder weiß. Der Wechsel ging im Blick auf das ganze
Panel unter. Eine schwarze Lasche sieht man aus dem Augenwinkel.

**Die Verfallszeit zählt erst, wenn keine Taste mehr gedrückt ist.** Wer EXIT festhält, ist
noch am Bedienen; ihm den Zustand unter der Hand wegzunehmen wäre das Gegenteil dessen, was
die Frist bezwecken soll. Der Sinn der Frist: Ein Bedienelement, das dauerhaft in einem
Sonderzustand steht, den man vergessen hat, löst beim nächsten beiläufigen Druck etwas aus,
das man nicht wollte. Preis ist ein zusätzlicher Teilrefresh je scharf gemachtem, aber nicht
genutztem `R` — bei einem Testsketch ohne Belang, in einem Dauerbetrieb eine Stelle zum
Nachdenken.

## Was man hier nicht verstellen sollte, ohne es zu wissen

**Die y-Positionen sind gemessen, nicht gerechnet.** Die Bedienelemente liegen bei
60–70, 110–150 und 200–210 — am Gerät abgelesen. Im Code stehen davon die Mitten
(`EXIT_MITTE = 65`, `MENU_MITTE = 205`) und für das Rad die gezeichneten Kanten
(`RAD_Y0/RAD_Y1 = 104/156`, siehe Tabelle unten). Wer sie ändert,
verschiebt die Laschen gegen die echten Taster. Nachmessen geht mit einem Sketch, der
an beiden langen Kanten ein Maßband von 0 bis 271 zeichnet (Striche alle 10 px,
Beschriftung alle 50 px); der Wegwerf-Sketch dafür ist nach dem Messen gelöscht worden.

**Keine Lasche hat die Höhe ihres Elements — sie sitzt auf dessen Mitte.** Die Mitte ist
die Information, auf die es beim Zuordnen ankommt; die Höhe richtet sich danach, was
hineinpassen muss.

| Lasche | Element | gezeichnet | warum |
|---|---|---|---|
| `E`, `M` | 11 px | 32 px (`KLEIN_H`) | ein Buchstabe in Größe 24 braucht 24 px |
| Rad | 41 px (110–150) | 53 px (`RAD_Y0/Y1` = 104/156) | Pfeil + `OK` + Pfeil plus Abstände |

**Der Drehschalter bekommt eine Lasche, nicht drei.** Er ist *ein* Bedienelement mit
drei Kontakten (Quadratur-Encoder plus Tastkontakt). Drei getrennte Laschen würden drei
Bedienelemente vortäuschen. Deshalb drei Zonen in einer Form, und invertiert wird nur
die betroffene Zone.

**Die 4 px Lücken zwischen Pfeil, `OK` und Pfeil sind keine Kosmetik.** Vorher lagen die
drei Inhalte ohne Abstand aneinander — zwischen `OK` und dem unteren Pfeil sogar mit 0 px.
Passiv fiel das nicht auf, weil alles dieselbe Farbe hat. **Invertiert schon:** Die
Zonengrenze lag genau auf der Inhaltskante, und der weiße Pfeil stieß mit seiner
breitesten Zeile — der Basis — an die weiße Fläche daneben. Er las sich damit als **Kerbe**
im schwarzen Balken statt als Pfeil; in der `OK`-Zone wirkten umgekehrt beide schwarzen
Pfeile abgeschnitten.

Deshalb gilt jetzt: 4 px zwischen den Inhalten, und jede Zone reicht 2 px über ihren
Inhalt hinaus, damit der invertierte Inhalt ringsum Rand behält.

```
Pfeil oben  110..117      Zone hoch    106..119
   Lücke    118..121
OK          122..137      Zone OK      120..139
   Lücke    138..141
Pfeil unten 142..149      Zone runter  140..154
```

Die Inhaltspositionen hängen **nicht** an `RAD_Y0`/`RAD_Y1`. Wer die Lasche höher oder
flacher macht, verschiebt damit nur den Rand. Vorher war der obere Pfeil als `RAD_Y0 + 4`
und der untere als `RAD_Y1 - 11` gerechnet — jede Änderung der Höhe hätte den Inhalt
mitgezogen und den unteren Pfeil aus der Mitte geschoben.

**Solche Fehler sieht man nur im Vergleich.** Alle vier Zustände nebeneinander zu rendern
hat sie sichtbar gemacht; im Vollbild und einzeln fiel keiner auf. Wie das geht, steht
unten unter *Layout prüfen ohne Gerät*.

**`pinMode(pin, INPUT)` ist richtig.** Alle fünf Tasten haben 4,7-kΩ-Pull-ups auf der
Platine (`material/SCHALTPLAN.md` → *Tasten*). `INPUT_PULLUP` bringt nichts.

## Refresh — die teuerste Erkenntnis dieses Sketches

Regelfall ist `EPD_PartUpdate()`: Pro Tastendruck ändert sich eine Lasche, genau der
Fall, für den Partial gedacht ist. Zwischen den Teilrefreshs darf **nicht** neu
initialisiert werden — `EPD_FastMode1Init()` enthält einen Hardware-Reset, und ein
zurückgesetzter Controller kennt das vorherige Bild nicht mehr.

Der Vollrefresh (`vollrefresh()`) ist Löschzyklus + Neuaufbau. Der Neuaufbau ist am
Gerät ermittelt, nicht hergeleitet; jeweils nach `EPD_Display_Clear()` + `EPD_Update()`:

| Neuaufbau | ohne `EPD_Clear_R26A6H()` | mit `EPD_Clear_R26A6H()` |
|---|---|---|
| `EPD_Update()` (`0xF7`) | Text unvollständig | sauber, flackert |
| `EPD_FastUpdate()` (`0xC7`) | **sauber, kein Flackern** ← gewählt | Panel bleibt weiß |

Zweimal hintereinander `EPD_Display()` + `EPD_Update()` lässt das Panel ebenfalls weiß.

Drei dieser Fehlbilder sehen aus wie ein Hardware- oder Kontrastproblem und sind ein
Register-Missverständnis: `0x26`/`0xA6` heißt im Datenblatt „Write RAM (RED)", der
Elecrow-Treiber benutzt es als *vorheriges Bild* für den Teilrefresh. Welche Bedeutung
bei welchem Update-Modus gilt, steht in keiner der beiden Quellen.

**Gewischt wird das Ruhebild, nicht die gedrückte Lasche.** Sonst friert der
Vollrefresh den schwarzen Block ein, und das Loslassen müsste ihn per Teilrefresh
wieder wegnehmen — ein großer Schwarz-nach-Weiß-Sprung, das Schlechteste, was man
Partial geben kann.

## Layout prüfen ohne Gerät

```bash
make sim SKETCH=sketches/bedienleiste
```

Übersetzt den Sketch nativ und schreibt `tools/simulator/out.png` — mit dem echten
`EPD.cpp` und den echten Fonts, ersetzt sind nur Arduino, WLAN und HTTP. Der Simulator
führt nur `setup()` aus und zeigt damit das Ruhebild.

Für die gedrückten Zustände `render(T_KEINE)` in `setup()` vorübergehend auf die
gewünschte Taste ändern und die Ausschnitte nebeneinanderlegen — so sind die Kerben in den
invertierten Pfeilen aufgefallen:

```bash
cp sketches/bedienleiste/bedienleiste.ino /tmp/bl.bak
for t in T_KEINE T_HOCH T_OK T_RUNTER; do
  cp /tmp/bl.bak sketches/bedienleiste/bedienleiste.ino
  sed -i '' "s/  render(T_KEINE);/  render($t);/" sketches/bedienleiste/bedienleiste.ino
  make sim SKETCH=sketches/bedienleiste SIM_OUT=/tmp/z_$t.png
  magick /tmp/z_$t.png -crop 60x70+0+94 +repage -scale 600% /tmp/k_$t.png
done
cp /tmp/bl.bak sketches/bedienleiste/bedienleiste.ino     # nicht vergessen
magick /tmp/k_T_*.png +append /tmp/zustaende.png
```

Kontrast und Lesbarkeit entscheidet trotzdem das Panel, nicht das PNG.

## Weiterführend

- `../CLAUDE.md` — Zeichen-API, Refresh-Modi, harte Fakten zum Board
- `../../PROGRESS_BAR.md` — Partial-Refresh im Detail, inklusive der Stelle, an die
  `EPD_Clear_R26A6H()` gehört: vor den **ersten** Teilrefresh
- `../../README.md` → *Sketches* — der Absatz zu diesem Sketch
