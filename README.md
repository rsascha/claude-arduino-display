# CrowPanel ESP32 5.79" E-Paper HMI Display

Setup für das Elecrow CrowPanel ESP32 5.79" E-Paper HMI Display (272×792, S/W, SPI).

## Hardware

| | |
|---|---|
| MCU | ESP32-S3-WROOM-1-N8R8 (8 MB Flash, 8 MB OPI-PSRAM, 240 MHz) |
| Display | 5.79", 272×792, Schwarz/Weiß |
| Display-Controller | 2× SSD1683 (zwei Panel-Hälften) |
| USB-UART | CH340 (VID 0x1A86 / PID 0x7523) |
| Port | `/dev/cu.usbserial-210` |
| Akku-Anschluss | SH1.0, 2-polig (1,0 mm Raster), 3,7 V Li-Ion, Ladeschaltung an Bord |

**Der BAT-Stecker ist SH1.0, nicht 1,25 mm.** Ein JST/MX1.25-Akku passt nicht ohne
Adapter. Und: Handelsübliche SH1.0-Akkukabel haben teils vertauschte Polung — vor dem
ersten Anstecken gegen den Aufdruck + / − am Stecker messen. Belege und Quellen in
`material/CLAUDE.md` → *Akkuanschluss*; das Benutzerhandbuch schreibt an der Stelle
nur „BAT" und nennt den Typ nicht.

## Board-Einstellungen (Arduino IDE)

`Werkzeuge` →

| Einstellung | Wert |
|---|---|
| Board | **ESP32S3 Dev Module** |
| Flash Size | **8MB (64Mb)** |
| Partition Scheme | **Huge APP (3MB No OTA/1MB SPIFFS)** |
| PSRAM | **OPI PSRAM** |
| CPU Frequency | 240MHz (WiFi) |
| Flash Mode | QIO 80MHz |
| USB CDC On Boot | Disabled |
| Upload Mode | UART0 / Hardware CDC |
| Upload Speed | **460800** — 921600 scheitert beim CH340 mit "Unable to verify flash chip connection" |
| Port | `/dev/cu.usbserial-210` |

Als FQBN für die CLI:

```
esp32:esp32:esp32s3:FlashSize=8M,PartitionScheme=huge_app,PSRAM=opi,CPUFreq=240,FlashMode=qio,UploadSpeed=460800
```

## Pinbelegung E-Paper

Maßgeblich ist `spi.h` in den Beispielen — **nicht** `DEV_Config.h` aus der `EPD`-Library
(die enthält die Waveshare-Standardpins und passt nicht zu diesem Board).

| Signal | GPIO |
|---|---|
| SCK | 12 |
| MOSI | 11 |
| RES (Reset) | 47 |
| DC | 46 |
| CS | 45 |
| BUSY | 48 |
| Display-Power-Enable | 7 (muss in `setup()` auf HIGH!) |

Ohne `digitalWrite(7, HIGH)` bleibt das Panel stromlos und zeigt nichts an.

## Bedienelemente

Laut Benutzerhandbuch (`material/crowpanel-5.79-benutzerhandbuch.pdf`, S. 2–3) hat das
Board fünf Bedienelemente. Drei davon sitzen an der Gehäusekante und sind frei
programmierbar, zwei liegen auf der Platine und sind fest verdrahtet:

| Element | Lage | Funktion |
|---|---|---|
| MENU | Kante | frei programmierbar |
| Rotary Switch (Drehschalter) | Kante | frei programmierbar, dreh- **und** drückbar |
| EXIT | Kante | frei programmierbar |
| BOOT | Platine | hält GPIO 0 auf LOW → Flash-Modus |
| RESET | Platine | zieht EN auf LOW → Neustart |

Das Handbuch nennt keine GPIOs. Sie stehen in `examples/5.79_key`; die Zuordnung wurde
am 23.08.2026 **am Gerät verifiziert** — jedes Element einzeln betätigt und der Zähler
auf dem Panel abgelesen. Die Positionsangaben gelten für die Ausrichtung **Querformat,
USB-Anschluss oben, Bedienelemente an der linken Kante**:

| Position | Element | GPIO | `#define` in `5.79_key` |
|---|---|---|---|
| oben | EXIT | 1 | `EXIT_KEY` |
| Mitte, Rad nach oben | Drehschalter | 4 | `NEXT_KEY` |
| Mitte, Rad drücken | Drehschalter | 5 | `OK_KEY` |
| Mitte, Rad nach unten | Drehschalter | 6 | `PRV_KEY` |
| unten | MENU | 2 | `HOME_KEY` |

Die Reihenfolge an der Kante ist damit **EXIT – Drehschalter – MENU**. Das Handbuch (S. 2)
zeigt sie umgekehrt, weil seine Abbildung die **Rückseite** mit USB-C unten darstellt —
gedreht auf die obige Ausrichtung kehrt sich die Reihenfolge um.

Nachvollziehen lässt sich das mit `make flash SKETCH=examples/5.79_key`: Jeder Tastendruck
schreibt seinen Namen auf Display und Konsole. Zwei Dinge sind an der Elecrow-Vorlage
angepasst — ein Textrest `Reset Tag` mitten im Code, der das Kompilieren verhinderte,
und `Paint_NewImage(..., 0, ...)` statt `Rotation` (180), damit das Bild bei USB oben
richtig herum steht.

