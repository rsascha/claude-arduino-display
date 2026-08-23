# Datenblätter — das Wesentliche

Destillat der drei PDFs in diesem Ordner. **Maßgeblich sind die PDFs**, nicht diese Datei;
jede Aussage hier trägt deshalb eine Seitenangabe. Wo etwas erschlossen und nicht belegt
ist, steht das ausdrücklich dabei.

| Datei | Inhalt |
|---|---|
| `crowpanel-5.79-benutzerhandbuch.pdf` | 5 S., Elecrow, deutsch — Board, Bedienelemente, Spezifikation |
| `ssd1683-datasheet.pdf` | 49 S., Solomon Systech Rev 1.0 (Jan 2021) — Display-Controller |
| `esp32-s3-wroom-1-datasheet.pdf` | Espressif v1.2 (2023) — Funkmodul |

---

## 1. Benutzerhandbuch

**Abmessungen** (S. 2): 167,91 × 56,94 mm Außenmaß, aktive Fläche 139,00 × 47,74 mm.

**Bedienelemente** (S. 2 Abbildung, S. 3 Tabelle): An der Kante MENU, Rotary Switch
(Drehschalter) und EXIT; auf der Platine BOOT und RESET. Die Abbildung auf S. 2 zeigt die
**Rückseite mit USB-C unten** und dort zusätzlich PWR, TF-Kartenschacht, GPIO-Header,
BAT-Anschluss (SH1.0-2P) und das ESP32-S3-WROOM-1-N8R8-Modul.

**Achtung bei der Reihenfolge:** Weil die Abbildung die Rückseite zeigt, steht sie beim
Blick auf das Display mit USB oben und Elementen links kopf — dann ist von oben nach unten
**EXIT, Drehschalter, MENU** (am Gerät geprüft, GPIOs in §4).

**Spezifikation** (S. 3): 272 (H) × 792 (L) Pixel, Pixelabstand 0,1755 mm, Anzeigespannung
2,2–3,7 V, Betrieb 0–50 °C, Lagerung −25–70 °C, 4-Draht-SPI (3-Draht möglich).

Das Handbuch schreibt den Controller als „SSD16832" — Tippfehler, das mitgelieferte
Datenblatt ist der **SSD1683**.

**Keine GPIO-Angaben.** Das Handbuch nennt an keiner Stelle Pinnummern; alle GPIOs in
diesem Repo stammen aus dem Elecrow-Beispielcode. Auch die Links zu den beiden anderen
PDFs stehen nur als QR-Code (S. 4–5), nicht im Text.

---

## 2. SSD1683 — Display-Controller

### Warum `EPD_W` 800 ist, sichtbar aber 792

Ein SSD1683 treibt **400 Source × 300 Gate** (S. 5, §2). Das Panel ist 792 breit — passt
also nicht auf einen Chip. Der **Cascade Mode** (S. 21, §6.12) koppelt zwei Chips zu
800 × 300: Ein Pin `M/S#` bestimmt Master (an VDDIO) oder Slave (an VSS); beim Slave sind
Oszillator, Booster und Regler abgeschaltet und werden vom Master mitversorgt.

Daraus folgt die Puffergeometrie in `EPD_Init.h`:

```
Source_BYTES     = 400/8 = 50      // pro Chip
Gate_BITS        = 272             // genutzte Gates von 300 möglichen
ALLSCREEN_BYTES  = 13600           // pro Chip
Puffer gesamt    = 27200           // beide Chips
```

800 Sources, 792 sichtbar — die 8 Pixel dazwischen sind die Naht zwischen den Hälften.
`Paint_SetPixel()` überspringt sie mit `if (Xpoint >= 396) Xpoint += 8`.

### Master- und Slave-Register

Der Elecrow-Treiber spricht beide Chips über **eine** CS-Leitung an und unterscheidet sie
am Kommando-Byte: Slave-Register = Master-Register **+ 0x80**.

