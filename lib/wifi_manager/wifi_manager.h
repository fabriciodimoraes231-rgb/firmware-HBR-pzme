#pragma once
#include <Arduino.h>
#include <WiFi.h>

class WifiManager {
public:
  bool connect(const char *ssid, const char *pass);
};
