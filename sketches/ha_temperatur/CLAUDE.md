# ha_temperatur

Holt **einen** Sensorwert aus Home Assistant und zeigt ihn groß an. Der einfachste der
HA-Sketches und die Vorlage für alles Weitere.

```bash
cp secrets.h.example secrets.h    # einmalig, Token eintragen
make flash SKETCH=sketches/ha_temperatur
```

`secrets.h` ist per `.gitignore` ausgeschlossen. Vor dem ersten Commit prüfen:
`git check-ignore -v sketches/ha_temperatur/secrets.h` muss anschlagen.

## Wie die Daten kommen

```
GET /api/states/<entity_id>
Authorization: Bearer <Token>
```

**`entity_id` nie raten.** Bei den SONOFF-Sensoren passt der Anzeigename nicht zur ID:
`sensor.temperatur_sonoff_snzb_02d_temperatur` heißt „Temperatur **Badezimmer**". Immer
über `friendly_name` verifizieren.

**`unavailable` und `unknown` sind gültige Zustände**, keine Fehler. `atof()` macht daraus
0.0 — auf dem Display steht dann 0 Grad, als wäre gemessen worden.

## Entscheidungen im Sketch

**Fehler gehören auf das Display.** HTTP 401 und 404 werden mit einem Hinweis auf die
wahrscheinliche Ursache angezeigt, nicht nur geloggt — sonst sieht man bei einem Problem
nur ein leeres Panel und weiß nicht, ob Board, WLAN oder Token schuld ist.

**Das Grad-Zeichen ist selbst gezeichnet.** Die Font-Arrays decken ASCII 32..126 ab, `°`
(176) läge weit dahinter und läse über das Array hinaus.

**`UPDATE_INTERVAL_MS = 15 min`.** Refresh-Zyklen von E-Paper sind endlich, und die
Sensoren melden ohnehin nur alle paar Minuten. Wer den Wert kleiner dreht, kauft nichts an
Aktualität und verbraucht Panel-Lebensdauer.

**`TZ_BERLIN`** setzt Zeitzone samt Sommerzeitregel für den „Aktualisiert"-Zeitstempel.
Die REST-API antwortet in **UTC** — ohne diese Zeile stünde eine um zwei Stunden falsche
Uhrzeit auf dem Panel.

## Weiterführend

- `../../CLAUDE.md` → *Home Assistant* — die Fallen der API
- `../ha_verlauf/CLAUDE.md` — derselbe Sensor, aber als Verlauf
