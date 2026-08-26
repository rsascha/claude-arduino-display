# ha_kacheln

Sechs Räume als Kacheln im 3×2-Raster, sortiert von warm nach kalt in Lesereihenfolge —
oben links der wärmste Raum, unten rechts der kälteste. Die Rangfolge steckt damit in der
Anordnung und braucht keine eigene Beschriftung.

```bash
cp secrets.h.example secrets.h
make flash SKETCH=sketches/ha_kacheln
```

## Der Trend

**`TREND_HOURS = 3`.** Die SONOFF-Sensoren lösen 0,1 K auf. Über eine Stunde bewegt sich
ein geschlossener Raum oft nur um genau diesen einen Schritt — der Pfeil zeigte dann
Rauschen an. Über drei Stunden ist ein geöffnetes Fenster deutlich zu sehen, der Tagesgang
draußen aber noch nicht beherrschend.

**`TREND_FLAT = 0.2f`.** Darunter gilt es als unverändert und es gibt einen waagerechten
Strich statt eines Pfeils.

**Eine Abfrage statt zwei.** Aktueller Wert und Vergleich kommen aus derselben
History-Antwort: letzter Messwert = jetzt, letzter Messwert **vor** dem Referenzzeitpunkt
= Vergleich. Gesucht wird bewusst der letzte davor und nicht der erste der Antwort — nur
so stimmt der Vergleich auch, wenn ein längerer Zeitraum vorgesetzt wird, wie es der
Simulator mit seinen 10-Tage-Dateien tut.

**Die Trendzahl steht ohne Einheit da.** Korrekt wäre Kelvin, weil es eine Differenz ist —
aber ein „K" hinter der Zahl fragt auf einem Wohnzimmer-Display mehr, als es beantwortet.

## Zahlen auf Deutsch — und warum das Komma Ärger macht

`snprintf()` schreibt immer einen Punkt, die Locale-Umschaltung gibt es in der
Arduino-Laufzeit nicht. Das Komma wird deshalb nachträglich gesetzt — und dabei fällt auf,
dass der Font **dickte-gleich** ist: Das Komma bekommt dieselbe Zellenbreite wie eine
Ziffer, seine Tinte belegt davon aber nur die ersten 7 von 24 px. „23,1" sah dadurch aus,
als stünde dort ein Leerzeichen.

`showNumber()` setzt den Wert deshalb zeichenweise mit `EPD_ShowChar()` und rückt nach
Komma oder Punkt nur `size/4` vor. **Weiter nicht** — `EPD_ShowChar()` malt die ganze
Zelle inklusive Hintergrund, eine zu weit nach links gezogene Folgezelle radiert das Komma
wieder aus.

## Layout

`COL_X[] = { 2, 264, 526, 789 }`, `ROW_Y[] = { 2, 124, 247 }`, `TILE_H = 122`. Der Trend
steht **rechtsbündig an der Kachelkante** statt in festem Abstand hinter dem Wert: „23,1"
und „-3,5" sind verschieden breit, ein mitwandernder Trend ließe die sechs Kacheln unruhig
wirken. Der Pfeil ist zeilenweise gefüllt gezeichnet, nicht als Umriss — bei 20 × 16 px
wäre eine 1 px starke Kontur kaum zu erkennen.

## Refresh

Alle 10 Minuten `EPD_FastUpdate()`, jeder sechste Durchgang (`FULL_REFRESH_EVERY = 6`) —
also stündlich — mit Löschzyklus. Genau die Aufteilung, die der Vergleich in
`ha_umschalten` nahelegt.

## Ohne Gerät prüfen

```bash
make sim-fetch SKETCH=sketches/ha_kacheln
make sim       SKETCH=sketches/ha_kacheln
```

## Weiterführend

- `../ha_wechsel/CLAUDE.md` — diese Seite im Wechsel mit dem Wetter
