#pragma once
#include "config_manager.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

class CaptivePortal {
public:
  void start(ConfigManager &cfgMgr);

private:
  static ConfigManager *m_cfgMgr;
  static AsyncWebServer m_server;

  static void handleRoot(AsyncWebServerRequest *request);
  static void handleSave(AsyncWebServerRequest *request);
  static void handleReset(AsyncWebServerRequest *request);
  static String buildHtml();
};
