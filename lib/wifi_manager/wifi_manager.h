#pragma once
#include <Arduino.h>
#include <WiFi.h>

class WifiManager {
public:
  bool connect(const char *ssid, const char *pass);
  bool isConnected();
  void maintain(const char *ssid, const char *pass);

private:
  unsigned long m_lastReconnectAttempt = 0;
};
