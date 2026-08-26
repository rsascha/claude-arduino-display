# ha_wetter

Vier Spalten: Kompassrose mit Pfeil, Windgeschwindigkeit, Luftdruck mit Drei-Stunden-
Tendenz, Wetter-Icon. Der Sketch, an dem am meisten selbst gezeichnet wird.

```bash
cp secrets.h.example secrets.h
make flash SKETCH=sketches/ha_wetter
```

## Der Pfeil zeigt, WOHER der Wind kommt

Home Assistant liefert die Windrichtung nach meteorologischer Konvention als **Herkunft**:
341° heißt „aus NNW". Ein kartenüblicher Pfeil in Wehrichtung zeigte damit nach SSO,
während daneben „NNW" steht — Bild und Text sähen aus, als widersprächen sie sich. Deshalb
zeigt der Pfeil auf die NNW-Marke der Rose.

## Luftdruck vom Solarnode, nicht aus der Vorhersage

Beide Quellen sind da und 16 hPa auseinander: `weather.forecast_home` meldet 1024 hPa auf
Meereshöhe reduziert, der Solarnode misst 1008 hPa vor Ort. Für die Aussage ist das ohne
Belang — beim Luftdruck zählt die Tendenz, und eine Differenz ist höhenunabhängig. Die
`TREND_HOURS = 3` sind nicht willkürlich, sondern der meteorologische Standardzeitraum für
die Drucktendenz.

## Icons als Umriss über den Umweg Weiß

Eine flächig schwarze Wolke wäre auf E-Paper ein Klecks. Ein Umriss aus einzelnen Bögen
scheitert daran, dass sich die Bögen der drei Wolkenbäuche gegenseitig durchschneiden.

Der Ausweg: die ganze Form **zweimal** zeichnen — erst 2 px größer in Schwarz, dann in
Weiß darüber. Übrig bleibt ein sauberer Umriss, und weil die weiße Füllung alles darunter
löscht, verdeckt die Wolke beim Icon *heiter* automatisch die Sonnenstrahlen hinter ihr.

## Fünf Icons plus Mond

Home Assistant kennt 15 Wetterzustände. Mit nur Sonne und Regen wären `cloudy` und `fog`
als Sonnentag durchgegangen. Die Zuordnung steht als Tabelle in `CONDITIONS`; was dort
nicht steht, wird zur Wolke mit dem Rohzustand als Text — besser ein unbekanntes Wort als
ein falsches Bild.

Getestet sind alle sechs Bilder, indem dem Simulator nacheinander jeder Zustand
untergeschoben wurde. Auf Schnee oder Mond hätte man sonst bis zum Winter oder bis zur
Nacht warten müssen.

## Refresh

Alle 10 Minuten `EPD_FastUpdate()`, jeder sechste Durchgang mit Löschzyklus.

## Ohne Gerät prüfen

```bash
make sim-fetch SKETCH=sketches/ha_wetter
make sim       SKETCH=sketches/ha_wetter
```

`fetch.sh` zieht die `entity_id`s aus der `.ino` und holt auch `weather.forecast_home`.
`#include`-Zeilen filtert es vorher heraus — `#include "weather.h"` sähe für das Muster
sonst aus wie die entity_id `weather.h`, und HA antwortet darauf mit 404.

## Weiterführend

- `../ha_wechsel/CLAUDE.md` — diese Seite im Wechsel mit den Temperaturen
