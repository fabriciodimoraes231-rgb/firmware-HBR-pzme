#include "ConfigManager.h"

ConfigManager::ConfigManager() {}

void ConfigManager::load(Config &cfg) {
  prefs.begin("pzem", true);

  prefs.getString("wifi_ssid", cfg.wifi_ssid, sizeof(cfg.wifi_ssid));
  prefs.getString("wifi_pass", cfg.wifi_pass, sizeof(cfg.wifi_pass));

  prefs.getString("mqtt_host", cfg.mqtt_host, sizeof(cfg.mqtt_host));
  cfg.mqtt_port = prefs.getUShort("mqtt_port", 1883);
  prefs.getString("mqtt_user", cfg.mqtt_user, sizeof(cfg.mqtt_user));
  prefs.getString("mqtt_pass", cfg.mqtt_pass, sizeof(cfg.mqtt_pass));
  prefs.getString("topic_prefix", cfg.topic_prefix, sizeof(cfg.topic_prefix));

  cfg.num_phases = prefs.getUChar("num_phases", 1);
  if (cfg.num_phases < 1) cfg.num_phases = 1;
  if (cfg.num_phases > MAX_PHASES) cfg.num_phases = MAX_PHASES;
  cfg.nominal_voltage = prefs.getFloat("nominal_voltage", 127.0f);
  cfg.send_interval_s = prefs.getUShort("send_interval_s", 5);
  if (cfg.send_interval_s < 1) cfg.send_interval_s = 1;
  if (cfg.send_interval_s > 3600) cfg.send_interval_s = 3600;

  cfg.alarm_interval_s = prefs.getUShort("alarm_interval_s", 60);
  if (cfg.alarm_interval_s < 5) cfg.alarm_interval_s = 5;
  if (cfg.alarm_interval_s > 3600) cfg.alarm_interval_s = 3600;

  cfg.v_max = prefs.getFloat("v_max", 242.0f);
  cfg.v_min = prefs.getFloat("v_min", 198.0f);
  cfg.a_max = prefs.getFloat("a_max", 100.0f);
  cfg.a_min = prefs.getFloat("a_min", 0.5f);
  cfg.temp_max = prefs.getFloat("temp_max", 80.0f);

  for (uint8_t i = 0; i < MAX_PHASES; i++) {
    char key[16];
    sprintf(key, "v_factor_%d", i);
    cfg.v_factor[i] = prefs.getFloat(key, 1.0f);

    sprintf(key, "a_factor_%d", i);
    cfg.a_factor[i] = prefs.getFloat(key, 1.0f);

    sprintf(key, "w_factor_%d", i);
    cfg.w_factor[i] = prefs.getFloat(key, 1.0f);
  }

  prefs.end();
}

void ConfigManager::save(const Config &cfg) {
  prefs.begin("pzem", false);

  prefs.putString("wifi_ssid", cfg.wifi_ssid);
  prefs.putString("wifi_pass", cfg.wifi_pass);

  prefs.putString("mqtt_host", cfg.mqtt_host);
  prefs.putUShort("mqtt_port", cfg.mqtt_port);
  prefs.putString("mqtt_user", cfg.mqtt_user);
  prefs.putString("mqtt_pass", cfg.mqtt_pass);
  prefs.putString("topic_prefix", cfg.topic_prefix);

  prefs.putUChar("num_phases", cfg.num_phases);
  prefs.putFloat("nominal_voltage", cfg.nominal_voltage);
  prefs.putUShort("send_interval_s", cfg.send_interval_s);
  prefs.putUShort("alarm_interval_s", cfg.alarm_interval_s);

  prefs.putFloat("v_max", cfg.v_max);
  prefs.putFloat("v_min", cfg.v_min);
  prefs.putFloat("a_max", cfg.a_max);
  prefs.putFloat("a_min", cfg.a_min);
  prefs.putFloat("temp_max", cfg.temp_max);

  for (uint8_t i = 0; i < MAX_PHASES; i++) {
    char key[16];
    sprintf(key, "v_factor_%d", i);
    prefs.putFloat(key, cfg.v_factor[i]);

    sprintf(key, "a_factor_%d", i);
    prefs.putFloat(key, cfg.a_factor[i]);

    sprintf(key, "w_factor_%d", i);
    prefs.putFloat(key, cfg.w_factor[i]);
  }

  prefs.end();
}

void ConfigManager::reset() {
  prefs.begin("pzem", false);
  prefs.clear();
  prefs.end();
}

uint32_t ConfigManager::getBootCount() {
  prefs.begin("pzem", true);
  uint32_t count = prefs.getUInt("boot_count", 0);
  prefs.end();
  return count;
}

void ConfigManager::incrementBootCount() {
  prefs.begin("pzem", false);
  uint32_t count = prefs.getUInt("boot_count", 0);
  prefs.putUInt("boot_count", count + 1);
  prefs.end();
}
