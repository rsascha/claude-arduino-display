# CLAUDE.md

Arduino-Projekt für das **Elecrow CrowPanel ESP32 5.79" E-Paper HMI Display**
(272×792, S/W, ESP32-S3-WROOM-1-N8R8, 2× SSD1683 über SPI).

Ausführliche Doku inkl. Quellen: `README.md`.

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
| MAC | `1c:db:d4:54:e1:c8` |

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
- **Schriftgröße 16 ist auf dem Panel gut lesbar** — für Achsenbeschriftungen reicht sie,
  24 ist dafür nicht nötig (am realen Gerät geprüft).
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

Ordnername und `.ino` müssen **identisch** heißen:

```
sketches/mein_sketch/mein_sketch.ino
```

Alle weiteren Dateien im Ordner werden automatisch mitkompiliert. `src/` als Ordnername
vermeiden: Arduino behandelt ein `src/` *innerhalb* eines Sketches als Sonderfall
(einziger rekursiv kompilierter Unterordner). Im Sketchbook-Wurzelverzeichnis sind
zusätzlich `libraries/` und `hardware/` reserviert.

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

| Sketch | Inhalt |
|---|---|
| `hello_epaper` | Text, Formen, Grundlayout — der Einstieg |
| `smiley` | eigene Bogen-Funktion, `safePixel()`, Kreise mit Strichstärke |
| `ha_temperatur` | ein HA-Sensorwert groß auf dem Display |
| `ha_verlauf` | Temperaturkurve über 10 Tage mit Gitter und Achsen |

Fehler gehören **auf das Display**, nicht nur ins Log — sonst sieht man bei einem Problem
nur ein leeres Panel. `ha_temperatur` und `ha_verlauf` zeigen HTTP 401/404 mit einem
Hinweis auf die wahrscheinliche Ursache an.

## Struktur

```
sketches/          Eigene Sketches (secrets.h darin ist gitignored)
libraries/         Elecrow-Library-Bundle (zugleich Sketchbook-libraries/)
examples/          Offizielle Elecrow-Beispiele + Demos — Vorlagen zum Kopieren
factory_firmware/  Werksfirmware als Backup
Makefile
```

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

## Offene Punkte

- Beispiele stammen aus der Core-2.x/3.0-Zeit, gebaut wird mit 3.3.11. `5.79_Global_refresh`
  und die eigenen Sketches kompilieren sauber; `5.79_BLE` ist ungetestet und dürfte wegen
  der geänderten BLE-API in Core 3.x Anpassungen brauchen.
- `ha_verlauf` zeigt 10 statt der gewünschten 14 Tage — mehr gibt HAs Recorder per REST
  nicht her. Für die vollen zwei Wochen bräuchte der ESP32 einen WebSocket-Client, oder
  `purge_keep_days` in HA müsste hochgesetzt werden (wirkt erst ab dann, nicht rückwirkend).
