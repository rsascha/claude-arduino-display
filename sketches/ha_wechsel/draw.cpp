#include "draw.h"
#include <math.h>

void safePixel(int x, int y, uint16_t color) {
  if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
  Paint_SetPixel((uint16_t)x, (uint16_t)y, color);
}

void fillRect(int x0, int y0, int x1, int y1, uint16_t color) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) safePixel(x, y, color);
}

void safeLine(int x0, int y0, int x1, int y1, uint16_t color) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    safePixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    const int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void thickLine(int x0, int y0, int x1, int y1, int th, uint16_t color) {
  for (int i = 0; i < th; i++) {
    safeLine(x0 + i, y0, x1 + i, y1, color);
    safeLine(x0, y0 + i, x1, y1 + i, color);
  }
}

void fillDisc(int cx, int cy, int r, uint16_t color) {
  const int rr = r * r;
  for (int y = -r; y <= r; y++)
    for (int x = -r; x <= r; x++)
      if (x * x + y * y <= rr) safePixel(cx + x, cy + y, color);
}

void drawRing(int cx, int cy, int r, int th, uint16_t color) {
  const int ro = r * r, ri = (r - th) * (r - th);
  for (int y = -r; y <= r; y++)
    for (int x = -r; x <= r; x++) {
      const int d = x * x + y * y;
      if (d <= ro && d >= ri) safePixel(cx + x, cy + y, color);
    }
}

// Gefuelltes Dreieck ueber die Kantenfunktion — fuer Trendpfeil und
// Kompassnadel. EPD.h kennt nichts dergleichen.
static float edgeFn(float ax, float ay, float bx, float by, float px, float py) {
  return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

void fillTriangle(float x0, float y0, float x1, float y1,
                  float x2, float y2, uint16_t color) {
  const int minX = (int)floorf(fminf(x0, fminf(x1, x2)));
  const int maxX = (int)ceilf (fmaxf(x0, fmaxf(x1, x2)));
  const int minY = (int)floorf(fminf(y0, fminf(y1, y2)));
  const int maxY = (int)ceilf (fmaxf(y0, fmaxf(y1, y2)));
  for (int y = minY; y <= maxY; y++)
    for (int x = minX; x <= maxX; x++) {
      const float px = x + 0.5f, py = y + 0.5f;
      const float w0 = edgeFn(x0, y0, x1, y1, px, py);
      const float w1 = edgeFn(x1, y1, x2, y2, px, py);
      const float w2 = edgeFn(x2, y2, x0, y0, px, py);
      if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0))
        safePixel(x, y, color);
    }
}

void polar(int cx, int cy, float deg, float r, float& x, float& y) {
  const float a = deg * (float)M_PI / 180.0f;
  x = cx + r * sinf(a);
  y = cy - r * cosf(a);
}

void drawDegreeSign(int cx, int cy, int r, int thickness) {
  drawRing(cx, cy, r + thickness, thickness, BLACK);
}

// Gefuelltes Dreieck statt Umriss: bei 16 x 12 px waere eine 1 px starke
// Kontur auf dem Panel kaum zu erkennen.
void drawTrendArrow(int x, int y, int w, int h, int dir) {
  const int half = w / 2;
  if (dir == 0) {
    fillRect(x, y + h / 2 - 1, x + w - 1, y + h / 2 + 1, BLACK);
    return;
  }
  for (int i = 0; i < h; i++) {
    const int spread = (i * half) / (h - 1);      // i == 0 ist die Spitze
    const int yy = (dir > 0) ? (y + i) : (y + h - 1 - i);
    fillRect(x + half - spread, yy, x + half + spread, yy, BLACK);
  }
}

int textWidth(const char* s, int size) { return (int)strlen(s) * (size / 2); }

void showCentered(int cx, int y, const char* s, int size) {
  EPD_ShowString(cx - textWidth(s, size) / 2, y, s, size, BLACK);
}

void commaDecimal(char* s) {
  for (char* p = s; *p; p++) if (*p == '.') *p = ',';
}

static int advanceFor(char c, int size) {
  return (c == ',' || c == '.') ? size / 4 : size / 2;
}

int numberWidth(const char* s, int size) {
  int w = 0;
  for (const char* p = s; *p; p++) w += advanceFor(*p, size);
  return w;
}

void showNumber(int x, int y, const char* s, int size, uint16_t color) {
  for (const char* p = s; *p; p++) {
    EPD_ShowChar((uint16_t)x, (uint16_t)y, (uint16_t)*p, (uint16_t)size, color);
    x += advanceFor(*p, size);
  }
}

void showNumberCentered(int cx, int y, const char* s, int size) {
  showNumber(cx - numberWidth(s, size) / 2, y, s, size, BLACK);
}

// Rahmen und Trennlinien je 2 px stark: eine einzelne Pixelreihe ist auf
// diesem Panel kaum zu sehen.
void drawFrame(const int* colX, int ncol) {
  EPD_DrawRectangle(FRAME_X0,     FRAME_Y0,     FRAME_X1,     FRAME_Y1,     BLACK, 0);
  EPD_DrawRectangle(FRAME_X0 + 1, FRAME_Y0 + 1, FRAME_X1 - 1, FRAME_Y1 - 1, BLACK, 0);
  for (int c = 1; c < ncol; c++)
    fillRect(colX[c], FRAME_Y0, colX[c] + 1, GRID_Y1, BLACK);
  fillRect(FRAME_X0, GRID_Y1, FRAME_X1, GRID_Y1 + 1, BLACK);
}

void drawFooter(const char* rightHint) {
  char stamp[48];
  struct tm lt;
  const time_t nowT = time(nullptr);
  localtime_r(&nowT, &lt);
  strftime(stamp, sizeof(stamp), "Stand %d.%m. %H:%M", &lt);
  EPD_ShowString(FRAME_X0 + PAD_X, FOOTER_Y, stamp, FOOTER_SZ, BLACK);

  const int stampEnd = FRAME_X0 + PAD_X + textWidth(stamp, FOOTER_SZ);
  const int hintStart = FRAME_X1 - PAD_X - textWidth(rightHint, FOOTER_SZ);
  EPD_ShowString(hintStart, FOOTER_Y, rightHint, FOOTER_SZ, BLACK);

  // Der Hinweis auf EXIT sitzt mittig in der tatsaechlich freien Luecke, nicht
  // auf der Bildmitte: der rechte Text ist je Bildschirm verschieden lang und
  // reichte auf der Temperaturseite ueber die Mitte hinaus — dort stand dann
  // "EXIT blaetPfeil und Zahl:" uebereinander. Passt er nicht, entfaellt er
  // lieber ganz.
  const char* keyHint = "EXIT blaettert - MENU laedt neu";
  const int need = textWidth(keyHint, FOOTER_SZ) + 24;
  if (hintStart - stampEnd >= need)
    showCentered((stampEnd + hintStart) / 2, FOOTER_Y, keyHint, FOOTER_SZ);
}