Die Taster liegen gegen Masse und werden mit `pinMode(pin, INPUT)` gelesen;
gedrückt ist **LOW**. GPIO 41 schaltet die Power-LED (`examples/5.79_PWR`).

## Verwendung

```bash
make build                              # Standard-Sketch kompilieren
make flash                              # kompilieren + flashen
make monitor                            # serieller Monitor (115200)
make port                               # erkannten Port anzeigen

make flash SKETCH=examples/5.79_wifi    # anderen Sketch wählen
```

## Struktur

```
sketches/         Eigene Sketches — siehe Abschnitt „Sketches"
libraries/        Elecrow-Libraries — zugleich Sketchbook-libraries/
examples/         Offizielle Elecrow-Beispiele + Demos
factory_firmware/ Werksfirmware als Backup
material/         Handbuch + Datenblätter (PDF und .txt), Fotos des laufenden Panels,
                  Kommando- und GPIO-Tabellen als YAML
tools/simulator/  Host-Build eines Sketches -> PNG, ohne Gerät
Makefile          Build-/Flash-Targets, dazu `make sim` und `make material-txt`
PROGRESS_BAR.md   Fortschrittsanzeige und Partial-Refresh im Detail
LICENSE           MIT für den eigenen Code, plus Herkunft des Fremdmaterials
```

Ein Sketch-Ordner und die `.ino` darin müssen **gleich heißen**
(`sketches/hello_epaper/hello_epaper.ino`). Alle weiteren Dateien im Ordner
werden automatisch mitkompiliert — deshalb liegen `EPD.cpp`, `spi.cpp` &
Co. direkt neben der `.ino` und nicht in `libraries/`.

`src/` wäre als Ordnername ungeeignet: Arduino behandelt ein `src/`
*innerhalb* eines Sketches als Sonderfall (wird als einziger Unterordner
rekursiv kompiliert). Im Sketchbook-Wurzelverzeichnis sind zudem
`libraries/` und `hardware/` fest vergeben.

Der Sketchbook-Pfad von Arduino IDE **und** arduino-cli zeigt auf dieses
Verzeichnis, damit `libraries/`, `examples/` und `sketches/` automatisch gefunden werden.
Rückgängig: in der IDE unter `Einstellungen → Sketchbook-Speicherort`, für die CLI
mit `arduino-cli config set directories.user ~/Documents/Arduino`.

## Sketches

Eigene Sketches in `sketches/`. Kompilieren und flashen mit
`make flash SKETCH=sketches/<name>`.

| Sketch | Kurz | WLAN |
|---|---|---|
| [`hello_epaper`](sketches/hello_epaper) | Text, Formen, Grundlayout — der Einstieg | – |
| [`smiley`](sketches/smiley) | eigene Bogen-Funktion, `safePixel()`, Kreise mit Strichstärke | – |
| [`ha_temperatur`](sketches/ha_temperatur) | ein Sensorwert aus Home Assistant, groß auf dem Display | ja |
| [`ha_verlauf`](sketches/ha_verlauf) | Temperaturkurve über 10 Tage mit Gitter und Achsen | ja |
| [`ha_raeume`](sketches/ha_raeume) | sechs Räume plus Außen in einem Diagramm | ja |
| [`ha_kacheln`](sketches/ha_kacheln) | alle Räume plus Außen als Kacheln, sortiert nach Temperatur | ja |
| [`ha_wetter`](sketches/ha_wetter) | Wind, Luftdruck und Wetterlage in vier Spalten | ja |
| [`ha_wechsel`](sketches/ha_wechsel) | Temperaturen und Wetter im Wechsel, EXIT und MENU bedienbar | ja |
| [`ha_umschalten`](sketches/ha_umschalten) | zwei Sensoren im Wechsel; Testsketch für die Refresh-Modi | ja |
| [`progress_bar`](sketches/progress_bar) | Fortschrittsanzeige, EXIT startet neu — Partial-Refresh | – |
| [`bedienleiste`](sketches/bedienleiste) | die drei Bedienelemente als Laschen am linken Rand, reagieren auf Druck | – |

**`hello_epaper`** — der Startpunkt. Text in mehreren Größen, ein Rahmen, ein paar Formen.
Zeigt, warum der Puffer 27200 Byte groß ist und was `digitalWrite(7, HIGH)` damit zu tun
hat, dass überhaupt etwas erscheint.

**`smiley`** — ergänzt fehlende Zeichenprimitive. `EPD.h` kann Linie, Rechteck und Kreis,
aber **keinen Bogen**; der Mund wird deshalb aus einzelnen Pixeln gebaut (Schrittweite
`0.5f / radius`, sonst reißt die Linie bei großen Radien auf). Hier steht auch der
`safePixel()`-Wrapper, den jeder eigene Zeichencode braucht: `Paint_SetPixel()` prüft seine
Koordinaten nicht und schreibt sonst über den Puffer hinaus.

**`ha_temperatur`** — ein Wert aus Home Assistant über `GET /api/states/<entity_id>`, groß
dargestellt. Fehler landen **auf dem Display**, nicht nur im Log: HTTP 401 und 404 werden
mit einem Hinweis auf die wahrscheinliche Ursache angezeigt, sonst sieht man bei einem
Problem nur ein leeres Panel.

