// Host-Build eines Sketches: fuehrt setup() aus und schreibt den Bildpuffer
// als PBM nach stdout. `make sim` wandelt das mit ImageMagick in ein PNG.
//
// Der Sketch wird per -include eingebunden und bleibt dabei unveraendert. Was
// er an Arduino-Umgebung braucht, liefern die Stubs in arduino/ — inklusive
// eines HTTPClient, der statt einer HTTP-Antwort eine lokale Datei liest.
//
// Entscheidend ist, dass EPD.cpp und EPDfont.h die ECHTEN Dateien aus dem
// Sketch-Ordner sind. Nur deshalb stimmt das Ergebnis pixelgenau mit dem Panel
// ueberein; ein Nachbau mit anderen Schriftmetriken haette genau den Fehler
// verdeckt, der in ha_verlauf zur 6-px-Ueberlappung gefuehrt hat.

#include <Arduino.h>
#include <WiFi.h>

SerialStub Serial;
WiFiStub   WiFi;

static const char* g_dataDir = "tools/simulator/data";
const char* simDataDir() { return g_dataDir; }

// --- Stubs fuer die Panel-Ansteuerung ---------------------------------------
// EPD.cpp (das Zeichnen) kommt echt aus dem Sketch-Ordner. Nur EPD_Init.cpp und
// spi.cpp, die tatsaechlich Pins wackeln lassen, werden hier ersetzt.
void EPD_READBUSY(void)      {}
void EPD_HW_RESET(void)      {}
void EPD_Update(void)        {}
void EPD_PartUpdate(void)    {}
void EPD_FastUpdate(void)    {}
void EPD_DeepSleep(void)     {}
void EPD_Init(void)          {}
void EPD_FastMode1Init(void) {}
void EPD_SetRAMMP(void)      {}
void EPD_SetRAMMA(void)      {}
void EPD_SetRAMSP(void)      {}
void EPD_SetRAMSA(void)      {}
void EPD_Clear_R26A6H(void)  {}
void EPD_Display_Clear(void) {}
void EPD_Display(const uint8_t*) {}
void EPD_WhiteScreen_ALL_Fast(const unsigned char*) {}
void EPD_GPIOInit(void)      {}
void EPD_WR_REG(uint8_t)     {}
void EPD_WR_DATA8(uint8_t)   {}
void SPI_Write(uint8_t)      {}

// --- Der Sketch -------------------------------------------------------------
#include SKETCH_PATH

// --- Bildausgabe ------------------------------------------------------------

// Der Puffer ist 800 px breit, sichtbar sind 792: zwei SSD1683 mit je einer
// Haelfte, dazwischen 8 ungenutzte Spalten. Paint_SetPixel() ueberspringt sie
// mit `if (Xpoint >= 396) Xpoint += 8` — hier dieselbe Rechnung rueckwaerts,
// damit das PNG zeigt, was das Panel zeigt, und nicht den Rohpuffer.
static const int VIS_W = 792, VIS_H = 272, BUF_STRIDE = 800 / 8;

static void writePbm(const uint8_t* buf) {
  printf("P1\n%d %d\n", VIS_W, VIS_H);
  for (int y = 0; y < VIS_H; y++) {
    for (int x = 0; x < VIS_W; x++) {
      const int bx = (x >= 396) ? x + 8 : x;
      const int bit = (buf[y * BUF_STRIDE + bx / 8] >> (7 - (bx % 8))) & 1;
      // Im Puffer ist 1 = weiss (WHITE 0xFF), in PBM ist 1 = schwarz.
      printf("%d", bit ? 0 : 1);
    }
    printf("\n");
  }
}

int main(int argc, char** argv) {
  if (argc > 1) g_dataDir = argv[1];

  fprintf(stderr, "Simulator: setup() laeuft, Daten aus %s\n", g_dataDir);
  setup();

  writePbm(ImageBW);
  fprintf(stderr, "Simulator: %dx%d geschrieben\n", VIS_W, VIS_H);
  return 0;
}
