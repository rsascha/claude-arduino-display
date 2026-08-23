#pragma once

// Eigene Typen gehoeren in eine .h, nicht in die .ino: die Arduino-Toolchain
// erzeugt Funktionsprototypen und setzt sie VOR selbst definierte Typen. Eine
// Funktion mit 'IconKind' oder 'Reading' als Parameter scheitert sonst an
// "has not been declared". Siehe auch ha_kacheln/tiles.h.

// Die sechs Bilder, die der Sketch zeichnen kann. Home Assistant kennt 15
// Wetterzustaende; die Zuordnung steht als Tabelle in der .ino.
enum IconKind {
  ICON_SUN,      // Sonne
  ICON_MOON,     // Mondsichel — nur fuer 'clear-night'
  ICON_PARTLY,   // Sonne mit Wolke davor
  ICON_CLOUD,    // Wolke
  ICON_RAIN,     // Wolke mit Tropfen
  ICON_SNOW,     // Wolke mit Flocken
};

struct ConditionDef {
  const char* state;   // Zustand, wie ihn weather.<entity> liefert
  IconKind    icon;
  const char* label;   // deutsches Wort fuers Display, ASCII 32..126
};

// Ein abgefragter Wert. `text` haelt die Rohantwort, weil manche Werte gar
// keine Zahl sind ("NNW", "partlycloudy") — und weil 'unavailable' und
// 'unknown' gueltige Zustaende sind, die atof() klaglos zu 0.0 machen wuerde.
struct Reading {
  bool  ok    = false;
  float value = 0;
  char  text[24] = { 0 };
};

// Messwert plus Vergleichswert von vor N Stunden, beides aus derselben
// History-Antwort. Wie in ha_kacheln, hier aber fuer den Luftdruck.
struct Trend {
  bool  ok     = false;
  bool  hasRef = false;
  float value  = 0;
  float ref    = 0;
  int   points = 0;
};
