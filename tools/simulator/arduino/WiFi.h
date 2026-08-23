// WiFi-Ersatz fuer den Host-Build. Tut so, als sei die Verbindung da — der
// Simulator holt seine Daten aus lokalen Dateien (siehe HTTPClient.h).
#pragma once
#include <Arduino.h>

#define WL_CONNECTED 3
#define WIFI_STA     1

struct IPAddressStub {
  String toString() const { return String("192.168.178.99"); }
};

struct WiFiStub {
  int  status() const { return WL_CONNECTED; }
  void mode(int) {}
  void begin(const char*, const char*) {}
  IPAddressStub localIP() const { return IPAddressStub(); }
};
extern WiFiStub WiFi;
