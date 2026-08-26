# ha_wechsel

**Der Betriebssketch.** Zeigt abwechselnd die Raumtemperaturen und die Wetterlage.

```bash
cp secrets.h.example secrets.h
make flash SKETCH=sketches/ha_wechsel
```

## Bedienung

| Eingabe | Wirkung |
|---|---|
| automatisch | alle 60 s eine Seite weiter (`SWITCH_INTERVAL_MS`) |
| EXIT (GPIO 1) | blättert sofort um, die Minute läuft danach von vorn |
| MENU (GPIO 2) | holt die Daten neu und zeichnet die aktuelle Seite frisch |

EXIT startet die Minute bewusst neu — sonst wäre die aufgerufene Seite womöglich nach zwei
Sekunden wieder weg.

## Daten getrennt vom Bildwechsel

Geholt wird alle **zehn** Minuten (`FETCH_INTERVAL_MS`), gewechselt jede Minute; gezeichnet
wird aus dem Zwischenspeicher. Andernfalls liefe jede Minute eine Runde von elf
HTTP-Anfragen, und beim Umblättern sähe man die Netzwerklatenz statt das Panel.

Läuft der Zehn-Minuten-Abruf ab, wird **nicht** neu gezeichnet: Der nächste Wechsel steht
ohnehin binnen einer Minute an und bringt die frischen Zahlen mit. Ein zusätzlicher
Bildaufbau wäre ein Refresh-Zyklus für nichts.

## Aufgeteilt auf mehrere Dateien

| Datei | Inhalt |
|---|---|
| `ha_wechsel.ino` | Daten, Tasten, Taktung |
| `draw.cpp` / `draw.h` | gemeinsame Zeichenhilfen |
| `screen_temperaturen.cpp` | die Kachelseite |
| `screen_wetter.cpp` | die Wetterseite |
| `screens.h` | die `struct`-Typen beider Seiten |

In einer einzigen `.ino` wären es rund 900 Zeilen mit der Hälfte doppelt — beide Seiten
brauchen dieselben Primitive, weil `EPD.h` nur Linie, Rechteck und Kreis kennt. Der
Simulator übersetzt seitdem **alle** `.cpp` des Sketch-Ordners (`SIM_SRCS` im `Makefile`),
nicht nur `EPD.cpp`.

## Fußzeile mit Platzprüfung

Der Tastenhinweis sitzt mittig in der tatsächlich freien Lücke zwischen Zeitstempel und
rechtem Text, nicht auf der Bildmitte: Die rechten Texte sind je Seite verschieden lang,
und auf der Temperaturseite stand sonst „EXIT blaetPfeil und Zahl:" übereinander. Passt
der Hinweis nicht, entfällt er ganz.

## Refresh

Ein Wechsel pro Minute sind **1.440 Bildwechsel am Tag**. Deshalb `EPD_FastUpdate()` als
Regelfall und nur jeder 60. Aufbau (`FULL_REFRESH_EVERY = 60`) — also stündlich — mit
Löschzyklus. Bei jedem Wechsel voll zu refreshen hieße sechzigmal pro Stunde mehrfaches
Schwarzblitzen; das fällt mehr auf als das Ghosting, das es verhindert.

## Ohne Gerät prüfen

```bash
make sim-fetch SKETCH=sketches/ha_wechsel
make sim       SKETCH=sketches/ha_wechsel
```

## Weiterführend

- `../ha_kacheln/CLAUDE.md` und `../ha_wetter/CLAUDE.md` — die beiden Seiten einzeln,
  mit der Begründung für Trendzeitraum, Pfeilrichtung und Icons
