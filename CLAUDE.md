# CLAUDE.md

Arduino-Projekt für das **Elecrow CrowPanel ESP32 5.79" E-Paper HMI Display**
(272×792, S/W, ESP32-S3-WROOM-1-N8R8, 2× SSD1683 über SPI).

Ausführliche Doku inkl. Quellen: `README.md`.
Partial-Refresh im Detail: `sketches/progress_bar/CLAUDE.md`.

## Bauen und Flashen

```bash
make build                                  # kompilieren
make flash                                  # kompilieren + flashen
make monitor                                # serieller Monitor, 115200
make port                                   # erkannten Port anzeigen
make flash SKETCH=examples/5.79_wifi        # anderen Sketch
```

Default-Sketch ist `sketches/hello_epaper`. FQBN und Port stehen im `Makefile`;
der Port wird per `arduino-cli board list` automatisch erkannt.

`make monitor` funktioniert nur interaktiv im Terminal: `arduino-cli monitor`
beendet sich ohne TTY sofort mit Exit-Code 0 und ohne Ausgabe. Für automatisiertes
Mitlesen den Port direkt öffnen und dabei RTS pulsen (löst den Reset aus, sonst
verpasst man die Boot-Ausgabe) — Muster siehe Commit-Historie zu `ha_verlauf`.

## Verifizierte Umgebung

| | |
|---|---|
| Core | `esp32:esp32@3.3.11` |
| Board | `esp32:esp32:esp32s3` (ESP32S3 Dev Module) |
| Port | `/dev/cu.usbserial-210` (CH340) |

Sketchbook-Pfad von Arduino IDE **und** arduino-cli zeigt auf dieses Verzeichnis.
Die IDE hat eine **eigene** Config unter `~/.arduinoIDE/arduino-cli.yaml` — Änderungen
an der CLI-Config (`~/Library/Arduino15/arduino-cli.yaml`) wirken dort **nicht**.
Beide müssen gepflegt werden.

## Harte Fakten, die sonst Zeit kosten

- **Upload mit 460800 Baud.** 921600 (der Core-Default) bricht an diesem CH340 mit
  `Unable to verify flash chip connection` ab, *nachdem* der Stub-Flasher schon lief.
- **`digitalWrite(7, HIGH)` in `setup()`** schaltet die Panel-Spannung. Fehlt das,
  bleibt das Display dunkel — unabhängig davon, ob der restliche Code stimmt.
- **`EPD_W` ist 800, sichtbar sind 792.** Zwei SSD1683-Controller mit je einer Hälfte,
  dazwischen 8 ungenutzte Pixel. `Paint_SetPixel()` korrigiert das selbst
  (`if (Xpoint >= 396) Xpoint += 8`). Puffer muss 27200 Byte groß sein.
- **Schriftgrößen: nur 12, 16, 24, 48.** Auch 8 zeichnet nichts — `EPD_ShowChar()`
  kennt dafür keinen Font-Zweig und steigt per `else return;` aus.
  Zeichenbreite ist `size/2`, `EPD_ShowString()` bricht nicht um.
- **Der Font ist dickte-gleich — auch das Komma.** `EPD_ShowString()` rückt je Zeichen
  stur `size/2` px vor. Die Tinte einer Ziffer füllt die Zelle fast ganz aus (bei Größe 48
  die Spalten 2..23 von 24), die des Kommas nur die Spalten 0..6. Hinter dem Komma klaffen
  dadurch ~20 leere Pixel und „23,1" sieht aus, als stünde dort ein Leerzeichen. Abhilfe:
  zeichenweise mit `EPD_ShowChar()` setzen und nach Komma/Punkt nur `size/4` vorrücken
  (`showNumber()` in `ha_kacheln`). Weiter nicht — `EPD_ShowChar()` malt die **ganze Zelle
  inklusive Hintergrund**, eine zu weit nach links gezogene Folgezelle radiert das Komma
  wieder aus.
- **Nur ASCII 32..126.** Font-Arrays haben 95 Einträge, Index ist `chr - ' '`.
  `°` (176) → Index 144, liest über das Array hinaus. Gilt auch für Umlaute.