**`ha_verlauf`** — Aktualisierung alle 30 Minuten.
Holt 10 Tage History und zeichnet daraus eine Kurve mit Gitter, Datums- und Gradachse. Die
Antwort (~19 KB) wird direkt mit `strstr()` gescannt statt mit einem JSON-Parser, und je
Pixelspalte wird ein Wert gehalten — der Speicherbedarf bleibt so konstant, egal wie viele
Messpunkte zurückkommen. Warum es 10 und nicht 14 Tage sind, steht unter
*Home Assistant* → `purge_keep_days`.

**`ha_raeume`** — sechs Temperaturkurven in einem Diagramm: Wohnzimmer, Schlafzimmer,
Badezimmer, Küche, Flur und Außen. Zeigt die zwei Probleme, die dabei auftreten und die es
bei einer einzelnen Kurve nicht gibt:

*Keine Farben.* Sechs verschiedene Strichmuster waren der erste Versuch, wirkten auf dem
Panel aber unruhig. Jetzt sind alle Kurven durchgezogen und werden rechts neben ihrem
Endpunkt beschriftet. Weil die Kurven dort oft nur wenige Pixel auseinanderliegen — aktuell
enden alle sechs zwischen 21,6 und 22,9 °C, also innerhalb von zehn Pixeln — weichen
kollidierende Beschriftungen nach unten aus, und eine Führungslinie verbindet jede mit
ihrem tatsächlichen Kurvenende. Ohne die ginge durch das Verschieben genau die Zuordnung
verloren, um die es geht. Der Preis: Wo sich zwei Kurven kreuzen, lässt sich nicht mehr
sagen, welche welche ist.

*Gemeinsame y-Achse.* Außen (der Solarnode) erreicht in der Sonne knapp 40 °C, innen liegt
alles zwischen 20 und 28. Die Achse umfasst deshalb rund 20 K, und die fünf Innenkurven
drängen sich im unteren Drittel. Das ist bewusst so: Eine gekappte Außenkurve würde zeigen,
*dass* es draußen wärmer war, aber nicht wie warm — und genau dieser Kontrast ist der
interessante Teil.

Fällt ein Sensor aus, zeichnet der Sketch die übrigen und markiert den fehlenden in der
Legende mit `n/a`. Nur wenn alle sechs ausfallen, erscheint ein Fehlerbild — anders als
`ha_verlauf`, wo eine fehlende Antwort das ganze Bild kostet.

**`ha_kacheln`** — dieselben sechs Sensoren wie `ha_raeume`, aber ohne Verlauf: ein
3×2-Raster aus Kacheln mit Name, aktuellem Wert in Schriftgröße 48 und dem Trend gegenüber
dem Stand von vor drei Stunden. Sortiert ist nach Temperatur, warm nach kalt in
Lesereihenfolge — oben links der wärmste Raum, unten rechts der kälteste. Die Rangfolge
steckt damit in der Anordnung und braucht keine eigene Beschriftung.

*Trend neben den Wert, nicht darunter.* Als dritte Textzeile kostete er 30 px Höhe; neben
dem Wert kostet er nichts, weil dort ohnehin 94 px frei sind. Die gewonnene Höhe geht an
Name und Trend, die dadurch von Größe 16 auf 24 wachsen konnten — der Messwert selbst nicht,
48 ist die größte Größe, die `EPD_ShowChar()` kennt. Der Trend steht rechtsbündig an der
Kachelkante statt in festem Abstand hinter dem Wert: „23,1" und „-3,5" sind verschieden
breit, ein mitwandernder Trend ließe die sechs Kacheln unruhig wirken.

*Warum drei Stunden.* Die SONOFF-Sensoren lösen 0,1 K auf. Über eine Stunde bewegt sich ein
geschlossener Raum oft nur um genau diesen einen Schritt — der Pfeil zeigte dann Rauschen
an. Über drei Stunden ist ein geöffnetes Fenster deutlich zu sehen, der Tagesgang draußen
aber noch nicht beherrschend. Als unverändert gilt alles unter 0,2 K; dafür gibt es einen
waagerechten Strich statt eines Pfeils.

*Eine Abfrage statt zwei.* Der Trend kommt aus derselben History-Antwort wie der aktuelle
Wert: letzter Messwert der Antwort = jetzt, letzter Messwert vor dem Referenzzeitpunkt =
Vergleich. Ein zusätzliches `GET /api/states/<entity_id>` wäre eine zweite Anfrage für eine
Zahl, die schon da ist. Gesucht wird dabei bewusst der letzte Wert **vor** dem
Referenzzeitpunkt und nicht der erste der Antwort — nur so liefert die Funktion auch dann
den richtigen Vergleich, wenn ihr ein längerer Zeitraum vorgesetzt wird, wie es der
Simulator mit seinen 10-Tage-Dateien tut.

