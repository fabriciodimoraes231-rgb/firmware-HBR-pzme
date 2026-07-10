#include "wifi_manager.h"
#include "app_config.h"
#include <esp_task_wdt.h>

bool WifiManager::connect(const char *ssid, const char *pass) {
  Serial.printf("WiFi %s ... ", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED &&
         attempts < WIFI_CONNECT_MAX_ATTEMPTS) {
    esp_task_wdt_reset();
    delay(WIFI_CONNECT_RETRY_DELAY_MS);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" OK");
    Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }

  Serial.println(" FAIL - reiniciando em AP mode");
  return false;
}