- **Ausrichtung:** `EPD.h` liefert `Rotation 180` — steht bei diesem Aufbau auf dem Kopf.
  Sketches übergeben stattdessen ein eigenes `DISPLAY_ROTATION = 0` an `Paint_NewImage()`.
- **`Paint_SetPixel()` prüft die Koordinaten NICHT.** Es rechnet direkt
  `Addr = X/8 + Y*widthByte` und schreibt in den Puffer; die Parameter sind `uint16_t`,
  ein negativer Wert wird klaglos zu einer riesigen Zahl. Eigener Zeichencode braucht
  einen `safePixel()`-Wrapper, der in `int` rechnet und vorher abfängt — sonst ist ein
  Koordinatenfehler kein falsches Bild, sondern ein Speicherüberschreiber.
- **Es gibt keine Bogen-Funktion.** `EPD.h` kann Linie, Rechteck, Kreis (`mode` 0 = Umriss,
  1 = gefüllt) — mehr nicht. Bögen selbst aus Pixeln bauen, Schrittweite `0.5f / radius`,
  sonst reißt die Linie bei großen Radien auf.
- **`EPD_DrawCircle()` zeichnet 1 px dünn** und ist auf dem Panel kaum zu sehen. Für
  sichtbare Linien mehrere Kreise mit wachsendem Radius übereinander legen.
- **Fast-Update ist der Regelfall, nicht Vollrefresh.** Die drei Modi unterscheiden sich
  nur im Parameter zu `0x22` (`0xF7`/`0xC7`/`0xDC`). Am Gerät verglichen (`ha_umschalten`):
  `EPD_FastUpdate()` ist schnell und sauber — die richtige Wahl für einen Bildwechsel.
  Der Vollrefresh *mit* `EPD_Display_Clear()` davor lässt das Panel **mehrfach** komplett
  schwarz werden und dauert spürbar; er verhindert nur Ghosting über viele Durchgänge.
  `EPD_PartUpdate()` ist für Vollbildwechsel unbrauchbar: `EPD_Display()` schreibt immer
  das ganze RAM beider Controller. Partial lohnt erst mit eingeengtem RAM-Adressbereich
  (`0x44`/`0x45`) für kleine Ausschnitte — Fortschrittsbalken, tickende Uhr.
- **Vor dem ersten `EPD_PartUpdate()` gehört `EPD_Clear_R26A6H()`** — sonst wird die erste
  Änderung hellgrau statt schwarz. Der SSD1683 hat zwei RAMs: `0x24`/`0xA4` das neue Bild,
  `0x26`/`0xA6` das vorherige. Partial wählt seine Waveform pro Pixel aus dem Übergang
  *alt → neu* und braucht beide. `EPD_Display_Clear()` hinterlässt in `0x26` aber `0x00`,
  und `EPD_Display()` schreibt **nur** `0x24`/`0xA4` — das falsche „alte Bild" bleibt also
  stehen. `EPD_Clear_R26A6H()` setzt es auf `0xFF` (= vorher weiß). Ab dem zweiten Update
  führt der Controller `0x26` selbst nach, deshalb fällt der Fehler nur beim ersten auf und
  sieht nach einem Kontrastproblem des Panels aus. Elecrow macht es in `5.79_key` Zeile
  36–38 genauso; im Treiber steht kein Kommentar dazu, das Datenblatt schweigt ebenfalls.
- **`EPD_Clear_R26A6H()` gehört vor den ersten Partial-Refresh — und sonst nirgendwohin.**
  Vor einem vollen oder schnellen Neuaufbau richtet er Schaden an. Am Gerät verglichen
  (`bedienleiste`, fünf Rezepte hintereinander auf dasselbe Panel), jeweils nach
  `EPD_Display_Clear()` + `EPD_Update()`:

  | Neuaufbau | ohne `EPD_Clear_R26A6H()` | mit `EPD_Clear_R26A6H()` |
  |---|---|---|
  | `EPD_Update()` (`0xF7`) | Text unvollständig | sauber, flackert |
  | `EPD_FastUpdate()` (`0xC7`) | **sauber, kein Flackern** | Panel bleibt weiß |

  Die Zutaten müssen also über Kreuz zusammenpassen; drei der vier Kombinationen sehen aus
  wie ein Hardware- oder Kontrastproblem, sind aber ein Register-Missverständnis. Zweimal
  hintereinander `EPD_Display()` + `EPD_Update()` lässt das Panel ebenfalls weiß — der
  zweite Aufbau rechnet dann gegen ein `0x26`, das der Controller nach dem ersten Update
  selbst nachgeführt hat.