*Zahlen auf Deutsch.* `snprintf()` schreibt immer einen Punkt, und die Locale-Umschaltung,
die das ändern würde, gibt es in der Arduino-Laufzeit nicht — das Komma wird deshalb
nachträglich gesetzt. Damit fällt sofort auf, dass der Font dickte-gleich ist: Das Komma
bekommt dieselbe Zellenbreite wie eine Ziffer, seine Tinte belegt davon aber nur die ersten
7 von 24 px. „23,1" sah dadurch aus, als stünde dort ein Leerzeichen. Der Wert wird deshalb
zeichenweise gesetzt und nach dem Komma nur um `size/4` vorgerückt — weiter nicht, denn
`EPD_ShowChar()` malt die ganze Zelle inklusive Hintergrund und würde das Komma sonst
wieder ausradieren. Die Trendzahl steht bewusst **ohne** Einheit da: korrekt wäre Kelvin,
weil es eine Differenz ist, aber ein „K“ hinter der Zahl fragt auf einem Wohnzimmer-Display
mehr, als es beantwortet. Was gemeint ist, sagen der Pfeil und die Fußzeile.

*Pfeil selbst gezeichnet.* Die Font-Arrays decken ASCII 32..126 ab, ein Pfeilzeichen ist
nicht dabei. Das Dreieck wird deshalb zeilenweise gefüllt statt als Umriss: bei 16 × 12 px
wäre eine 1 px starke Kontur auf dem Panel kaum zu erkennen — dieselbe Beobachtung wie bei
`EPD_DrawCircle()`. Aus dem gleichen Grund sind Rahmen und Rasterlinien 2 px stark.

*Refresh.* Alle 10 Minuten `EPD_FastUpdate()`, jeder sechste Durchgang — also stündlich —
mit Löschzyklus. Genau die Aufteilung, die der Vergleich in `ha_umschalten` nahelegt:
Fast-Update als Regelfall, Vollrefresh nur, um Ghosting einzusammeln. `ha_verlauf` macht
das noch anders und fährt bei jeder Aktualisierung den vollen Zyklus.

Fällt ein Sensor aus, zeigt seine Kachel `n/a` und rutscht ans Ende der Sortierung; ein
Fehlerbild gibt es nur, wenn alle sechs ausfallen.

**`ha_wetter`** — vier Spalten: Kompassrose mit Pfeil, Windgeschwindigkeit, Luftdruck mit
der Änderung der letzten drei Stunden, Wetter-Icon. Der Sketch, an dem am meisten selbst
gezeichnet wird — `EPD.h` kennt Linie, Rechteck und Kreis, sonst nichts.

*Der Pfeil zeigt, woher der Wind kommt.* Home Assistant liefert die Windrichtung nach
meteorologischer Konvention als Herkunft: 341° heißt „aus NNW". Ein kartenüblicher Pfeil in
Wehrichtung zeigte damit nach SSO, während daneben „NNW" steht — Bild und Text sähen aus,
als widersprächen sie sich. Deshalb zeigt der Pfeil auf die NNW-Marke der Rose, und Rose,
Pfeil und Text sagen dasselbe.

*Luftdruck vom Solarnode, nicht aus der Vorhersage.* Beide Quellen sind da und 16 hPa
auseinander: `weather.forecast_home` meldet 1024 hPa auf Meereshöhe reduziert, der
Solarnode misst 1008 hPa vor Ort. Für die Aussage ist das ohne Belang — beim Luftdruck
zählt die Tendenz, und eine Differenz ist höhenunabhängig. Die drei Stunden sind nicht
willkürlich, sondern der meteorologische Standardzeitraum für die Drucktendenz.

*Icons als Umriss über den Umweg Weiß.* Eine flächig schwarze Wolke wäre auf E-Paper ein
Klecks. Ein Umriss aus einzelnen Bögen scheitert daran, dass sich die Bögen der drei
Wolkenbäuche gegenseitig durchschneiden. Der Ausweg: die ganze Form zweimal zeichnen —
erst 2 px größer in Schwarz, dann in Weiß darüber. Übrig bleibt ein sauberer Umriss, und
weil die weiße Füllung alles darunter löscht, verdeckt die Wolke beim Icon *heiter*
automatisch die Sonnenstrahlen hinter ihr.

*Fünf Icons plus Mond.* Home Assistant kennt 15 Wetterzustände. Mit nur Sonne und Regen
wären `cloudy` und `fog` als Sonnentag durchgegangen. Was nicht in der Zuordnungstabelle
steht, wird zur Wolke mit dem Rohzustand als Text — besser ein unbekanntes Wort als ein
falsches Bild. Getestet sind alle sechs Bilder, indem dem Simulator nacheinander jeder
Zustand untergeschoben wurde; auf Schnee oder Mond hätte man sonst bis zum Winter oder bis
zur Nacht warten müssen.

**`ha_wechsel`** — die beiden vorigen Sketches in einem: alle 60 Sekunden wechselt das
Bild zwischen den Temperaturkacheln und der Wetterseite. **EXIT** (GPIO 1) blättert sofort
um und startet die Minute von vorn, damit die aufgerufene Seite in Ruhe lesbar bleibt.
**MENU** (GPIO 2) holt die Daten neu und zeichnet die aktuelle Seite frisch.

