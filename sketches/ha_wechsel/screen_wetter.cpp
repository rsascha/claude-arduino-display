// Bildschirm 2: Wind, Luftdruck und Wetterlage in vier Spalten. Uebernommen
// aus sketches/ha_wetter — dort steht auch, warum der Pfeil auf die
// Herkunftsrichtung zeigt und warum die Icons als Umriss ueber den Umweg
// Weiss entstehen.

#include "draw.h"
#include "screens.h"
#include <math.h>

static const int COL_X[] = { FRAME_X0, 199, 396, 593, FRAME_X1 };
static const int NCOL_W = 4;

static const int HEAD_Y  = 10;    // Spaltenueberschrift, Groesse 16
static const int VALUE_Y = 100;   // grosse Zahl, Groesse 48
static const int UNIT_Y  = 160;   // Einheit, Groesse 24
static const int EXTRA_Y = 200;   // Trend bzw. Zustandswort

static const int ROSE_CY = 118, ROSE_R = 66;
static const int ROSE_TEXT_Y = 192, ROSE_DEG_Y = 222;

static int colCenter(int col) { return (COL_X[col] + COL_X[col + 1]) / 2; }

// --- Kompassrose ------------------------------------------------------------

static void drawCompass(int cx, int cy, int r, bool haveDir, float deg) {
  drawRing(cx, cy, r, 3, BLACK);

  for (int i = 0; i < 16; i++) {
    const float a = i * 22.5f;
    const int len = (i % 4 == 0) ? 12 : (i % 2 == 0 ? 8 : 4);
    float x0, y0, x1, y1;
    polar(cx, cy, a, (float)(r - 3), x0, y0);
    polar(cx, cy, a, (float)(r - 3 - len), x1, y1);
    thickLine((int)x0, (int)y0, (int)x1, (int)y1, (i % 4 == 0) ? 3 : 2, BLACK);
  }

  // Buchstaben INNEN: aussen braeuchten sie 16 px zusaetzlichen Rand, und die
  // Spalte ist nur 197 px breit.
  const int lr = r - 30;
  const char* names[] = { "N", "O", "S", "W" };
  for (int i = 0; i < 4; i++) {
    float x, y;
    polar(cx, cy, i * 90.0f, (float)lr, x, y);
    EPD_ShowString((uint16_t)(x - 4), (uint16_t)(y - 8), names[i], 16, BLACK);
  }

  if (!haveDir) { showCentered(cx, cy - 8, "?", 24); return; }

  // Der Pfeil zeigt dorthin, WOHER der Wind kommt: Home Assistant liefert die
  // Richtung nach meteorologischer Konvention als Herkunft. Ein Pfeil in
  // Wehrichtung zeigte bei "NNW" nach SSO, und Bild und Text saehen aus, als
  // widersprachen sie sich.
  const float L = r - 34, W = 11, T = 22;
  float tx, ty, lx, ly, rx, ry, bx, by;
  polar(cx, cy, deg,          L, tx, ty);
  polar(cx, cy, deg -  90.0f, W, lx, ly);
  polar(cx, cy, deg +  90.0f, W, rx, ry);
  polar(cx, cy, deg + 180.0f, T, bx, by);
  fillTriangle(tx, ty, lx, ly, rx, ry, BLACK);
  fillTriangle(lx, ly, bx, by, rx, ry, BLACK);
  fillDisc(cx, cy, 4, BLACK);
}

// --- Wetter-Icons -----------------------------------------------------------

// Die Wolke als Vereinigung dreier Kreise und eines Rechtecks. `grow` blaeht
// die Form auf: erst in Schwarz mit grow=2, dann in Weiss mit grow=0 darueber —
// uebrig bleibt ein 2 px starker Umriss. Der Umweg ist noetig, weil sich die
// Boegen der drei Baeuche sonst gegenseitig durchschneiden wuerden.
static void cloudShape(int cx, int cy, int w, int grow, uint16_t color) {
  // Das Rechteck endet genau an den aeusseren Kreisraendern und nicht bei
  // cx +- w/2: sonst steht rechts eine rechtwinklige Stufe ueber den Bauch
  // hinaus, was am Panel wie ein Zeichenfehler aussieht.
  const int leftX  = cx - w / 3, leftR  = w / 6;
  const int midX   = cx - w / 8, midR   = w / 4;
  const int rightX = cx + w / 4, rightR = w / 5;

  fillDisc(midX,   cy - w / 10, midR   + grow, color);
  fillDisc(leftX,  cy + w / 12, leftR  + grow, color);
  fillDisc(rightX, cy + w / 20, rightR + grow, color);
  fillRect(leftX  - leftR  - grow, cy + w / 12 - grow,
           rightX + rightR + grow, cy + w / 5  + grow, color);
}

static void drawCloud(int cx, int cy, int w) {
  cloudShape(cx, cy, w, 2, BLACK);
  cloudShape(cx, cy, w, 0, WHITE);
}

static void drawSun(int cx, int cy, int r) {
  drawRing(cx, cy, r, 3, BLACK);
  for (int i = 0; i < 8; i++) {
    float x0, y0, x1, y1;
    polar(cx, cy, i * 45.0f, (float)(r + 6),  x0, y0);
    polar(cx, cy, i * 45.0f, (float)(r + 16), x1, y1);
    thickLine((int)x0, (int)y0, (int)x1, (int)y1, 3, BLACK);
  }
}

