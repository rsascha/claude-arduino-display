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
sketches/         Eigene Sketches (hello_epaper, ...)
libraries/        Elecrow-Libraries — zugleich Sketchbook-libraries/
examples/         Offizielle Elecrow-Beispiele + Demos
factory_firmware/ Werksfirmware als Backup
material/         Handbuch + Datenblätter (PDF), Fotos des laufenden Panels
Makefile          Build-/Flash-Targets
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

## Beispiele

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

## Lizenz

Der eigene Code und die Dokumentation stehen unter MIT (siehe `LICENSE`).

`libraries/`, `examples/`, `factory_firmware/` und die PDFs in `material/` sind
Fremdmaterial und behalten die Lizenz ihrer jeweiligen Rechteinhaber — darunter
GPL-3.0 (GxEPD2) und AGPL-3.0 (EPaperDrive). Die Aufstellung steht in `LICENSE`.

Fotos in `material/` werden **ohne EXIF** committet. iPhone-Aufnahmen tragen sonst
GPS-Koordinaten mit ±5 m Genauigkeit; der Umwandlungsbefehl in `CLAUDE.md` entfernt
sie mit `-strip`.
