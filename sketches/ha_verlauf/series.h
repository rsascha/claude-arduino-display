#pragma once
#include <time.h>

// Bewusst in einer eigenen Header-Datei, nicht in der .ino:
// Die Arduino-IDE erzeugt beim Kompilieren automatisch Funktionsprototypen und
// setzt sie an den Anfang der Datei — noch VOR selbst definierte Typen. Eine
// Funktion, die 'Series' als Parameter hat, scheitert dann mit
// "'Series' has not been declared". Aus einem Header ist der Typ rechtzeitig da.
struct Series {
  int    count   = 0;
  float  vmin    = 0, vmax = 0;
  time_t tFirst  = 0, tLast = 0;
  float  current = 0;
};