- **Die Regel für `0x26`/`0xA6` lautet: dort muss stehen, was das Panel tatsächlich zeigt.**
  `EPD_Clear_R26A6H()` ist nur der **Sonderfall** „vorher alles weiß" und deshalb allein
  direkt nach dem Löschzyklus richtig. Wer Vollrefresh und Teilrefresh abwechselt, braucht
  danach mehr: Nach dem Wischen steht das neue Bild auf dem Panel, in `0x26` aber noch der
  Rest des Löschzyklus — der nächste Teilrefresh kommt dann **grau, egal an welcher Stelle**,
  und erst der übernächste stimmt. Abhilfe ist, den Bildpuffer selbst nach `0x26`/`0xA6` zu
  schreiben; die Bausteine dafür exportiert `EPD_Init.h` (`EPD_SetRAMMP/MA/SP/SA`,
  `EPD_WR_REG`, `EPD_WR_DATA8`), die Adressrechnung ist die aus `EPD_Display()`. Beispiel:
  `merkeAltesBild()` in `sketches/bedienleiste`. **`0x26` benutzt dieselbe Kodierung wie
  `0x24` (1 = weiß)** — der Puffer wird unverändert übernommen; am Gerät bestätigt. Der
  Datenblattname „Write RAM (RED)" führt hier in die Irre.

- **Fast-Update mitten im Betrieb treibt das Panel ins Schwarze.** `EPD_FastUpdate()`
  (`0xC7`) lädt keine LUT nach und benutzt die, die gerade geladen ist — nach Teilrefreshs
  also deren Waveform. Am Gerät (`bedienleiste`) kam der erste Inhaltswechsel danach nur
  grau heraus, beim zweiten war fast alles schwarz. `EPD_FastUpdate()` gehört deshalb
  ausschließlich direkt hinter einen Hardware-Reset. GxEPD2 führt für denselben Fall ein
  Flag `_using_partial_mode` und initialisiert bei jedem Moduswechsel neu; die Alternative
  ist, den Moduswechsel ganz zu vermeiden und zur Laufzeit nur Teilrefresh zu fahren.
- **RAM-Fenster: Die Registerfolge steht, die Wirkung ist ungeklärt.** Ein Teilrefresh muss
  nicht das ganze RAM schreiben — `0x11`/`0x44`/`0x45`/`0x4E`/`0x4F` engen den Bereich ein
  (Master), `0x91`/`0xC4`/`0xC5`/`0xCE`/`0xCF` den des Slave, dessen X-Adressen
  **rückwärts** zählen: `slaveX = 99 - Pufferspalte`. Pufferzeile `r` landet auf Panelzeile
  `271 - r`. Vorlage ist GxEPD2 (`_setPartialRamArea`), die Konventionen stammen aus
  `EPD_Init.cpp`. Ein Fenster über den Laschenstreifen sind 1.632 statt 27.200 Byte, und
  der Tastendruck wird dadurch spürbar schneller. **Aber:** Ein kleines Fenster macht am
  Gerät den vorherigen Inhaltswechsel wieder rückgängig, ein Fenster über das ganze Bild
  nicht. Beides belegt, keine Erklärung. Details und die widerlegten Hypothesen in
  `sketches/bedienleiste/CLAUDE.md`.
- **Der Drehschalter verhält sich wie zwei Taster, nicht wie ein Encoder.** Der Schaltplan
  nennt einen Quadratur-Encoder (`TM_2024A`); gemessen ist: Ruhezustand beide HIGH, je
  Betätigung zieht **eine** Phase für 250–640 ms, die Phasen überlappen sich nie. Ein
  Quadratur-Dekoder hätte nichts zu dekodieren. Messmethode und Werte in
  `sketches/bedienleiste/tasten.h`. **Lücke:** Gemessen wurden einzelne Rastungen mit
  Pausen, nicht schnelles Durchdrehen — dabei könnten überlappende Phasen auftreten.