static void drawMoon(int cx, int cy, int r) {
  fillDisc(cx, cy, r, BLACK);
  fillDisc(cx + r / 2, cy - r / 6, r - 3, WHITE);
}

static void drawDrops(int cx, int cy, int w, int n) {
  for (int i = 0; i < n; i++) {
    const int x = cx - w / 3 + i * (w / (n + 1));
    thickLine(x, cy, x - 6, cy + 16, 3, BLACK);
  }
}

static void drawFlakes(int cx, int cy, int w, int n) {
  for (int i = 0; i < n; i++) {
    const int x = cx - w / 3 + i * (w / (n + 1));
    const int y = cy + 8 + (i % 2) * 8;
    thickLine(x - 6, y, x + 6, y, 2, BLACK);
    thickLine(x, y - 6, x, y + 6, 2, BLACK);
    thickLine(x - 4, y - 4, x + 4, y + 4, 2, BLACK);
    thickLine(x - 4, y + 4, x + 4, y - 4, 2, BLACK);
  }
}

static void drawIcon(IconKind kind, int cx, int cy) {
  switch (kind) {
    case ICON_SUN:  drawSun(cx, cy, 26);  break;
    case ICON_MOON: drawMoon(cx, cy, 30); break;
    case ICON_PARTLY:
      drawSun(cx - 22, cy - 20, 18);
      drawCloud(cx + 14, cy + 14, 80);
      break;
    case ICON_CLOUD: drawCloud(cx, cy, 96); break;
    case ICON_RAIN:
      drawCloud(cx, cy - 14, 90);
      drawDrops(cx, cy + 22, 90, 4);
      break;
    case ICON_SNOW:
      drawCloud(cx, cy - 16, 90);
      drawFlakes(cx, cy + 18, 90, 3);
      break;
  }
}

// --- Die vier Spalten -------------------------------------------------------

static void drawDirection(const Reading& windDir, const Reading& windSector) {
  const int cx = colCenter(0);
  showCentered(cx, HEAD_Y, "Richtung", 16);
  drawCompass(cx, ROSE_CY, ROSE_R, windDir.ok, windDir.value);
  showCentered(cx, ROSE_TEXT_Y, windSector.ok ? windSector.text : "n/a", 24);
  if (windDir.ok) {
    char deg[24];
    snprintf(deg, sizeof(deg), "aus %d Grad", (int)lroundf(windDir.value));
    showCentered(cx, ROSE_DEG_Y, deg, 16);
  }
}

static void drawWind(const Reading& windSpeed) {
  const int cx = colCenter(1);
  showCentered(cx, HEAD_Y, "Wind", 16);
  if (!windSpeed.ok) { showCentered(cx, VALUE_Y, "n/a", 48); return; }

  char v[16];
  snprintf(v, sizeof(v), "%.1f", windSpeed.value);
  commaDecimal(v);
  showNumberCentered(cx, VALUE_Y, v, 48);
  showCentered(cx, UNIT_Y, "km/h", 24);
}

static void drawPressure(const Trend& pressure, int trendHours) {
  const int cx = colCenter(2);
  showCentered(cx, HEAD_Y, "Luftdruck", 16);
  if (!pressure.ok) { showCentered(cx, VALUE_Y, "n/a", 48); return; }

  char v[16];
  snprintf(v, sizeof(v), "%.1f", pressure.value);
  commaDecimal(v);
  showNumberCentered(cx, VALUE_Y, v, 48);
  showCentered(cx, UNIT_Y, "hPa", 24);

  if (!pressure.hasRef) { showCentered(cx, EXTRA_Y, "kein Vergleich", 16); return; }

  const float d = pressure.value - pressure.ref;
  char t[24];
  if (fabsf(d) < 0.05f) snprintf(t, sizeof(t), "0.0");
  else                  snprintf(t, sizeof(t), "%+.1f", d);
  commaDecimal(t);
  showNumberCentered(cx, EXTRA_Y, t, 24);

  char span[24];
  snprintf(span, sizeof(span), "seit %d h", trendHours);
  showCentered(cx, EXTRA_Y + 26, span, 16);
}

static void drawWeatherColumn(const ConditionDef* conditions, int nCondition,
                              const Reading& condition) {
  const int cx = colCenter(3);
  showCentered(cx, HEAD_Y, "Wetter", 16);

  // Was nicht in der Tabelle steht, wird zur Wolke mit dem Rohzustand als
  // Text — besser ein unbekanntes Wort als ein falsches Bild.
  IconKind icon = ICON_CLOUD;
  const char* label = condition.ok ? condition.text : "n/a";
  if (condition.ok) {
    for (int i = 0; i < nCondition; i++)
      if (!strcmp(conditions[i].state, condition.text)) {
        icon  = conditions[i].icon;
        label = conditions[i].label;
        break;
      }
  }
  drawIcon(icon, cx, 118);
  showCentered(cx, EXTRA_Y + 6, label, 24);
}

void drawWeatherScreen(const ConditionDef* conditions, int nCondition,
                       const Reading& windSpeed, const Reading& windDir,
                       const Reading& windSector, const Reading& condition,
                       const Trend& pressure, int trendHours) {
  drawFrame(COL_X, NCOL_W);
  drawDirection(windDir, windSector);
  drawWind(windSpeed);
  drawPressure(pressure, trendHours);
  drawWeatherColumn(conditions, nCondition, condition);
  drawFooter("Luftdruck: Solarnode vor Ort");
}
