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
| EXIT (GPIO 1) | Lasche schwarz; der Buchstabe wechselt `E` → `R` |
| **R** drücken | Vollrefresh: Panel wird gewischt, danach steht das Ruhebild |
| Rad hoch / drücken / runter (GPIO 4/5/6) | die betroffene Zone der Radlasche wird schwarz |
| MENU (GPIO 2) | Lasche schwarz |
| jede Taste außer EXIT | nimmt ein scharfes `R` wieder auf `E` zurück |

Der Vollrefresh braucht zwei Drücker, weil er das Panel mehrere Sekunden weiß stehen
lässt. Wer nur antippen will, ob die Lasche reagiert, soll dabei nicht das halbe
Display ausknipsen.

## Was man hier nicht verstellen sollte, ohne es zu wissen

**Die y-Positionen sind gemessen, nicht gerechnet.** `EXIT_MITTE = 65`,
`RAD_Y0/RAD_Y1 = 110/150`, `MENU_MITTE = 205` — am Gerät abgelesen. Wer sie ändert,
verschiebt die Laschen gegen die echten Taster. Nachmessen geht mit einem Sketch, der
an beiden langen Kanten ein Maßband von 0 bis 271 zeichnet (Striche alle 10 px,
Beschriftung alle 50 px); der Wegwerf-Sketch dafür ist nach dem Messen gelöscht worden.

**Die äußeren Laschen sind absichtlich größer als ihre Taster.** Die Taster sind nur
11 px hoch, ein Buchstabe in Größe 24 braucht 24 px. `KLEIN_H = 32` zentriert auf der
Tastermitte ist der Kompromiss. Kleiner geht nur ohne Beschriftung.

**Der Drehschalter bekommt eine Lasche, nicht drei.** Er ist *ein* Bedienelement mit
drei Kontakten (Quadratur-Encoder plus Tastkontakt). Drei getrennte Laschen würden drei
Bedienelemente vortäuschen. Deshalb drei Zonen in einer Form, und invertiert wird nur
die betroffene Zone.

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
führt nur `setup()` aus und zeigt damit das Ruhebild. Für einen gedrückten Zustand
vorübergehend `render(T_KEINE)` in `setup()` auf die gewünschte Taste ändern.

Kontrast und Lesbarkeit entscheidet trotzdem das Panel, nicht das PNG.

## Weiterführend

- `../CLAUDE.md` — Zeichen-API, Refresh-Modi, harte Fakten zum Board
- `../../PROGRESS_BAR.md` — Partial-Refresh im Detail, inklusive der Stelle, an die
  `EPD_Clear_R26A6H()` gehört: vor den **ersten** Teilrefresh
- `../../README.md` → *Sketches* — der Absatz zu diesem Sketch
