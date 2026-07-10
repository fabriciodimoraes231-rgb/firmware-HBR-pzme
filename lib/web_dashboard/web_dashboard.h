#pragma once
#include "alarm_manager.h"
#include "config_manager.h"
#include "mqtt_manager.h"
#include "phase_data.h"
#include "time_manager.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

class WebDashboard {
public:
  void begin(ConfigManager &configManager, AlarmManager &alarmManager,
             TimeManager &timeManager, MqttManager &mqttManager,
             Config &config, PhaseData *phases, float &currentTemp,
             AlarmState &alarmState);

private:
  AsyncWebServer m_server{80};

  ConfigManager *m_configManager = nullptr;
  AlarmManager *m_alarmManager = nullptr;
  TimeManager *m_timeManager = nullptr;
  MqttManager *m_mqttManager = nullptr;
  Config *m_config = nullptr;
  PhaseData *m_phases = nullptr;
  float *m_currentTemp = nullptr;
  AlarmState *m_alarmState = nullptr;

  String m_configBody;
  String m_mqttConfigBody;

  void handleData(AsyncWebServerRequest *r);
  void handleConfigPost(AsyncWebServerRequest *r);
  void handleMqttConfigPost(AsyncWebServerRequest *r);
};
