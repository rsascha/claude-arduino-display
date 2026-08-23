#pragma once

// Eigene Typen gehoeren in eine .h, nicht in die .ino: die Arduino-Toolchain
// erzeugt Funktionsprototypen und setzt sie VOR selbst definierte Typen.

// --- Temperaturen -----------------------------------------------------------

struct RoomDef {
  const char* entity;    // interne entity_id, nicht der Anzeigename
  const char* label;     // Kurzname fuers Display, ASCII 32..126
};

// Messwert plus Vergleichswert von vor N Stunden, beides aus derselben
// History-Antwort. `value` und `ref` bleiben getrennt, weil ein fehlender
// Vergleichswert etwas anderes ist als ein Trend von null.
struct Tile {
  int   room    = -1;
  bool  ok      = false;
  bool  hasRef  = false;
  float value   = 0;
  float ref     = 0;
  int   points  = 0;
};

// --- Wetter -----------------------------------------------------------------

enum IconKind {
  ICON_SUN, ICON_MOON, ICON_PARTLY, ICON_CLOUD, ICON_RAIN, ICON_SNOW,
};

struct ConditionDef {
  const char* state;     // Zustand, wie ihn weather.<entity> liefert
  IconKind    icon;
  const char* label;     // deutsches Wort fuers Display
};

// Ein abgefragter Wert. `text` haelt die Rohantwort, weil manche Werte gar
// keine Zahl sind ("NNW", "partlycloudy").
struct Reading {
  bool  ok    = false;
  float value = 0;
  char  text[24] = { 0 };
};

struct Trend {
  bool  ok     = false;
  bool  hasRef = false;
  float value  = 0;
  float ref    = 0;
  int   points = 0;
};

// --- Die beiden Bildschirme -------------------------------------------------
// Beide malen in den aktuellen Paint-Puffer, Rahmen und Fusszeile inklusive.
// Der Bildwechsel selbst gehoert nicht dazu — den macht die .ino, weil nur sie
// weiss, ob dieser Durchgang ein Vollrefresh ist.

void drawTemperatureScreen(const RoomDef* rooms, Tile* tiles, int n, int trendHours);

void drawWeatherScreen(const ConditionDef* conditions, int nCondition,
                       const Reading& windSpeed, const Reading& windDir,
                       const Reading& windSector, const Reading& condition,
                       const Trend& pressure, int trendHours);