| Zweck | Master | Slave |
|---|---|---|
| RAM X-Adressbereich | `0x44` | `0xC4` |
| RAM Y-Adressbereich | `0x45` | `0xC5` |
| RAM-Adresszähler X/Y | `0x4E`/`0x4F` | `0xCE`/`0xCF` |
| RAM schreiben S/W | `0x24` | `0xA4` |
| RAM schreiben „Rot" | `0x26` | `0xA6` |

**Das steht so nicht im Datenblatt.** Es beschreibt einen einzelnen Chip; „Slave" kommt nur
in §6.12 und der Pinbeschreibung (S. 7, `M/S#`) vor, eine Registerverschiebung wird nirgends
erwähnt. Die Tabelle ist aus `examples/5.79_Global_refresh/EPD_Init.cpp` abgelesen
(`EPD_Display_Clear()`, Z. 195–236) und beim Betrieb des Panels bestätigt — aber eben
empirisch, nicht dokumentiert.

### Kommandos, die der Elecrow-Treiber benutzt

| Hex | Name im Datenblatt | S. | Verwendung im Treiber |
|---|---|---|---|
| `0x10` | Deep Sleep mode | 25 | `EPD_DeepSleep()`, Parameter `0x01` = Mode 1 |
| `0x11` | Data Entry mode setting | 25 | `0x05` = Y dekrementiert, X inkrementiert |
| `0x12` | SW RESET | 25 | in `EPD_Init()` nach dem Hardware-Reset |
| `0x18` | Temperature Sensor Control | 27 | `0x80` = interner Sensor |
| `0x1A` | Write to temperature register | 27 | `0x64, 0x00` |
| `0x20` | Master Activation | 28 | löst die mit `0x22` gewählte Sequenz aus |
| `0x22` | Display Update Control 2 | 29 | siehe unten |
| `0x24` | Write RAM (Black White) | 29 | Bit 1 = weiß, Bit 0 = schwarz |
| `0x26` | Write RAM (RED) | 30 | beim S/W-Panel nur zum Löschen beschrieben |
| `0x3C` | Border Waveform Control | 34 | `0x03` |
| `0x44`/`0x45` | Set RAM X-/Y-address | 34/35 | Adressbereich |
| `0x4E`/`0x4F` | Set RAM address counter | 36 | Adresszähler |

### Die drei Update-Modi

`0x22` wählt die Sequenz, `0x20` startet sie. Die dokumentierten Werte (S. 29):

| Wert | Bedeutung laut Datenblatt | Treiberfunktion |
|---|---|---|
| `0xF7` | Takt + Analog an, Temperatur laden, LUT laden (3-Farb-Modus), anzeigen | `EPD_Update()` |
| `0xC7` | Takt + Analog an, anzeigen (3-Farb-Modus) — **ohne** Temperatur/LUT | `EPD_FastUpdate()` |
| `0xDC` | **nicht in der Tabelle** | `EPD_PartUpdate()` |

Zwei Auffälligkeiten:

- Der Treiber nutzt durchgehend die **3-Farb-Varianten** (`F7`, `C7`), obwohl das Panel S/W
  ist. Für S/W wären `FF` und `CF` vorgesehen. Läuft trotzdem — vermutlich, weil die
  Waveforms im OTP des Panels entsprechend abgelegt sind.
- `0xDC` steht in keiner Zeile der Tabelle auf S. 29. Die Bits ergeben „Takt an, Analog an,
  anzeigen, Analog aus" ohne LUT-Nachladen, was zum Teilrefresh passt, ist aber
  Rückrechnung, keine Dokumentation.

### Deep Sleep — der Reset ist Pflicht

`0x10` mit `0x01` (Mode 1) oder `0x03` (Mode 2). Danach (S. 25, wörtlich):

> To Exit Deep Sleep mode, User required to send HWRESET to the driver