*Daten getrennt vom Bildwechsel.* Geholt wird alle zehn Minuten, gezeichnet aus dem
Zwischenspeicher. Andernfalls liefe jede Minute eine Runde von elf HTTP-Anfragen, und beim
Umblättern sähe man die Netzwerklatenz statt das Panel — dieselbe Trennung wie in
`ha_umschalten`. Läuft der Zehn-Minuten-Abruf ab, wird **nicht** neu gezeichnet: der
nächste Wechsel steht ohnehin binnen einer Minute an und bringt die frischen Zahlen mit.
Ein zusätzlicher Bildaufbau wäre ein Refresh-Zyklus für nichts.

*Aufgeteilt auf mehrere Dateien.* `draw.cpp` hält die Zeichenhilfen, `screen_temperaturen.cpp`
und `screen_wetter.cpp` je eine Seite, die `.ino` nur Daten, Tasten und Taktung. In einer
einzigen `.ino` wären es rund 900 Zeilen mit der Hälfte doppelt — beide Seiten brauchen
dieselben Primitive, weil `EPD.h` nur Linie, Rechteck und Kreis kennt. Der Simulator
übersetzt seitdem alle `.cpp` des Sketch-Ordners (siehe `SIM_SRCS` im `Makefile`) statt nur
`EPD.cpp`.

*Fußzeile mit Platzprüfung.* Der Tastenhinweis sitzt mittig in der tatsächlich freien Lücke
zwischen Zeitstempel und rechtem Text, nicht auf der Bildmitte: die rechten Texte sind je
Seite verschieden lang, und auf der Temperaturseite stand sonst
„EXIT blaetPfeil und Zahl:" übereinander. Passt der Hinweis nicht, entfällt er ganz.

*Ein Wechsel pro Minute sind 1.440 Bildwechsel am Tag.* Deshalb Fast-Update als Regelfall
und nur jeder 60. Aufbau — also stündlich — mit Löschzyklus. Bei jedem Wechsel voll zu
refreshen hieße sechzigmal pro Stunde mehrfaches Schwarzblitzen; das fällt mehr auf als
das Ghosting, das es verhindert.

**`ha_umschalten`** — Testsketch. Blendet im 5-Sekunden-Takt zwischen Wohnzimmer- und
Schlafzimmerkurve um und wechselt dabei reihum Voll-, Fast- und Partial-Refresh durch;
Modus und gemessene Dauer stehen auf dem Display. Ergebnis siehe
*Die drei Refresh-Modi*. Beide Verläufe werden einmal geholt und im Speicher gehalten —
löste jedes Umschalten eine HTTP-Anfrage aus, würde der Test die Netzwerklatenz messen
statt das Panel. **Kein Dauerbetrieb:** E-Paper verträgt keine unbegrenzte Zahl an
Refresh-Zyklen.

**`progress_bar`** — fünf Segmente, alle 3 Sekunden eines mehr, EXIT startet neu. Der
Gegentest zu `ha_umschalten`: Partial taugt nichts für einen Vollbildwechsel, wohl aber
hier, wo sich pro Schritt nur ein Segment ändert. Der Sketch zeigt die zwei Bedingungen,
ohne die das nicht funktioniert — `EPD_Clear_R26A6H()` vor dem ersten Partial-Update und
kein Hardware-Reset zwischendurch. Ausführlich in **[`PROGRESS_BAR.md`](PROGRESS_BAR.md)**.

**`bedienleiste`** — die drei Bedienelemente an der linken Gehäusekante bekommen ein
Gegenstück auf dem Panel: oben eine Lasche mit **E** für EXIT, unten eine mit **M** für
MENU, dazwischen die des Drehschalters. Wer drückt, sieht seine Lasche schwarz werden.
Damit ist ohne Serial-Monitor am Gerät ablesbar, welches Element wo sitzt und welchen GPIO
es zieht.

*Die y-Positionen sind gemessen, nicht gerechnet.* 60–70 für EXIT, 110–150 für den
Drehschalter, 200–210 für MENU. Abgelesen wurden sie mit einem Wegwerf-Sketch, der an
beiden langen Kanten ein Maßband von 0 bis 271 zeichnet — Striche alle 10 px, Beschriftung
alle 50 px. Er hat seinen Zweck erfüllt und ist wieder gelöscht; die Zahlen stehen jetzt
hier und in `CLAUDE.md`. EXIT und MENU liegen
mit ihren Mitten 65 und 205 symmetrisch zur Bildmitte, das hätte man annehmen können. Die
Mitte des Drehschalters liegt aber bei 130 und damit sechs Pixel darüber, und die Höhen der
drei Elemente stehen in keinem Datenblatt. Beides fällt nur beim Messen auf.

*Keine Lasche hat die Höhe ihres Elements.* Sie sitzt auf dessen **Mitte** — das ist die
Information, auf die es beim Zuordnen ankommt; die Höhe richtet sich danach, was
hineinpassen muss. Die beiden Taster sind nur 11 px hoch, ein Buchstabe in Größe 24
braucht 24 px, und kleiner ist auf dem Panel nicht mehr sicher lesbar: also 32 px. Das Rad
ist 41 px hoch, seine Lasche 53 — Pfeil, `OK` und Pfeil brauchen zusammen mehr, wenn sie
Abstand behalten sollen.

