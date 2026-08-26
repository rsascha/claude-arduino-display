# smiley

Zeichnet ein Smiley-Gesicht. Zweck ist nicht das Gesicht, sondern das Ergänzen fehlender
Zeichenprimitive: `EPD.h` kann Linie, Rechteck und Kreis — sonst nichts. Kein WLAN.

```bash
make flash SKETCH=sketches/smiley
```

## Die drei Bausteine, die von hier aus weiterverwendet werden

**`safePixel()`** — der Wrapper, den **jeder** eigene Zeichencode braucht.
`Paint_SetPixel()` prüft seine Koordinaten nicht und rechnet mit `uint16_t`: ein negativer
Wert wird klaglos zu einer riesigen Zahl, und geschrieben wird trotzdem. Ohne den Wrapper
ist ein Koordinatenfehler kein falsches Bild, sondern ein Speicherüberschreiber.

**`thickCircle()`** — `EPD_DrawCircle()` zeichnet 1 px dünn und ist auf diesem Panel kaum
zu erkennen. Mehrere Kreise mit wachsendem Radius übereinander ergeben eine sichtbare
Linie. Dieselbe Beobachtung gilt für Rahmen: 2 px sind das Minimum.

**`drawArc()`** — es gibt keine Bogen-Funktion. Der Bogen wird aus einzelnen Pixeln
gebaut, Schrittweite `0.5f / radius` im Bogenmaß. Größere Schritte reißen die Linie bei
großen Radien auf. Winkel in Grad, y zeigt nach unten: 0° ist rechts, 90° unten — ein
Bogen von 30° bis 150° ist damit ein Lächeln.

## Weiterführend

- `../CLAUDE.md` — Doku-Pflicht und Sketch-Index
- `../../CLAUDE.md` → *Harte Fakten*
