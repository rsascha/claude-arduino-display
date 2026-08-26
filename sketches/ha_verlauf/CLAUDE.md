# ha_verlauf

Temperaturkurve über 10 Tage mit Gitter, Datums- und Gradachse. Aktualisierung alle
30 Minuten.

```bash
cp secrets.h.example secrets.h    # einmalig, Token eintragen
make flash SKETCH=sketches/ha_verlauf
```

## Warum 10 Tage und nicht 14

HAs Recorder hält standardmäßig `purge_keep_days = 10` vor. Längere Zeiträume liegen in
der Langzeitstatistik — die gibt es aber **nur über WebSocket**
(`recorder/statistics_during_period`), nicht über REST. Für zwei Wochen bräuchte der ESP32
also einen WebSocket-Client, oder `purge_keep_days` müsste in HA hochgesetzt werden (wirkt
erst ab dann, nicht rückwirkend).

## Die zwei API-Fallen, die hier zugeschlagen haben

**Ohne `end_time` liefert `/api/history/period/<start>` genau EINEN Tag** ab `start` —
nicht bis jetzt. Eine 10-Tage-Abfrage gibt sonst kommentarlos einen einzelnen Tag aus der
Mitte zurück; das sieht aus wie fehlende Daten, ist aber ein fehlender Parameter.

**Zeitstempel als `Z` schreiben, nicht `+00:00`.** Das `+` wird im Query-String zum
Leerzeichen dekodiert, HA antwortet mit `Invalid end_time`.

## Speicher: ein Wert je Pixelspalte

Die Antwort ist ~19 KB. Sie wird **nicht** mit `Arduino_JSON` geparst — die Bibliothek
baut daraus einen Objektbaum mit einem Vielfachen an RAM-Bedarf. Stattdessen scannt der
Sketch direkt mit `strstr()` nach `"state":"` und `"last_changed":"`.

Gehalten wird ein Wert je **Pixelspalte** (`NCOL = PLOT_X1 - PLOT_X0`, 710). Damit ist der
Speicherbedarf konstant, egal wie viele Messpunkte zurückkommen. Der Puffertyp steckt in
`series.h` — eigene `struct`-Typen gehören in einen Header, nicht in die `.ino`: Die
Toolchain setzt automatisch erzeugte Prototypen **vor** selbst definierte Typen, und eine
Funktion mit `struct Foo` als Parameter scheitert dann an `'Foo' has not been declared`.

## Layout

`PLOT_X0/X1 = 62/772`, `PLOT_Y0/Y1 = 46/222`. Unterhalb der Zeichenfläche liegen
Datumsachse und Fußzeile. **Hier hat sich schon einmal etwas überlappt:** Eine Textzeile
braucht `size` Pixel Höhe, nicht weniger; mit 10 px Abstand zwischen zwei 16er-Zeilen
überlagerten sich Achse und Fußzeile um 6 px. Am Bildschirm fiel das nicht auf, erst auf
dem Foto des Panels.

## Offen

Der Sketch fährt bei **jeder** Aktualisierung den vollen Löschzyklus. Nach dem Vergleich
in `ha_umschalten` wäre `EPD_FastUpdate()` als Regelfall besser, mit Vollrefresh nur alle
paar Durchgänge — so machen es `ha_kacheln` und `ha_wechsel`. Bewusst noch nicht
umgestellt.

## Ohne Gerät prüfen

```bash
make sim-fetch SKETCH=sketches/ha_verlauf   # echte Antworten nach tools/simulator/data/
make sim       SKETCH=sketches/ha_verlauf   # -> tools/simulator/out.png
```

## Weiterführend

- `../../CLAUDE.md` → *Home Assistant*
- `../ha_raeume/CLAUDE.md` — dieselbe Kurve, aber sechsfach
