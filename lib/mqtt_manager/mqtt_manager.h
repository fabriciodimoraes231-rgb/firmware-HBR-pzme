#pragma once
#include "alarm_manager.h"
#include "config_manager.h"
#include "pzem_sensor.h"
#include "time_manager.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>

class MqttManager {
public:
  MqttManager();
  bool connect(const Config &cfg);
  void loop();
  bool isConnected();
  void publishData(const Config &cfg, const PhaseData *phases,
                   float temperature, const String &datetime);
  void publishAlarm(const Config &cfg, const AlarmEvent &event,
                    const char *type, const String &datetime);
  void publishStatus(const Config &cfg, bool online);
  void publishConfig(const Config &cfg, const char *payload);
  void setConfigCallback(void (*cb)(const Config &));
  void setTimeCallback(void (*cb)(unsigned long));

private:
  WiFiClient m_wifiClient;
  PubSubClient m_mqtt;
  Config m_lastCfg;
  bool m_configured;
  void (*m_configCb)(const Config &);
  void (*m_timeCb)(unsigned long);

  static void callback(char *topic, byte *payload, unsigned int len);
  static MqttManager *m_instance;

  void handleConfig(const String &msg);
  void handleTime(const String &msg);
  String buildTopic(const Config &cfg, const char *suffix);
};