*Drei Funktionen, eine Lasche.* Der Drehschalter ist ein einziges Bedienelement mit drei
Kontakten (GPIO 4 hoch, 5 drücken, 6 runter). Er bekommt deshalb **eine** Lasche, die in
drei Zonen geteilt ist — Pfeil hoch, `OK`, Pfeil runter — und beim Bedienen färbt sich nur
die betroffene Zone. Drei getrennte Laschen hätten drei Bedienelemente vorgetäuscht, wo
nur eines ist. Die Pfeile sind aus Pixeln gefüllt: Die Font-Arrays decken ASCII 32..126 ab,
ein Pfeilzeichen ist nicht dabei.

*Zwischen den drei Zonen liegen 4 px, und das ist ein Bugfix, kein Geschmack.* Zuerst
lagen Pfeil, `OK` und Pfeil ohne Abstand aneinander. Passiv sah das gut aus — invertiert
nicht: Die Zonengrenze lag genau auf der Inhaltskante, und der weiße Pfeil stieß mit seiner
breitesten Zeile an die weiße Fläche daneben. Er las sich dadurch als **Kerbe** im
schwarzen Balken statt als Pfeil, und in der `OK`-Zone wirkten die beiden schwarzen Pfeile
abgeschnitten. Aufgefallen ist das erst, als alle vier Zustände nebeneinander gerendert
wurden — einzeln und in Originalgröße sah jeder für sich plausibel aus. Der Simulator kann
das, ohne zu flashen; der Weg steht in `sketches/bedienleiste/CLAUDE.md`.

*Weiß auf Schwarz kostet nichts.* Für die gedrückte Lasche genügt `EPD_ShowString(...,
WHITE)`. Das funktioniert, weil `EPD_ShowChar()` die ganze Zelle malt und für den
Hintergrund `!color` setzt — bei `WHITE` also Schwarz. Dieselbe Eigenschaft, die in
`ha_kacheln` das Komma ausradiert hat, ist hier genau das Gewünschte.

*Refresh.* Pro Tastendruck zwei Partial-Updates — eines beim Drücken, eines beim
Loslassen. Die Bedingungen dafür stehen in [`PROGRESS_BAR.md`](PROGRESS_BAR.md):
`EPD_Clear_R26A6H()` vor dem ersten Partial-Update und kein Hardware-Reset zwischendurch.

*Der Vollrefresh läuft auf Zuruf, und zwar in zwei Schritten.* Erst war er alle 20
Teilbilder fällig — und traf damit mitten ins Bedienen: Der Löschzyklus lässt das Panel
mehrere Sekunden **weiß** stehen, und wer gerade draufschaut, hält das für einen Absturz.
Jetzt macht der erste Druck auf **EXIT** aus dem `E` ein `R`, und erst der Druck auf `R`
wischt. Jede andere Taste nimmt das `R` zurück. Zwei Schritte deshalb, weil ein
Bedienelement, das man nur antippen will, um zu sehen ob es reagiert, nicht nebenbei das
halbe Display für Sekunden ausknipsen soll. Gewischt wird dabei das **Ruhebild**, nicht die
gedrückte Lasche — sonst müsste das Loslassen den großen schwarzen Block per Teilrefresh
wieder wegnehmen, und genau das kann Partial am schlechtesten. Wie viele Teilbilder seit
dem letzten Wischen aufgelaufen sind, steht in der Fußzeile.

*Der Neuaufbau nach dem Löschzyklus ist am Gerät ermittelt, nicht hergeleitet.* Drei
Anläufe, drei verschiedene Fehlerbilder: einmal blieb das Panel komplett weiß, einmal kam
das Bild blass, einmal fehlten Teile des Textes. Der Grund ist, dass RAM `0x26`/`0xA6` im
Datenblatt „Write RAM (RED)" heißt, im Elecrow-Treiber aber als *vorheriges Bild* für den
Teilrefresh dient — und nirgends steht, was davon bei einem vollen Update gilt. Statt weiter
zu raten wurden fünf Rezepte nacheinander auf das Panel geschickt und angesehen, wie es
`ha_umschalten` schon für die drei Refresh-Modi gemacht hat:

| Neuaufbau | ohne `EPD_Clear_R26A6H()` | mit `EPD_Clear_R26A6H()` |
|---|---|---|
| `EPD_Update()` (0xF7) | Text unvollständig | sauber, flackert |
| `EPD_FastUpdate()` (0xC7) | **sauber, kein Flackern** | Panel bleibt weiß |

Die beiden Zutaten müssen also über Kreuz zusammenpassen. Gewählt ist die ruhige
Kombination — das Flackern übernimmt der Löschzyklus davor, der Neuaufbau muss es nicht
wiederholen. Sie entspricht zugleich Elecrows eigenem `5.79_Global_refresh`. Dass
`EPD_Clear_R26A6H()` damit nicht überflüssig ist, sondern nur an eine andere Stelle gehört
— vor den **ersten Teilrefresh** —, steht in [`PROGRESS_BAR.md`](PROGRESS_BAR.md).

*`INPUT` genügt.* Die fünf Tasten haben 4,7-kΩ-Pull-ups auf der Platine
(`material/SCHALTPLAN.md` → *Tasten*), der Pin ist also nicht offen. `INPUT_PULLUP` würde
nichts verbessern; entprellt wird trotzdem, mit zwei Messungen im Abstand von 15 ms.

Sketches mit WLAN brauchen eine `secrets.h` neben der `.ino`:

