// Arduino-Ersatz fuer den Host-Build (tools/simulator).
//
// Nachgebaut ist nur, was die Sketches in diesem Repo tatsaechlich benutzen.
// Absicht ist NICHT eine Arduino-Emulation, sondern dass sich der unveraenderte
// Sketch-Code nativ uebersetzen laesst — damit der Simulator denselben
// Zeichencode und dieselben Fonts verwendet wie das Geraet. Ein Nachbau mit
// eigenen Schriftmetriken waere wertlos: Genau so ist in ha_verlauf eine
// 6-px-Ueberlappung im Prototyp unsichtbar geblieben, die auf dem Panel da war.

#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <string>
#include <ctime>
#include <sys/time.h>

// --- String -----------------------------------------------------------------

class String {
public:
  std::string s;

  String() {}
  String(const char* p) : s(p ? p : "") {}
  String(const std::string& v) : s(v) {}
  String(int v)          { char b[32]; snprintf(b, sizeof(b), "%d", v);  s = b; }
  String(unsigned v)     { char b[32]; snprintf(b, sizeof(b), "%u", v);  s = b; }
  String(long v)         { char b[32]; snprintf(b, sizeof(b), "%ld", v); s = b; }

  const char* c_str() const { return s.c_str(); }
  size_t length() const     { return s.size(); }

  int indexOf(const char* needle) const {
    const size_t p = s.find(needle);
    return p == std::string::npos ? -1 : (int)p;
  }
  int indexOf(char c, int from) const {
    const size_t p = s.find(c, (size_t)from);
    return p == std::string::npos ? -1 : (int)p;
  }
  String substring(int a, int b) const { return String(s.substr(a, b - a)); }

  String operator+(const String& o) const { return String(s + o.s); }
  String operator+(const char* o)   const { return String(s + (o ? o : "")); }
  String operator+(int v)           const { return *this + String(v); }
  String& operator+=(const String& o) { s += o.s; return *this; }
  String& operator+=(const char* o)   { s += (o ? o : ""); return *this; }
  bool operator==(const char* o) const { return s == (o ? o : ""); }
};

inline String operator+(const char* a, const String& b) { return String(a) + b; }

// --- Serial -----------------------------------------------------------------

// Landet auf stderr, damit stdout fuer die Bildausgabe frei bleibt.
struct SerialStub {
  void begin(unsigned long) {}
  void println()                 { fprintf(stderr, "\n"); }
  void println(const char* m)    { fprintf(stderr, "%s\n", m); }
  void print(const char* m)      { fprintf(stderr, "%s", m); }
  void printf(const char* f, ...) {
    va_list a; va_start(a, f); vfprintf(stderr, f, a); va_end(a);
  }
};
extern SerialStub Serial;

// --- GPIO und Zeit ----------------------------------------------------------

#define HIGH 1
#define LOW  0
#define INPUT  0
#define OUTPUT 1

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}
inline int  digitalRead(int) { return HIGH; }
inline void delay(unsigned long) {}
inline void delayMicroseconds(unsigned long) {}

inline unsigned long millis() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return (unsigned long)(tv.tv_sec * 1000UL + tv.tv_usec / 1000UL);
}

// Setzt die Zeitzone wirklich, damit localtime_r() dieselben Ortszeiten liefert
// wie auf dem Geraet — die Datumsachse haengt daran.
inline void configTzTime(const char* tz, const char*, const char* = nullptr,
                         const char* = nullptr) {
  setenv("TZ", tz, 1);
  tzset();
}
inline bool getLocalTime(struct tm* info, unsigned long = 0) {
  const time_t now = time(nullptr);
  localtime_r(&now, info);
  return true;
}

template <typename T> T max(T a, T b) { return a > b ? a : b; }
template <typename T> T min(T a, T b) { return a < b ? a : b; }
