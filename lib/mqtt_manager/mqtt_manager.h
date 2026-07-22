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
  bool connect(const Config &cfg, const String &datetime);
  void loop();
  bool isConnected();
  void publishData(const Config &cfg, const PhaseData *phases,
                   float temperature, const String &datetime);
  void publishAlarm(const Config &cfg, const AlarmEvent &event,
                    const char *type, const String &datetime,
                    bool conditionActive);
  void publishStatus(const Config &cfg, bool online, const String &datetime);
  void publishConfig(const Config &cfg, const char *payload);
  void setConfigCallback(void (*cb)(const Config &));
  void setTimeCallback(void (*cb)(unsigned long));
  void setAlarmAckCallback(void (*cb)(bool all, AlarmType type,
                                      uint8_t phase));
  // Chamado logo antes do ESP.restart() disparado por uma config nova via
  // MQTT, para dar chance de fechar (RESOLVED) os alarmes ainda abertos -
  // senao o registro em RAM do AlarmManager e perdido no reboot sem o
  // frontend nunca receber o fechamento.
  void setPreRestartCallback(void (*cb)());

private:
  WiFiClient m_wifiClient;
  PubSubClient m_mqtt;
  Config m_lastCfg;
  bool m_configured;
  void (*m_configCb)(const Config &);
  void (*m_timeCb)(unsigned long);
  void (*m_ackCb)(bool all, AlarmType type, uint8_t phase);
  void (*m_preRestartCb)();

  static void callback(char *topic, byte *payload, unsigned int len);
  static MqttManager *m_instance;

  void handleConfig(const String &msg);
  void handleTime(const String &msg);
  void handleAlarmAck(const String &msg);
  String buildTopic(const Config &cfg, const char *suffix);
};
