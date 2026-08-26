#include "zeichnen.h"
#include "EPD.h"

void safePixel(int x, int y, uint16_t color) {
  if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
  Paint_SetPixel((uint16_t)x, (uint16_t)y, color);
}

void fillRect(int x0, int y0, int x1, int y1, uint16_t color) {
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++)
      safePixel(x, y, color);
}

int textWidth(const char* s, int size) {
  return (int)strlen(s) * (size / 2);
}