```bash
cp sketches/<name>/secrets.h.example sketches/<name>/secrets.h
```

Sie ist per `.gitignore` ausgeschlossen und wird **nicht** committet — committet wird nur
`secrets.h.example` mit Platzhaltern.

## Simulator

Layout ausprobieren, ohne zu flashen:

```bash
make sim-fetch SKETCH=sketches/ha_raeume   # echte HA-Antworten nach tools/simulator/data/
make sim       SKETCH=sketches/ha_raeume   # -> tools/simulator/out.png
```

Der Sketch wird dabei **nativ auf dem Mac** übersetzt und sein Bildpuffer als PNG
ausgegeben. Entscheidend für die Aussagekraft: `EPD.cpp` und `EPDfont.h` sind die *echten*
Dateien aus dem Sketch-Ordner, der Sketch selbst bleibt unverändert. Ersetzt sind nur
Arduino, WLAN und HTTP (`tools/simulator/arduino/`). Der `HTTPClient`-Stub liefert dort, wo
auf dem Gerät die HTTP-Antwort käme, eine lokale Datei — `fetch.sh` holt sie per `curl` aus
der echten Instanz und liest die entity_ids dafür aus der `.ino`, damit keine zweite Liste
veralten kann.

Auch die 8 Pixel Naht bei x = 396 rechnet der Simulator heraus: Das PNG zeigt die
sichtbaren 792 Pixel, nicht den 800 Pixel breiten Rohpuffer.

**Warum dieser Aufwand statt eines schnellen Nachbaus:** Ein Prototyp mit eigenen
Schriftmetriken hat in `ha_verlauf` eine 6-px-Überlappung *nicht* gezeigt, die auf dem Panel
da war. Ein Simulator ist nur so viel wert wie seine Übereinstimmung mit dem Original.
Gleich der erste Lauf hat sich bezahlt gemacht: In der Kopfzeile von `ha_raeume` stand
„Stand 23.08. 16:05 °C" — das Gradzeichen aus `ha_verlauf`, wo es hinter einem Messwert
saß, hing hier hinter der Uhrzeit.

Braucht ImageMagick (`brew install imagemagick`) für die Umwandlung von PBM nach PNG.

## Elecrow-Beispiele

Vorlagen zum Kopieren, in `examples/`.

| Sketch | Inhalt |
|---|---|
| `5.79_Global_refresh` | Vollbild-Refresh, Bild anzeigen |
| `5.79_partial_refresh` | Partielles Update (schnell, ohne Flackern) |
| `5.79_GPIO` | GPIO-Grundlagen |
| `5.79_key` | Tasten-Eingabe |
| `5.79_PWR` | Deep Sleep / Stromsparen |
| `5.79_TF` | SD-Karte |
| `5.79_wifi`, `5.79_wifi_http` | WLAN, HTTP |
| `5.79_BLE` | Bluetooth LE |
| `Demos/` | Openweather-Demo, BLE-/WiFi-Refresh |

## Quellen

