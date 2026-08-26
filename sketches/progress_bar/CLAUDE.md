# progress_bar

Fortschrittsanzeige mit fünf Segmenten, alle 3 Sekunden eines mehr; EXIT startet den
Durchlauf neu. Kein WLAN.

```bash
make flash SKETCH=sketches/progress_bar
```

Der Gegentest zu `ha_umschalten`: Partial taugt nichts für einen Vollbildwechsel, wohl
aber hier, wo sich pro Schritt nur ein Segment ändert.

## Die zwei Bedingungen, ohne die Partial nicht funktioniert

**`EPD_Clear_R26A6H()` vor dem ersten `EPD_PartUpdate()`.** Der SSD1683 hat zwei RAMs:
`0x24`/`0xA4` das neue Bild, `0x26`/`0xA6` das vorherige. Partial wählt seine Waveform pro
Pixel aus dem Übergang *alt → neu* und braucht beide. `EPD_Display_Clear()` hinterlässt in
`0x26` aber `0x00`, und `EPD_Display()` schreibt **nur** `0x24`/`0xA4`. Ohne den Aufruf
rechnet das erste Partial-Update gegen einen falschen Ausgangszustand und treibt die Pixel
zu schwach — das erste Segment wird hellgrau statt schwarz. Ab dem zweiten Update führt
der Controller `0x26` selbst nach, deshalb fällt es nur beim ersten auf und sieht nach
einem Kontrastproblem des Panels aus.

**Kein Hardware-Reset zwischendurch.** `EPD_FastMode1Init()` enthält einen
`EPD_HW_RESET()`, und ein zurückgesetzter Controller kennt das vorherige Bild nicht mehr —
worauf Partial gerade aufbaut. Initialisiert wird deshalb nur **einmal je Durchlauf**
(`panelReset()`), danach nur noch `EPD_Display()` + `EPD_PartUpdate()`.

**Nicht verallgemeinern:** `EPD_Clear_R26A6H()` gehört vor den ersten *Teilrefresh* und
sonst nirgendwohin. Vor einem vollen oder schnellen Neuaufbau richtet er Schaden an — die
Kreuztabelle dazu steht in `../bedienleiste/CLAUDE.md`.

## Zeichendetails

`fillRect()` füllt selbst, weil `EPD_DrawRectangle()` im gefüllten Modus von `Ystart` bis
`Yend-1` zieht — die letzte Zeile fehlt sonst. `frameRect()` legt mehrere Rechtecke
ineinander, weil ein 1 px dünner Umriss auf dem Panel kaum zu erkennen ist.

Die Segmentbreite kommt aus einer Ganzzahldivision; der Rest wird auf beide Seiten
verteilt, sonst sitzt der Block sichtbar links vom Rahmenmittelpunkt.

## Ohne Gerät prüfen

```bash
make sim SKETCH=sketches/progress_bar
```

Der Simulator führt nur `setup()` aus — hier also das Bild vor dem ersten Schritt.

## Weiterführend

- `../../PROGRESS_BAR.md` — dasselbe Thema ausführlich
- `../bedienleiste/CLAUDE.md` — Partial mit Tastenbedienung, samt Refresh-Kreuztabelle
