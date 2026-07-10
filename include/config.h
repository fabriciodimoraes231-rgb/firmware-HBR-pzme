#pragma once
#include "app_config.h"
#include <cstdint>

struct Config {
  char wifi_ssid[32] = "";
  char wifi_pass[64] = "";

  char mqtt_host[64] = "";
  uint16_t mqtt_port = 1883;
  char mqtt_user[32] = "";
  char mqtt_pass[32] = "";
  char topic_prefix[64] = DEFAULT_MQTT_TOPIC_PREFIX;

  uint8_t num_phases = 1;
  float nominal_voltage = 127.0f;
  uint16_t send_interval_s = 5;
  uint16_t alarm_interval_s = 60;

  float v_max = 242.0f;
  float v_min = 198.0f;
  float a_max = 100.0f;
  float a_min = 0.5f;
  float temp_max = 80.0f;

  float v_factor[MAX_PHASES] = {1.0f, 1.0f, 1.0f};
  float a_factor[MAX_PHASES] = {1.0f, 1.0f, 1.0f};
  float w_factor[MAX_PHASES] = {1.0f, 1.0f, 1.0f};
};