- [Elecrow Wiki – 5.79" HMI Display](https://www.elecrow.com/wiki/CrowPanel_ESP32_E-paper_5.79-inch_HMI_Display.html)
- [Elecrow Wiki – Arduino Tutorial](https://www.elecrow.com/wiki/CrowPanel_ESP32_E-Paper_5.79inch_Arduino_Tutorial.html)
- [GitHub – Elecrow-RD/CrowPanel-ESP32-5.79-E-paper-HMI-Display-with-272-792](https://github.com/Elecrow-RD/CrowPanel-ESP32-5.79-E-paper-HMI-Display-with-272-792)
- [Elecrow Forum – DIS08792E mit externem Akku betreiben](https://forum.elecrow.com/discussion/1063/dis08792e-crowpanel-esp32-5-79-e-paper-hmi-display-powering-with-an-external-battery) — SH1.0 statt 1,25 mm, Hinweis auf vertauschte Polung
- Benutzerhandbuch (deutsch): `material/crowpanel-5.79-benutzerhandbuch.pdf` — Rückseiten-Beschriftung, Bedienelemente, Spezifikation
- [SSD1683 Datenblatt](https://www.elecrow.com/download/product/DIS08792E/SSD1683_Datasheet.PDF) — Display-Controller, 49 Seiten; liegt auch als `material/ssd1683-datasheet.pdf` im Repo
- [ESP32-S3-WROOM-1 Datenblatt](https://www.elecrow.com/download/product/DIS08792E/esp32-s3-wroom-1_datasheet.pdf) — Modul, Pinout, elektrische Werte; liegt auch als `material/esp32-s3-wroom-1-datasheet.pdf` im Repo

Die beiden Datenblatt-Links stehen im Handbuch nur als QR-Code (S. 4–5); der Text des
PDFs enthält sie nicht. Deshalb liegen die Datenblätter mit im Repo — geht der Elecrow-
Download irgendwann offline, wäre der Weg dorthin sonst nur noch über die QR-Codes.

## Zeichen-API (Elecrow `EPD.h`)

GxEPD2 liegt zwar in `libraries/`, hat aber **keine Panel-Klasse für 272×792**
(129 Display-Klassen geprüft, SSD1683 nur in 4,2"-Varianten). Für dieses Display
werden daher Elecrows `EPD.cpp`/`spi.cpp` direkt im Sketch-Ordner verwendet.

```c
Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);  // Puffer anlegen
Paint_Clear(WHITE);

EPD_ShowString(x, y, "Text", size, BLACK);   // size: nur 8, 12, 16, 24, 48
EPD_ShowNum(x, y, wert, stellen, size, BLACK);
EPD_DrawLine(x1, y1, x2, y2, BLACK);
EPD_DrawRectangle(x1, y1, x2, y2, BLACK, mode);   // mode 0 = Umriss, 1 = gefüllt
EPD_DrawCircle(cx, cy, r, BLACK, mode);
EPD_ShowPicture(x, y, w, h, bitmap, WHITE);

EPD_Display(ImageBW);   // Puffer übertragen
EPD_FastUpdate();       // sichtbar machen
EPD_DeepSleep();        // Bild bleibt auch ohne Strom stehen
```

Stolperfallen:

- **`digitalWrite(7, HIGH)` in `setup()`** schaltet die Panel-Spannung. Fehlt das, bleibt das Display dunkel.
- **`EPD_W` ist 800**, sichtbar sind aber nur **792** Pixel. Grund: zwei SSD1683-Controller
  mit je einer Panel-Hälfte, dazwischen 8 ungenutzte Pixel im Speicher. `Paint_SetPixel()`
  korrigiert das selbst (`if (Xpoint >= 396) Xpoint += 8`), gültige x-Werte sind also 0..791.
- **Ausrichtung:** `EPD.h` definiert `Rotation 180` — damit steht das Bild bei diesem
  Aufbau auf dem Kopf. In `hello_epaper.ino` wird stattdessen `DISPLAY_ROTATION = 0`
  an `Paint_NewImage()` übergeben; erlaubt sind 0, 90, 180, 270.
- **Nur die Schriftgrößen 12, 16, 24 und 48 funktionieren.** Alle anderen — **auch 8** —
  zeichnen kommentarlos nichts: `EPD_ShowChar()` hat für 8 zwar eine Größenberechnung,
  aber keinen Zweig zum Font-Array und verlässt die Funktion per `else return;`.
- **Nur ASCII 32..126.** Die Font-Arrays haben 95 Einträge, der Index ist `chr - ' '`.
  Ein `°` (176) ergäbe Index 144 und läse über das Array hinaus — Umlaute ebenso.
  Statt `°C` also `C` schreiben oder den Kreis selbst zeichnen.
- Zeichenbreite ist `size/2` — Text selbst auf Überlauf prüfen, es wird nicht umgebrochen.

### Die drei Refresh-Modi

`EPD_Update()`, `EPD_FastUpdate()` und `EPD_PartUpdate()` unterscheiden sich im Code nur
im Parameter zu Kommando `0x22` (`0xF7`, `0xC7`, `0xDC`; SSD1683-Datenblatt S. 29). Wie
verschieden sie am Panel aussehen, zeigt `sketches/ha_umschalten`, das im 5-Sekunden-Takt
zwischen zwei Kurven umblendet und dabei die Modi durchwechselt. Befund am realen Gerät:

| Modus | Verhalten | wofür |
|---|---|---|
| `EPD_Update()` mit `EPD_Display_Clear()` davor | dauert lange, das Panel wird **mehrfach** komplett schwarz | selten, gegen Ghosting |
| `EPD_FastUpdate()` | schnell und sauber | **Regelfall für Vollbildwechsel** |
| `EPD_PartUpdate()` | für Vollbildwechsel unbrauchbar | nur für kleine Ausschnitte |

Dass Partial hier nichts bringt, hat einen Grund: `EPD_Display()` schreibt immer das
**komplette** RAM beider Controller, ein Fenster gibt es nicht. Der Modus lohnt sich erst,
wenn man vorher den RAM-Adressbereich (`0x44`/`0x45`, S. 34–35) auf den geänderten
Ausschnitt einengt — für eine Fortschrittsanzeige oder eine tickende Uhr also, nicht für
ein neues Vollbild.

Das lange Schwarzwerden von `EPD_Update()` kommt nicht vom Kommando selbst, sondern vom
Löschzyklus davor: erst wird das Panel weiß geschrieben und aktualisiert, dann das Bild.
`ha_verlauf` macht das bei jeder Aktualisierung.

**E-Paper verträgt keine unbegrenzte Zahl an Refresh-Zyklen.** Ein Sketch, der im
Sekundentakt aktualisiert, ist als Test in Ordnung, als Dauerbetrieb nicht.

## Lizenz

Der eigene Code und die Dokumentation stehen unter MIT (siehe `LICENSE`).

`libraries/`, `examples/`, `factory_firmware/` und die PDFs in `material/` sind
Fremdmaterial und behalten die Lizenz ihrer jeweiligen Rechteinhaber — darunter
GPL-3.0 (GxEPD2) und AGPL-3.0 (EPaperDrive). Die Aufstellung steht in `LICENSE`.

Fotos in `material/` werden **ohne EXIF** committet. iPhone-Aufnahmen tragen sonst
GPS-Koordinaten mit ±5 m Genauigkeit; der Umwandlungsbefehl in `CLAUDE.md` entfernt
sie mit `-strip`.
