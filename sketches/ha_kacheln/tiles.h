#pragma once

// Eigene Typen gehoeren in eine .h, nicht in die .ino: die Arduino-Toolchain
// erzeugt Funktionsprototypen und setzt sie VOR selbst definierte Typen. Eine
// Funktion mit 'Tile' als Parameter scheitert sonst an
// "'Tile' has not been declared". Siehe auch ha_raeume/rooms.h.

struct RoomDef {
  const char* entity;    // interne entity_id, nicht der Anzeigename
  const char* label;     // Kurzname, ASCII 32..126, hoechstens 15 Zeichen
                         // (264 px Kachelbreite / 8 px je Zeichen minus Rand)
};

// Eine Kachel: der Messwert eines Raums plus sein Trend.
//
// `value` und `ref` sind getrennt gespeichert statt nur die Differenz, weil
// beide auf dem Display landen koennen und weil ein fehlender Referenzwert
// (Sensor erst seit einer Stunde online) etwas anderes ist als ein Trend von
// null — im ersten Fall bleibt die Pfeilstelle leer.
struct Tile {
  int   room    = -1;    // Index in ROOMS
  bool  ok      = false; // aktueller Wert vorhanden?
  bool  hasRef  = false; // Vergleichswert von vor TREND_HOURS Stunden vorhanden?
  float value   = 0;     // aktuell, Grad Celsius
  float ref     = 0;     // vor TREND_HOURS Stunden
  int   points  = 0;     // Messpunkte in der Antwort, nur fuers Log
};
