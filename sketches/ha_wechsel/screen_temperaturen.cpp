// Bildschirm 1: alle Raumtemperaturen plus Aussen als Kacheln, sortiert von
// warm nach kalt. Uebernommen aus sketches/ha_kacheln — dort steht auch,
// warum der Trend rechts neben dem Wert steht und nicht darunter.

#include "draw.h"
#include "screens.h"
#include <math.h>

// Das Raster teilt die Flaeche zwischen dem Aussenrahmen in drei Spalten und
// zwei Zeilen; unten bleibt der Streifen fuer die Fusszeile.
static const int COL_X[]  = { FRAME_X0, 264, 526, FRAME_X1 };
static const int NCOL_T = 3;

// Oberkante des Kachelinhalts. Zeile 2 beginnt UNTER der 2 px starken
// Trennlinie, nicht auf ihr — sonst waere die untere Kachel 2 px flacher als
// die obere und die Werte staenden nicht auf einer Linie.
static const int ROW_TOP[] = { FRAME_Y0, 126 };
static const int TILE_H = 122;

static const int NAME_SZ = 24, VALUE_SZ = 48, TREND_SZ = 24;
static const int NAME_DY   = 0;
static const int VALUE_DY  = NAME_DY + NAME_SZ + 12;      // 36
static const int CONTENT_H = VALUE_DY + VALUE_SZ;         // 84
static const int PAD_Y     = (TILE_H - CONTENT_H) / 2;    // 19, oben wie unten
static const int ARROW_W = 20, ARROW_H = 16;

static const float TREND_FLAT = 0.2f;   // darunter gilt es als unveraendert

static void drawTile(const RoomDef* rooms, int col, int row, const Tile& t) {
  const int x      = COL_X[col] + PAD_X;
  const int y      = ROW_TOP[row] + PAD_Y;
  const int rightX = COL_X[col + 1] - PAD_X;

  EPD_ShowString(x, y + NAME_DY, rooms[t.room].label, NAME_SZ, BLACK);

  if (!t.ok) {
    EPD_ShowString(x, y + VALUE_DY, "n/a", VALUE_SZ, BLACK);
    const char* msg = "kein Messwert";
    EPD_ShowString(rightX - textWidth(msg, 16), y + VALUE_DY + VALUE_SZ / 2 - 8,
                   msg, 16, BLACK);
    return;
  }

  char value[16];
  snprintf(value, sizeof(value), "%.1f", t.value);
  commaDecimal(value);
  showNumber(x, y + VALUE_DY, value, VALUE_SZ, BLACK);

  int cursor = x + numberWidth(value, VALUE_SZ) + 14;
  drawDegreeSign(cursor + 7, y + VALUE_DY + 9, 6, 3);
  cursor += 26;
  EPD_ShowString(cursor, y + VALUE_DY, "C", VALUE_SZ, BLACK);

  // Der Trend steht rechtsbuendig an der Kachelkante, nicht in festem Abstand
  // hinter dem Wert: die Werte sind verschieden breit ("23,1" gegen "-3,5"),
  // ein mitwandernder Trend liesse die sechs Kacheln unruhig wirken.
  const int midY = y + VALUE_DY + VALUE_SZ / 2;

  if (!t.hasRef) {
    const char* msg = "kein Vergleich";
    EPD_ShowString(rightX - textWidth(msg, 16), midY - 8, msg, 16, BLACK);
    return;
  }

  const float d = t.value - t.ref;
  const int dir = (d > TREND_FLAT) ? 1 : (d < -TREND_FLAT ? -1 : 0);

  // Ohne Einheit: korrekt waere Kelvin, weil es eine Differenz ist, aber ein
  // "K" hinter der Zahl fragt auf einem Wohnzimmer-Display mehr, als es
  // beantwortet. Pfeil und Fusszeile sagen, was gemeint ist.
  char trend[16];
  // "%+.1f" macht aus -0,04 ein "-0,0" — ein Vorzeichen vor einer Null sieht
  // nach einem Fehler aus. Genau null bekommt deshalb gar keins.
  if (fabsf(d) < 0.05f) snprintf(trend, sizeof(trend), "0.0");
  else                  snprintf(trend, sizeof(trend), "%+.1f", d);
  commaDecimal(trend);

  const int trendW = ARROW_W + 8 + numberWidth(trend, TREND_SZ);
  const int trendX = rightX - trendW;
  drawTrendArrow(trendX, midY - ARROW_H / 2, ARROW_W, ARROW_H, dir);
  showNumber(trendX + ARROW_W + 8, midY - TREND_SZ / 2, trend, TREND_SZ, BLACK);
}

// Warm nach kalt, Kacheln ohne Messwert ans Ende. Insertion Sort, n = 6.
static void sortTiles(Tile* tiles, int n) {
  for (int i = 1; i < n; i++) {
    const Tile key = tiles[i];
    int j = i - 1;
    while (j >= 0) {
      const bool keyFirst = key.ok && (!tiles[j].ok || key.value > tiles[j].value);
      if (!keyFirst) break;
      tiles[j + 1] = tiles[j];
      j--;
    }
    tiles[j + 1] = key;
  }
}

void drawTemperatureScreen(const RoomDef* rooms, Tile* tiles, int n, int trendHours) {
  sortTiles(tiles, n);
  drawFrame(COL_X, NCOL_T);
  fillRect(FRAME_X0, ROW_TOP[1] - 2, FRAME_X1, ROW_TOP[1] - 1, BLACK);

  for (int i = 0; i < n && i < NCOL_T * 2; i++)
    drawTile(rooms, i % NCOL_T, i / NCOL_T, tiles[i]);

  // Kurz gehalten, damit in der Fusszeile noch Platz fuer den Tastenhinweis
  // bleibt: der lange Text aus ha_kacheln reichte hier ueber die Bildmitte.
  char hint[64];
  snprintf(hint, sizeof(hint), "Trend: Grad seit %d h", trendHours);
  drawFooter(hint);
}
