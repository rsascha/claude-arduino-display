#pragma once

// Eigene Typen gehoeren in eine .h, nicht in die .ino: die Arduino-Toolchain
// erzeugt Funktionsprototypen und setzt sie VOR selbst definierte Typen.
// Eine Funktion mit 'RefreshMode' als Parameter scheitert sonst an
// "'RefreshMode' has not been declared". Siehe auch series.h.

// Ein anzuzeigender Sensor.
struct SensorDef {
  const char* entity;     // interne entity_id, nicht der Anzeigename
  const char* fallback;   // Beschriftung, falls HA keinen friendly_name liefert
};

// Die drei Refresh-Verfahren des SSD1683. Sie unterscheiden sich nur im
// Parameter zu Kommando 0x22 (Datenblatt S. 29) — und darin, ob vorher ein
// Loeschzyklus laeuft:
//
//   FULL  Panel weiss loeschen (EPD_Display_Clear + 0xF7), dann Bild mit 0xF7.
//         Sauberstes Ergebnis, aber der volle Schwarz-Weiss-Durchlauf.
//   FAST  Kein Loeschzyklus, Bild mit 0xC7 — ohne Temperatur- und LUT-Nachladen.
//   PART  Kein Loeschzyklus, Bild mit 0xDC. Am schnellsten und ohne Flackern;
//         0xDC steht allerdings in keiner Zeile der Tabelle auf S. 29.
enum RefreshKind { REFRESH_FULL, REFRESH_FAST, REFRESH_PART };

struct RefreshMode {
  RefreshKind kind;
  const char* name;
};
