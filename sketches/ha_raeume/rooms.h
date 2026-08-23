#pragma once

// Eigene Typen gehoeren in eine .h, nicht in die .ino: die Arduino-Toolchain
// erzeugt Funktionsprototypen und setzt sie VOR selbst definierte Typen. Eine
// Funktion mit 'RoomDef' als Parameter scheitert sonst an
// "'RoomDef' has not been declared". Siehe auch series.h.

struct RoomDef {
  const char* entity;    // interne entity_id, nicht der Anzeigename
  const char* label;     // Kurzname fuer die Beschriftung, ASCII 32..126.
                         // Mit angehaengtem Wert hoechstens 15 Zeichen, sonst
                         // passt die Beschriftung nicht mehr neben das Diagramm.
};

// Wohin die Beschriftung einer Kurve gezeichnet wird. Die Kurven enden am
// rechten Rand oft dicht beieinander; `y` wird deshalb gegenueber `yEnd`
// verschoben, bis sich keine zwei Beschriftungen mehr ueberlappen.
struct Label {
  int   room;            // Index in ROOMS
  int   yEnd;            // y des letzten Kurvenpunktes
  int   y;               // y der Beschriftung, ggf. verschoben
};
