# Eigene Sketches

## Doku-Pflicht

**Jeder neue Sketch und jede Änderung an einem bestehenden gehört nach `../README.md`,
Abschnitt *Sketches*.** Ein Sketch, der dort fehlt, existiert für jemanden, der das Repo
zum ersten Mal öffnet, nicht.

Zu aktualisieren sind **zwei** Stellen:

1. **Die Übersichtstabelle** — Zeile mit Link auf den Ordner, Kurzbeschreibung und der
   Angabe, ob WLAN nötig ist.
2. **Der Absatz darunter** — was der Sketch tut *und was man an ihm lernen kann*. Nicht
   „zeigt eine Kurve", sondern die Entscheidung dahinter: warum ein Wert je Pixelspalte,
   warum `strstr()` statt JSON-Parser, warum die Daten vorab geholt werden.

Bei Änderungen prüfen, ob der bestehende Absatz noch stimmt — eine geänderte Taktrate oder
ein anderer Refresh-Modus steht dort oft mit drin.

Zusätzlich pflegen, wenn es passt:

- `../CLAUDE.md` → Sketch-Tabelle unter *Sketches*, und *Harte Fakten*, falls dabei etwas
  gefunden wurde, das sonst wieder Zeit kostet.
- Eine eigene `.md` im Wurzelverzeichnis nur, wenn ein Thema den Absatz sprengt —
  Vorbild ist `../PROGRESS_BAR.md` zum Partial-Refresh.

## Neuen Sketch anlegen

Ordnername und `.ino` müssen **identisch** heißen, sonst findet Arduino den Sketch nicht:

```
sketches/mein_sketch/mein_sketch.ino
```

Die Vendor-Dateien werden **in den Sketch-Ordner kopiert**, nicht nach `libraries/`:

```bash
cp sketches/ha_verlauf/{EPD.cpp,EPD.h,EPD_Init.cpp,EPD_Init.h,EPDfont.h,spi.cpp,spi.h} \
   sketches/mein_sketch/
```

Alle Dateien im Ordner werden automatisch mitkompiliert. `src/` als Ordnername vermeiden:
Arduino behandelt ein `src/` *innerhalb* eines Sketches als Sonderfall.

**Eine eigene `CLAUDE.md` im Sketch-Ordner ist erlaubt und stört den Build nicht.** Arduino
übersetzt nur bekannte Endungen (`.ino`, `.cpp`, `.c`, `.h`, `.S`), alles andere wird
ignoriert — nachgeprüft: mit und ohne die Datei ist das Kompilat byte-gleich groß, und
`make sim` sieht sie ebenfalls nicht (es übersetzt `$(wildcard $(SKETCH)/*.cpp)`). Sinnvoll
ist sie, wenn ein Sketch eine Bedienung hat oder Konstanten, die man ohne Vorwissen falsch
verstellt. Vorbild: `bedienleiste/CLAUDE.md`. Sie ersetzt **nicht** den Absatz in
`../README.md` — der bleibt Pflicht.

Die Vendor-Dateien möglichst **unverändert** lassen — Einstellungen gehören in die `.ino`,
sonst gehen sie beim nächsten Kopieren verloren. Das gilt auch für `Rotation`: `EPD.h`
liefert 180, die Sketches übergeben stattdessen eine eigene `0` an `Paint_NewImage()`.

Braucht der Sketch Zugangsdaten, kommt `secrets.h.example` mit ins Repo und `secrets.h`
nicht — die `.gitignore`-Regel steht bereits, aber **vor** dem ersten Commit prüfen:

```bash
git check-ignore -v sketches/mein_sketch/secrets.h     # muss anschlagen
```

## Fallstricke, die hier immer wieder zuschlagen

- **Eigene `struct`-Typen gehören in eine `.h`**, nicht in die `.ino`. Die Toolchain
  erzeugt Funktionsprototypen und setzt sie *vor* selbst definierte Typen; eine Funktion
  mit `struct Foo` als Parameter scheitert dann an `'Foo' has not been declared`.
  Beispiele: `ha_verlauf/series.h`, `ha_umschalten/screens.h`.
- **`Paint_SetPixel()` prüft die Koordinaten nicht.** Eigener Zeichencode braucht einen
  `safePixel()`-Wrapper, der in `int` rechnet — sonst ist ein Koordinatenfehler kein
  falsches Bild, sondern ein Speicherüberschreiber.
- **Fehler gehören auf das Display**, nicht nur ins Log. Sonst sieht man bei einem Problem
  nur ein leeres Panel. Vorbild: `ha_temperatur` und `ha_verlauf` zeigen HTTP 401/404 mit
  einem Hinweis auf die wahrscheinliche Ursache.
- **Nur ASCII 32..126 und die Schriftgrößen 12, 16, 24, 48.** Umlaute und `°` lesen über
  das Font-Array hinaus.

Alles Weitere — Zeichen-API, Refresh-Modi, Home-Assistant-Fallen — steht in `../CLAUDE.md`.
