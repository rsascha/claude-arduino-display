# Fortschrittsanzeige mit Partial-Refresh

Beispiel-Sketch: **[`sketches/progress_bar/progress_bar.ino`](sketches/progress_bar/progress_bar.ino)**

Fünf Segmente, groß und zentriert. Alle 3 Sekunden kommt eines dazu; nach dem fünften
ist Schluss, und ein Druck auf **EXIT** startet den Durchlauf neu — auch mitten drin.
Der Sketch braucht kein WLAN und keine `secrets.h`.

```
              Schritt 3 von 5

   +-------------------------------------+
   |  #####  #####  #####                |
   |  #####  #####  #####                |
   +-------------------------------------+

                   60 %
```

Der eigentliche Zweck ist aber ein anderer: Es ist der Anwendungsfall, für den
`EPD_PartUpdate()` gedacht ist. Der Vergleich in `sketches/ha_umschalten` hatte gezeigt,
dass Partial für einen Vollbildwechsel nichts taugt. Hier ändert sich pro Schritt nur ein
Segment — und genau dafür ist der Modus da.

## Die zwei Dinge, ohne die Partial nicht funktioniert

### 1. `EPD_Clear_R26A6H()` vor dem ersten Partial-Update

**Das Symptom:** Das erste Segment wird hellgrau statt schwarz. Die Segmente 2 bis 5 sind
einwandfrei. Sieht nach einem Kontrastproblem des Panels aus, ist aber ein falsch
initialisiertes RAM.

**Die Ursache:** Der SSD1683 hat zwei RAM-Bereiche. `0x24`/`0xA4` hält das neue Bild,
`0x26`/`0xA6` das vorherige. Der Partial-Modus wählt seine Waveform **pro Pixel aus dem
Übergang altes Bild → neues Bild** — er braucht also beide.

Entscheidend ist, was die Treiberfunktionen dort hineinschreiben:

| Funktion | RAM `0x24` (Schwarz/Weiß) | RAM `0x26` („RED") |
|---|---|---|
| `EPD_Display_Clear()` | `0xFF` (weiß) | **`0x00`** |
| `EPD_Clear_R26A6H()` | — | **`0xFF`** (weiß) |
| `EPD_Display()` | Bilddaten | **wird nie beschrieben** |

Nach einem Löschzyklus steht in `0x26` also überall `0x00`, und `EPD_Display()` korrigiert
das nie. Das erste Partial-Update rechnet damit gegen einen falschen Ausgangszustand und
treibt die Pixel zu schwach. Ab dem zweiten Update führt der Controller `0x26` selbst nach
— deshalb fällt der Fehler nur beim ersten Segment auf.

**Die Lösung** ist ein Aufruf direkt nach dem Löschzyklus:

```c
EPD_Display_Clear();
EPD_Update();
EPD_Clear_R26A6H();     // 0x26/0xA6 auf 0xFF = "vorher weiss"
```

Elecrows eigenes `examples/5.79_key` macht das in Zeile 36–38 genauso. Im Treiber steht
kein Kommentar dazu, und das Datenblatt beschreibt den Zusammenhang nicht — der Aufruf
sieht deshalb aus wie eine überflüssige Zeile, die man beim Abschreiben weglässt.

### 2. Kein Hardware-Reset zwischen den Schritten

`EPD_FastMode1Init()` enthält einen `EPD_HW_RESET()`. Ein zurückgesetzter Controller kennt
das vorherige Bild nicht mehr — worauf der Partial-Modus gerade aufbaut. Initialisiert wird
deshalb nur **einmal je Durchlauf**:

```c
panelReset();              // GPIOInit + FastMode1Init + Loeschzyklus + Clear_R26A6H
renderBar(0);
EPD_Display(ImageBW);
EPD_FastUpdate();          // Startbild

for (int done = 1; done <= STEPS; done++) {
  renderBar(done);
  EPD_Display(ImageBW);
  EPD_PartUpdate();        // kein Reset dazwischen
}
```

Das ist der Unterschied zu `ha_umschalten`, das vor jedem Bild neu initialisiert, um die
drei Modi unter gleichen Bedingungen zu vergleichen.

## Layout

Bei 792 × 272 Pixeln, von oben:

| Element | y | Größe |
|---|---|---|
| Titel `Schritt N von 5` | 30–54 | 24 |
| Balken (Rahmen 3 px stark) | 78–188 | 600 × 110 px, x 96–696 |
| Prozentzeile | 212–236 | 24 |
| Außenrahmen | 269 | |

Eine Textzeile braucht `size` Pixel Höhe, nicht weniger — in `ha_verlauf` hatten sich zwei
Zeilen um 6 px überlappt, weil das zu knapp gerechnet war. Vertikale Abstände vorher
ausrechnen; am Bildschirm sieht man den Fehler erst auf dem Foto.

Zwei Abweichungen von der Zeichen-API, die den Aufwand wert sind:

- **`EPD_DrawRectangle()` zieht im gefüllten Modus nur bis `Yend-1`** — die letzte Zeile
  fehlt. Die Segmente füllt der Sketch deshalb selbst über `safePixel()`.
- **Ein 1-px-Umriss ist auf dem Panel kaum zu sehen.** Der Balkenrahmen besteht aus drei
  ineinandergelegten Rechtecken.

Der Balken läuft über beide Controller-Hälften. Das ist unkritisch: `EPD_DrawRectangle()`
zeichnet über `Paint_SetPixel()`, und das überspringt die 8 Pixel breite Naht bei x = 396
von selbst.

## EXIT-Taste

GPIO 1, gegen Masse, `pinMode(PIN_EXIT, INPUT)`, gedrückt ist **LOW**. Bei der Ausrichtung
Querformat / USB oben / Bedienelemente links ist EXIT der **obere** der beiden Taster —
am Gerät geprüft, siehe `README.md` → *Bedienelemente*.

`exitPressed()` entprellt beide Flanken und wartet die Freigabe ab, damit ein Druck nicht
mehrfach zählt. `waitOrExit()` wartet die Schrittdauer ab, bricht dafür aber sofort ab,
wenn EXIT kommt — sonst reagierte die Taste am Ende eines Schritts träge.

## Was als Nächstes ginge

`EPD_Display()` schreibt immer das **komplette** RAM beider Controller; ein Fenster kennt
die Funktion nicht. Bei jedem Schritt werden Titel, Rahmen und Prozentzeile also
mitgeschrieben, obwohl sich nur ein Segment ändert.

Wirklich partiell würde es erst, wenn man vorher den RAM-Adressbereich über `0x44`/`0x45`
(Datenblatt S. 34–35) auf die Zeilen des Balkens einengt. Ein **Y**-Fenster ist dabei
deutlich einfacher als ein X-Fenster: Beide Chips teilen sich dieselben Gate-Zeilen,
während eine Begrenzung in x über die Master/Slave-Aufteilung hinweg gedacht werden müsste.

Bisher nicht nötig — das Ergebnis sieht am Gerät sauber aus.