- **Im Partial-Betrieb nicht zwischendurch neu initialisieren.** `EPD_FastMode1Init()`
  enthält einen `EPD_HW_RESET()`, und ein zurückgesetzter Controller kennt das vorherige
  Bild nicht mehr. Einmal je Durchlauf initialisieren, dann nur noch
  `EPD_Display()` + `EPD_PartUpdate()`.
- **Refresh-Zyklen sind endlich.** Sekundentakt ist als Test in Ordnung, als Dauerbetrieb
  nicht. `ha_umschalten` ist ein Testsketch, kein Betriebszustand.
- **Schriftgröße 16 ist auf dem Panel gut lesbar** — für Achsenbeschriftungen reicht sie,
  24 ist dafür nicht nötig (am realen Gerät geprüft).
- **Drei Bedienelemente an der Kante: MENU, Drehschalter, EXIT** — alle frei
  programmierbar, gegen Masse, `pinMode(pin, INPUT)`, gedrückt ist LOW. BOOT und RESET
  sitzen auf der Platine und sind fest verdrahtet. Dass `examples/5.79_key` fünf Tasten
  kennt, ist kein Fehler: der Drehschalter belegt drei davon (zwei Drehrichtungen plus
  Druck). GPIO-Zuordnung und Quelle stehen in `README.md` → *Bedienelemente*.
- **Die Bedienelemente liegen bei y = 60..70, 110..150 und 200..210** (EXIT, Drehschalter,
  MENU; Ausrichtung USB oben, Elemente links). Am Gerät abgelesen mit einem Wegwerf-Sketch,
  der an beiden langen Kanten ein Maßband von 0 bis 271 zeichnet. Wer etwas auf Höhe eines
  Bedienelements platzieren will, braucht diese Zahlen — im Datenblatt stehen sie nicht,
  und geschätzt sitzt es daneben.
- **Textzeilen brauchen `size` Pixel Höhe, nicht weniger.** Zwei untereinander liegende
  16er-Zeilen brauchen also mindestens 32 px plus Abstand. In `ha_verlauf` überlappten
  sich Datumsachse und Fußzeile um 6 px, weil nur 10 px Abstand eingeplant waren.
  Vertikale Abstände vorher ausrechnen — am Bildschirm sieht man den Fehler erst
  auf dem Foto.

## Display-Treiber: Elecrow, nicht GxEPD2