**BUSY bleibt im Deep Sleep dauerhaft HIGH.** Wer nach `EPD_DeepSleep()` ohne
`EPD_HW_RESET()` weiterschreibt, hängt in `EPD_READBUSY()` fest — das ist eine
`while(1)`-Schleife ohne Timeout (`EPD_Init.cpp:11`). Die Beispiele rufen `EPD_DeepSleep()`
am Ende jedes Updates auf, deshalb beginnt jeder Zyklus mit `EPD_GPIOInit()` und einem
Init, der den Hardware-Reset enthält.

### Sonstiges, das noch nützlich werden kann

- **Interner Temperatursensor**, ±2 °C von −25 bis 50 °C (S. 5, §2; S. 17–18, §6.8). Der
  Treiber aktiviert ihn (`0x18`/`0x80`), liest den Wert aber nie aus — er dient nur der
  Waveform-Auswahl. Über das Lese-Kommando wäre eine Gehäusetemperatur ohne Zusatzsensor
  zu haben.
- **Partieller Refresh wird vom Chip unterstützt** (S. 5, §2), ebenso „Auto write RAM" für
  regelmäßige Muster.
- **SPI bis 20 MHz Schreibtakt** (S. 5, §2; AC-Kennwerte S. 45, §12.1). Der Elecrow-Treiber
  bit-bangt stattdessen in Software (`spi.cpp`, „IO模拟SPI") und toggelt CS pro Byte — vom
  Limit also weit entfernt. Hardware-SPI wäre der offensichtliche Hebel, falls ein Update
  je zu langsam ist.
- **Versorgung** VCI 2,3–3,7 V (S. 5, §2) — deckt sich mit den 2,2–3,7 V im Handbuch.
- **Ablaufdiagramme** für Power-On/Off und für Deep Sleep nach dem Update: S. 41–42, §9.

---

## 3. ESP32-S3-WROOM-1-N8R8 — Modul

### Was N8R8 bedeutet

8 MB Flash (Quad-SPI) und 8 MB PSRAM (**Octal**-SPI), Betriebstemperatur −40 bis 65 °C
(S. 3, Varianten-Tabelle).

**Folge des Octal-PSRAM:** GPIO **35, 36 und 37 sind belegt** und stehen nicht zur
Verfügung (S. 12, Fußnote b zu Tabelle 3, wörtlich: „pins IO35, IO36, and IO37 connect to
the OSPI PSRAM and are not available for other uses"). Das ist auch der Grund, warum in
den Board-Einstellungen `PSRAM=opi` stehen muss.

Das Modul führt 41 Pins heraus (S. 10–12, Tabelle 3); GPIO 22–34 gibt es nicht.

### Strapping-Pins — hier lauert etwas

Beim Reset lesen vier Pins ihre Beschaltung aus (S. 13, §3.3 und Tabelle 4):

| Pin | steuert | Default |
|---|---|---|
| GPIO 0 | Boot-Modus | Pull-up (1) |
| GPIO 3 | JTAG-Signalquelle | floating |
| GPIO 45 | VDD_SPI-Spannung | Pull-down (0) |
| GPIO 46 | Boot-Modus, ROM-Ausgaben | Pull-down (0) |

Dieses Board benutzt **GPIO 45 als Display-CS und GPIO 46 als Display-DC** — beides
Strapping-Pins. Im Betrieb ist das unkritisch (nach dem Reset sind es normale IOs), aber:
Wer eine externe Beschaltung an diese Leitungen hängt, die beim Einschalten zieht, ändert
damit den Boot-Modus. Dasselbe gilt für **GPIO 3 am GPIO-Header**.

### Stromaufnahme — relevant für den Deep-Sleep-Punkt

| Zustand | Typ. | Quelle |
|---|---|---|
| Aktiv, WLAN sendet (802.11b @20,5 dBm) | 355 mA Spitze | S. 17, Tabelle 12 |
| Aktiv, WLAN empfängt | 95 mA | S. 17, Tabelle 12 |
| Modem-Sleep, 240 MHz, beide Kerne idle | 33–48 mA | S. 18, Tabelle 13 |
| Light-Sleep | 240 µA | S. 18, Tabelle 14 |
| **Deep-Sleep** (RTC-Speicher an) | **7–8 µA** | S. 18, Tabelle 14 |

Der Sprung von Modem-Sleep auf Deep-Sleep ist rund Faktor 5000. Für `ha_verlauf` mit
30-Minuten-Takt hieße das: aufwachen, WLAN an, holen, zeichnen, schlafen. Das E-Paper hält
sein Bild stromlos. Versorgung laut Datenblatt 3,0–3,6 V, das Netzteil muss 0,5 A liefern
können (S. 16, Tabelle 10).

---

## 4. GPIO-Gesamtbild dieses Boards

Zusammengetragen aus den Elecrow-Beispielen, nicht aus den Datenblättern — die kennen das
Board nicht, nur das Modul.

| GPIO | Funktion | Beleg |
|---|---|---|
| 0 | BOOT-Taster (Strapping) | Handbuch S. 2 |
| 1 | EXIT | `5.79_key.ino:9` |
| 2 | MENU | `5.79_key.ino:8` |
| 4, 5, 6 | Drehschalter: 4 = hoch, 5 = Druck, 6 = runter | am Gerät geprüft, 23.08.2026 |
| 3, 8, 9, 14, 15, 16, 17, 18, 19, 20, 21, 38 | GPIO-Header, 12 Pins | `5.79_GPIO.ino:49` |
| 7 | Display-Spannung, muss HIGH | alle Beispiele |
| 10, 13, 39, 40 | TF-Karte: CS, MISO, SCK, MOSI | `5.79_TF.ino:6-9` |
| 11, 12 | E-Paper MOSI, SCK | `spi.h:7-8` |
| 35, 36, 37 | Octal-PSRAM, gesperrt | WROOM-1 S. 12 |
| 41 | Power-LED | `5.79_PWR.ino:22` |
| 42 | zweite Versorgungsfreigabe, nötig für die TF-Karte | `5.79_TF.ino:26` |
| 43, 44 | UART0 zum CH340 | WROOM-1 S. 12 |
| 45, 46 | E-Paper CS, DC (**Strapping**) | `spi.h:10-11` |
| 47, 48 | E-Paper RES, BUSY | `spi.h:9,12` |

**Frei ist praktisch nur der Header.** Alles außerhalb der zwölf Header-Pins ist belegt.
GPIO 19 und 20 sind am Modul zwar USB D−/D+, hier aber am Header — die USB-Buchse hängt
am CH340, nicht am nativen USB des ESP32.

---

## 5. Was in keinem der drei PDFs steht

- Die **GPIO-Zuordnung des Boards** — kommt ausschließlich aus dem Beispielcode
  (die fünf Tasten-GPIOs sind inzwischen am Gerät bestätigt, §4).
- Die **+0x80-Verschiebung der Slave-Register** (siehe §2).
- Der Wert **`0xDC`** für den Teilrefresh (siehe §2).
- Die **Waveform-Tabellen des konkreten Panels**. Sie liegen im OTP des Displays; das
  Datenblatt beschreibt nur Format und Suchmechanismus (S. 16–20, §6.7–6.11).
- Ob der Drehschalter ein **Encoder mit Rastung** ist oder drei getrennte Kontakte. Aus
  `5.79_key` lässt sich beides lesen. Die drei GPIOs sind am 23.08.2026 verifiziert
  (4 = hoch, 5 = Druck, 6 = runter, siehe §4) — die *Bauart* dahinter aber nicht: dass
  jede Drehrichtung genau einen Pin auslöst, schließt einen Quadratur-Encoder nicht aus,
  dessen zwei Leitungen der Beispielcode nur einzeln abfragt.
