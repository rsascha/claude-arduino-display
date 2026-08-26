# hello_epaper

Der Einstiegssketch: Text in mehreren Größen, ein Rahmen, ein paar Formen. Kein WLAN,
keine `secrets.h`. Wer das Board neu aufsetzt, flasht das hier zuerst — läuft es, stimmen
Toolchain, Verkabelung und Panel-Spannung.

```bash
make flash SKETCH=sketches/hello_epaper
```

## Was man an ihm lernt

**`digitalWrite(7, HIGH)` in `setup()` ist Pflicht.** GPIO 7 schaltet die Masse des Panels.
Fehlt die Zeile, bleibt das Display dunkel — unabhängig davon, ob der restliche Code
stimmt. Das ist die häufigste Ursache für „geht nicht" bei einem neuen Sketch.

**Der Puffer ist 27200 Byte groß, nicht 792 × 272 / 8.** `EPD_W` ist 800: zwei
SSD1683-Controller mit je einer Hälfte, dazwischen 8 ungenutzte Spalten.
`Paint_SetPixel()` überspringt sie selbst (`if (Xpoint >= 396) Xpoint += 8`), gültige
x-Werte sind also 0..791.

**`DISPLAY_ROTATION = 0` steht im Sketch, nicht in `EPD.h`.** Die Vendor-Datei liefert
`Rotation 180` — bei USB oben steht das Bild damit auf dem Kopf. Der Wert gehört in die
`.ino`, damit die Vendor-Dateien beim nächsten Kopieren unverändert bleiben.

**Nur die Schriftgrößen 12, 16, 24, 48.** Auch 8 zeichnet nichts: `EPD_ShowChar()` kennt
dafür keinen Font-Zweig und steigt per `else return;` aus. Zeichenbreite ist `size/2`,
`EPD_ShowString()` bricht **nicht** um — die Zeilenlänge muss selbst passen.

**Nur ASCII 32..126.** Umlaute und `°` lesen über das Font-Array hinaus. Wer ein Grad-
Zeichen braucht, zeichnet es selbst (Vorbild: `ha_temperatur`).

## Ablauf

`EPD_FastMode1Init()` → `EPD_Display_Clear()` → `EPD_Update()` löscht das Panel physisch;
E-Paper hält sein Bild ohne Strom, das Alte muss aktiv weg. Danach **erneut**
`EPD_GPIOInit()` + `EPD_FastMode1Init()` — so machen es auch die Elecrow-Beispiele.
Gezeichnet wird in den Puffer, `EPD_Display()` überträgt ihn, `EPD_FastUpdate()` macht ihn
sichtbar, `EPD_DeepSleep()` legt das Panel schlafen.

## Weiterführend

- `../CLAUDE.md` — Doku-Pflicht und Sketch-Index
- `../../CLAUDE.md` → *Harte Fakten* — Zeichen-API, Refresh-Modi, Board
