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

## Struktur

```
sketches/          Eigene Sketches
libraries/         Elecrow-Library-Bundle (zugleich Sketchbook-libraries/)
examples/          Offizielle Elecrow-Beispiele + Demos — Vorlagen zum Kopieren
factory_firmware/  Werksfirmware als Backup
Makefile
```

## Offene Punkte

- Beispiele stammen aus der Core-2.x/3.0-Zeit, gebaut wird mit 3.3.11. `5.79_Global_refresh`
  und `sketches/hello_epaper` kompilieren sauber; `5.79_BLE` ist ungetestet und dürfte
  wegen der geänderten BLE-API in Core 3.x Anpassungen brauchen.