GxEPD2 1.5.6 liegt in `libraries/`, ist für dieses Panel aber **unbrauchbar**: keine der
129 Display-Klassen deckt 272×792 ab (SSD1683 nur in 4,2"-Varianten mit 400×300).
Es zu nutzen hieße, eine Panel-Klasse inkl. Aufteilung auf zwei Controller neu zu schreiben.

Stattdessen: Elecrows `EPD.cpp`, `EPD_Init.cpp`, `spi.cpp`, `EPDfont.h` werden **in den
Sketch-Ordner kopiert** (nicht nach `libraries/`). Vorlage: `examples/5.79_Global_refresh`.

Die Vendor-Dateien möglichst **unverändert** lassen — Einstellungen gehören in die `.ino`,
sonst gehen sie beim nächsten Kopieren verloren.

## Sketch-Ordner anlegen

Steht in `sketches/CLAUDE.md`: Namensregel, Vendor-Dateien kopieren, `secrets.h`,
Fallstricke. Kurzfassung: Ordnername und `.ino` müssen **identisch** heißen, alle Dateien
im Ordner werden mitkompiliert, und `src/` als Ordnername ist ein Sonderfall (einziger
rekursiv kompilierter Unterordner). Im Sketchbook-Wurzelverzeichnis sind zusätzlich
`libraries/` und `hardware/` reserviert.

## Arduino-Eigenheiten

- **Eigene `struct`-Typen gehören in eine `.h`, nicht in die `.ino`.** Die Toolchain
  erzeugt automatisch Funktionsprototypen und setzt sie an den Dateianfang — *vor*
  selbst definierte Typen. Eine Funktion mit `struct Foo` als Parameter scheitert dann
  an `'Foo' has not been declared`. Aus einem Header ist der Typ rechtzeitig da.
  Beispiel: `sketches/ha_verlauf/series.h`.

## Home Assistant

Instanz: `http://192.168.178.83:8123` (HTTP, kein TLS — spart auf dem ESP32 die
gesamte Zertifikatsbehandlung). Zugangsdaten in `secrets.h` je Sketch, per
`.gitignore` ausgeschlossen; Vorlage ist `secrets.h.example`.

```c
GET /api/states/<entity_id>
Authorization: Bearer <langlebiger Zugriffstoken>
```

**Fallen, die alle schon zugeschlagen haben:**

- **Ohne `end_time` liefert `/api/history/period/<start>` genau EINEN Tag** ab `start`,
  nicht bis jetzt. Eine 14-Tage-Abfrage gibt sonst kommentarlos einen einzelnen Tag aus
  der Mitte zurück — sieht aus wie fehlende Daten, ist ein fehlender Parameter.
- **Zeitstempel als `Z` schreiben, nicht `+00:00`.** Das `+` wird im Query-String zum
  Leerzeichen dekodiert, HA antwortet mit `Invalid end_time`. Sieht nach kaputtem Datum
  aus, ist ein Kodierungsproblem.
- **Rohdaten reichen nur ~10 Tage zurück** (`purge_keep_days`). Längere Zeiträume liegen
  in der Langzeitstatistik (unbegrenzt, benötigt `state_class: measurement`) — die gibt
  es aber **nur über WebSocket** (`recorder/statistics_during_period`), nicht über REST.
- **`entity_id` und Anzeigename passen nicht zusammen.** Bei den SONOFF-Sensoren heißt
  `sensor.temperatur_sonoff_snzb_02d_temperatur` „Temperatur **Badezimmer**"; das
  Wohnzimmer ist `sensor.wohnzimmer_temperatur_sonoff_snzb_02d_temperatur`. Immer über
  `friendly_name` verifizieren, nie über die ID raten.
- **`unavailable` und `unknown` sind gültige Zustände**, keine Fehler. `atof()` macht
  daraus 0.0 — in einer Kurve reißt das den Verlauf auf 0 Grad.
- **Antworten nicht mit `Arduino_JSON` parsen, wenn sie groß sind.** 10 Tage History sind
  ~19 KB; die Bibliothek baut daraus einen Objektbaum mit einem Vielfachen an RAM-Bedarf.
  Stattdessen direkt mit `strstr()` nach `"state":"` und `"last_changed":"` scannen.

### MCP-Server

Ein Home-Assistant-MCP ist im **User-Scope** eingetragen (`~/.claude.json`, also außerhalb
des Repos und in allen Projekten verfügbar):

```bash
claude mcp add -s user home-assistant -- /opt/homebrew/bin/uvx \
  --with 'mcp<2.0.0' mcp-proxy --transport streamablehttp <interne-URL>
```

**Der MCP liefert Zeitstempel in lokaler Zeit, die REST-API in UTC.** Wer den Sketch
anhand der MCP-Ausgabe baut, bekommt eine um zwei Stunden verschobene Zeitachse.
Faustregel: MCP zum Stöbern, `curl` zum Verifizieren dessen, was der ESP32 sieht.

## Sketches

**Index samt Doku-Pflicht: `sketches/CLAUDE.md`.** Jeder Sketch hat dort eine Zeile und
daneben eine eigene `CLAUDE.md` in seinem Ordner — Bedienung, Konstanten und die Fehler,
die er schon gekostet hat. Der Betriebssketch ist `sketches/ha_wechsel`.

Fehler gehören **auf das Display**, nicht nur ins Log — sonst sieht man bei einem Problem
nur ein leeres Panel. `ha_temperatur` und `ha_verlauf` zeigen HTTP 401/404 mit einem
Hinweis auf die wahrscheinliche Ursache an.

## Struktur

```
sketches/          Eigene Sketches (secrets.h darin ist gitignored)
                   → `sketches/CLAUDE.md`: Index aller Sketches und die Doku-Pflicht
                   → je Sketch eine eigene `CLAUDE.md` neben der `.ino`
libraries/         Elecrow-Library-Bundle (zugleich Sketchbook-libraries/)
examples/          Offizielle Elecrow-Beispiele + Demos — Vorlagen zum Kopieren
factory_firmware/  Werksfirmware als Backup
material/          Handbuch + Datenblätter (PDF + .txt), Fotos (JPEG; *.HEIC ist gitignored)
                   → `material/CLAUDE.md` fasst die Datenblätter mit Seitenangaben zusammen
                   → `.txt` je PDF zum Durchsuchen, `make material-txt` erzeugt sie neu
                   → `*.yaml`: Kommando- und GPIO-Tabellen maschinenlesbar
features/TODO/     Ideen mit Substanz, die auf später warten — je eine `.md`
                   → was das Problem ist, was belegt ist, was noch zu messen wäre
tools/simulator/    Host-Build eines Sketches → PNG (`make sim`), ohne Gerät
                   → übersetzt alle `.cpp` des Sketch-Ordners (`SIM_SRCS`), nicht nur die `.ino`
Makefile
```

Fotos vom iPhone kommen als HEIC — auf GitHub nicht anzeigbar und als JPEG in voller
Auflösung sogar größer als das Original. Committet wird deshalb eine verkleinerte JPEG,
das HEIC bleibt lokal.

**`-strip` ist nicht optional.** iPhone-Fotos tragen GPS-Koordinaten mit ±5 m Genauigkeit
plus Zeitstempel im EXIF; ein Foto vom Schreibtisch verrät damit die Wohnadresse. `sips`
schleppt das beim Umwandeln mit. Deshalb `magick`, das HEIC direkt liest und in einem
Schritt verkleinert und säubert:

```bash
magick material/FOTO.HEIC -strip -resize 2000x2000 -quality 85 material/FOTO_2000px.jpg

# Gegenprobe: leere Ausgabe plus "unknown image property" = sauber
magick identify -format '%[EXIF:GPSLatitude]\n' material/FOTO_2000px.jpg
```

Vor dem Commit prüfen — ist ein Foto mit Koordinaten erst einmal in der Historie, hilft
nur noch Umschreiben.

## Geheimnisse

`secrets.h` liegt neben jeder `.ino`, die Zugangsdaten braucht, und ist per `.gitignore`
ausgeschlossen — committet wird nur `secrets.h.example` mit Platzhaltern. **Die
`.gitignore`-Regel muss stehen, bevor die Datei existiert**: Ist sie einmal committet,
hilft ein nachträgliches `.gitignore` nicht mehr, der Token steht dann dauerhaft in der
Historie. Vor jedem Commit prüfen:

```bash
git check-ignore -v sketches/<name>/secrets.h     # muss anschlagen
git diff --cached | grep -c 'eyJhbGciOi'          # nur der Platzhalter darf treffen
```

## Vorgehen, das sich bewährt hat

- **Erst per `curl` gegen die echte API testen, dann in C schreiben.** Beide HA-Fallen
  (fehlendes `end_time`, `+00:00` statt `Z`) sind so aufgefallen — auf dem Board hätten
  sie nach einem Fehler im Sketch ausgesehen.
- **Zahlen gegenprüfen.** Der handgeschriebene Parser auf dem ESP32 kam auf exakt dieselben
  287 Punkte, min 21.3 und max 27.7 wie Pythons `json`-Modul auf derselben Antwort.
- **Layout am realen Gerät kontrollieren.** Ein Foto des Panels hat eine 6-px-Überlappung
  gezeigt, die im SVG-Prototyp unsichtbar war, weil dort andere Schriftmetriken galten.
  Seitdem gibt es `make sim SKETCH=...`: Es übersetzt den Sketch nativ und schreibt sein
  Bild als PNG — mit dem **echten** `EPD.cpp` und den echten Fonts, ersetzt sind nur
  Arduino, WLAN und HTTP (`tools/simulator/`). Damit ist der Prototyp pixelgleich, und
  Layoutfehler fallen ohne Flashen auf. Vorher einmal `make sim-fetch SKETCH=...` für die
  Daten. Das Gerät bleibt trotzdem die letzte Instanz für Kontrast und Lesbarkeit.
- **Nie ungefragt committen.** Änderungen fertigstellen, dann fragen — Sascha schaut sich
  das Diff vorher an. Gilt auch, wenn die Änderung offensichtlich richtig ist und wenn
  vorher schon einmal committet werden durfte; die Erlaubnis gilt nur für den einen Commit.
- **Vor jedem Commit auf Geheimnisse prüfen** (siehe Abschnitt *Geheimnisse*).
- **Nie ungefragt flashen.** Vor jedem `make flash` fragen — auch beim Zurückflashen des
  vorherigen Sketches und auch, wenn vorher schon einmal geflasht werden durfte. Auf dem
  Board läuft ein produktiv genutzter Sketch; ein Flash überschreibt ihn sofort.
  Achtung: `make flash` **ohne** `SKETCH=` nimmt den Makefile-Default `hello_epaper`,
  nicht den zuletzt geflashten Sketch — zum Zurückflashen immer explizit
  `SKETCH=sketches/ha_verlauf` angeben.

## Stand

Auf dem Board liegt zuletzt `sketches/bedienleiste` — die drei Bedienelemente
als Laschen am linken Rand. Der Betriebssketch ist
`sketches/ha_wechsel` (Temperaturen und Wetter im Minutenwechsel, Daten alle
10 Minuten, Vollrefresh stündlich); zurück geht es mit
`make flash SKETCH=sketches/ha_wechsel`. Einzeln flashen lassen sich weiterhin
`sketches/ha_kacheln` und `sketches/ha_wetter`.

Alles committet, Arbeitsverzeichnis sauber.

**Erledigt:** Der Verdacht, der EXIT-Eingang könnte flattern, weil `INPUT` ohne
Pull-up einen offenen Pin liest, ist vom Tisch — **alle fünf Tasten haben
4,7-kΩ-Pull-ups auf der Platine** (`material/SCHALTPLAN.md` → *Tasten*).
`INPUT` ist damit richtig, `INPUT_PULLUP` würde nichts verbessern. Bleiben die
drei `EXIT: umgeblaettert` im Log der ersten Minute nach dem Flashen unerklärt;
falls sie wiederkommen, ist die Ursache eher die Entprellung als der Pegel.

Remote ist `git@github.com:rsascha/claude-arduino-display.git` und **privat — das bleibt
so.** Grund ist nicht der eigene Inhalt, sondern das mitgeführte Fremdmaterial: die
Datenblätter in `material/` (Solomon Systech, Espressif) und `factory_firmware/main.ino.bin`
(Elecrow) dürfen nicht weiterverbreitet werden. Soll das Repo je öffentlich werden, müssen
beide vorher raus und durch Links ersetzt werden; die Bezugsquellen stehen in `README.md`
unter *Quellen*.

## Offene Punkte

- Beispiele stammen aus der Core-2.x/3.0-Zeit, gebaut wird mit 3.3.11. `5.79_Global_refresh`
  und die eigenen Sketches kompilieren sauber; `5.79_BLE` ist ungetestet und dürfte wegen
  der geänderten BLE-API in Core 3.x Anpassungen brauchen.
- `ha_verlauf` zeigt 10 statt der gewünschten 14 Tage — mehr gibt HAs Recorder per REST
  nicht her. Für die vollen zwei Wochen bräuchte der ESP32 einen WebSocket-Client, oder
  `purge_keep_days` in HA müsste hochgesetzt werden (wirkt erst ab dann, nicht rückwirkend).
- Größere Zeichenflächen: Der Nutzen des 792 px breiten Panels ist bei einem einzelnen
  Messwert (`ha_temperatur`) kaum ausgeschöpft — mehrere Sensoren nebeneinander oder
  Wert plus Verlauf wären naheliegend.
- `ha_verlauf` fährt bei **jeder** Aktualisierung den vollen Löschzyklus. Nach dem
  Vergleich in `ha_umschalten` wäre `EPD_FastUpdate()` als Regelfall die bessere Wahl,
  mit einem Vollrefresh nur alle paar Durchgänge gegen Ghosting. Bewusst noch nicht
  umgestellt — Sascha wollte erst weitere Tests machen.
- Deep Sleep statt `delay()`: Die HA-Sketches halten WLAN dauerhaft aktiv. Für Batterie-
  betrieb wäre `esp_deep_sleep_start()` der richtige Weg — E-Paper hält sein Bild ohne
  Strom. Bisher nicht nötig, weil das Board am USB hängt. Der Akkuanschluss ist ein
  **SH1.0-2P (1,0 mm)**, kein 1,25 mm — Details und Polaritätswarnung in `README.md`
  → *Hardware* und `material/CLAUDE.md` → *Akkuanschluss*.
