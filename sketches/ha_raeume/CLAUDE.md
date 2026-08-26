# ha_raeume

Sechs Temperaturkurven in **einem** Diagramm: Wohnzimmer, Schlafzimmer, Badezimmer, Küche,
Flur und Außen. Aufbau wie `ha_verlauf`, die Unterschiede kommen alle aus der
Mehrfachdarstellung.

```bash
cp secrets.h.example secrets.h
make flash SKETCH=sketches/ha_raeume
```

## Keine Farben — also Beschriftung am Kurvenende

Sechs Strichmuster waren der erste Versuch und wirkten auf dem Panel unruhig. Jetzt sind
alle Kurven durchgezogen und werden **rechts neben ihrem Endpunkt** beschriftet
(`LABEL_X = PLOT_X1 + 18`, dahinter 128 px Platz).

Weil die Kurven dort oft nur wenige Pixel auseinanderliegen, weichen kollidierende
Beschriftungen nach unten aus, und eine **Führungslinie** verbindet jede mit ihrem
tatsächlichen Kurvenende. Ohne die ginge durch das Verschieben genau die Zuordnung
verloren, um die es geht. Der Preis: Wo sich zwei Kurven kreuzen, ist nicht mehr zu sagen,
welche welche ist.

## Gemeinsame y-Achse, absichtlich

Außen erreicht in der Sonne knapp 40 °C, innen liegt alles zwischen 20 und 28. Die Achse
umfasst deshalb rund 20 K, und die fünf Innenkurven drängen sich im unteren Drittel. Eine
gekappte Außenkurve würde zeigen, *dass* es draußen wärmer war, aber nicht wie warm — und
genau dieser Kontrast ist der interessante Teil.

## Ausfall einzelner Sensoren

Fällt einer aus, werden die übrigen gezeichnet und der fehlende in der Legende mit `n/a`
markiert. Ein Fehlerbild gibt es nur, wenn **alle sechs** ausfallen — anders als
`ha_verlauf`, wo eine fehlende Antwort das ganze Bild kostet.

## Wo was steht

- `rooms.h` — Raumliste (`ROOMS[]`: `entity_id` und Anzeigename)
- `series.h` — der Kurvenpuffer, ein Wert je Pixelspalte

Beide sind Header, weil eigene `struct`-Typen nicht in die `.ino` gehören.

## Ohne Gerät prüfen

```bash
make sim-fetch SKETCH=sketches/ha_raeume
make sim       SKETCH=sketches/ha_raeume
```

## Weiterführend

- `../ha_verlauf/CLAUDE.md` — die Einzelkurve, samt der API-Fallen
- `../ha_kacheln/CLAUDE.md` — dieselben Sensoren ohne Verlauf
