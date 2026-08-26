# ha_umschalten

**Testsketch, kein Betriebszustand.** Blendet im 5-Sekunden-Takt zwischen Wohnzimmer- und
Schlafzimmerkurve um und wechselt dabei reihum Voll-, Fast- und Partial-Refresh durch.
Modus und gemessene Dauer stehen unten rechts auf dem Display.

```bash
cp secrets.h.example secrets.h
make flash SKETCH=sketches/ha_umschalten
```

> **E-Paper verträgt keine unbegrenzte Zahl an Refresh-Zyklen.** Sekundentakt ist als Test
> in Ordnung, als Dauerbetrieb nicht. Nach dem Vergleich wieder etwas anderes flashen.

## Das Ergebnis, für das es ihn gibt

- **`EPD_FastUpdate()`** ist schnell und sauber — die richtige Wahl für einen Bildwechsel.
  Seitdem der Regelfall in `ha_kacheln`, `ha_wetter` und `ha_wechsel`.
- **Vollrefresh** *mit* `EPD_Display_Clear()` davor lässt das Panel **mehrfach** komplett
  schwarz werden und dauert spürbar. Er verhindert Ghosting über viele Durchgänge — als
  Regelfall ist er zu auffällig.
- **`EPD_PartUpdate()`** taugt für einen Vollbildwechsel **nicht**: `EPD_Display()`
  schreibt immer das ganze RAM beider Controller. Partial lohnt erst für kleine
  Ausschnitte — Vorbild `progress_bar`.

Die drei Modi unterscheiden sich nur im Parameter zu `0x22`: `0xF7` / `0xC7` / `0xDC`.

## Aufbau

`SENSORS[]` (zwei Sensoren), `MODES[]` (die drei Verfahren), `HOLD_MS = 5000`,
`SWITCHES_PER_MODE = 4`. Beide Verläufe werden **einmal** geholt und im Speicher gehalten;
das Umschalten löst keine HTTP-Anfrage aus — sonst würde der Test die Netzwerklatenz
messen statt das Panel. Aufgefrischt wird alle 30 Minuten.

Die angezeigte Dauer ist die des **vorherigen** Wechsels: Das aktuelle Bild ist zum
Zeitpunkt des Zeichnens noch nicht geschrieben.

## Weiterführend

- `../../PROGRESS_BAR.md` — Partial-Refresh richtig genutzt
- `../../CLAUDE.md` → *Harte Fakten* — die Refresh-Modi als Kurzfassung
