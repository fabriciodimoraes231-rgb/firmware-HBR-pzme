#include "MqttManager.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

MqttManager *MqttManager::m_instance = nullptr;

MqttManager::MqttManager()
  : m_mqtt(m_wifiClient), m_configured(false), m_configCb(nullptr), m_timeCb(nullptr) {
  m_instance = this;
}

bool MqttManager::connect(const Config &cfg) {
  m_lastCfg = cfg;
  m_mqtt.setServer(cfg.mqtt_host, cfg.mqtt_port);
  m_mqtt.setCallback(callback);
  m_mqtt.setKeepAlive(15);

  String clientId = "esp32_" + String((uint32_t)ESP.getEfuseMac(), HEX);

  String statusTopic = buildTopic(cfg, "status");
  String willPayload = "{\"status\":\"offline\"}";

  Serial.printf("MQTT %s:%d ... ", cfg.mqtt_host, cfg.mqtt_port);
  if (m_mqtt.connect(clientId.c_str(), cfg.mqtt_user, cfg.mqtt_pass,
                     statusTopic.c_str(), 1, true, willPayload.c_str())) {
    Serial.println("conectado");
    m_configured = true;

    String cfgTopic = buildTopic(cfg, "config");
    String timeTopic = buildTopic(cfg, "time");
    m_mqtt.subscribe(cfgTopic.c_str());
    m_mqtt.subscribe(timeTopic.c_str());

    publishStatus(cfg, true);
    return true;
  } else {
    Serial.printf("falhou (rc=%d)\n", m_mqtt.state());
    return false;
  }
}

void MqttManager::loop() {
  if (m_mqtt.connected()) {
    m_mqtt.loop();
  }
}

bool MqttManager::isConnected() {
  return m_mqtt.connected();
}

void MqttManager::setConfigCallback(void (*cb)(const Config &)) {
  m_configCb = cb;
}

void MqttManager::setTimeCallback(void (*cb)(unsigned long)) {
  m_timeCb = cb;
}

String MqttManager::buildTopic(const Config &cfg, const char *suffix) {
  String t = cfg.topic_prefix;
  if (t.length() > 0 && t.charAt(t.length() - 1) != '/') {
    t += "/";
  }
  t += suffix;
  return t;
}

void MqttManager::publishData(const Config &cfg, const PhaseData *phases,
                               float temperature, const String &datetime) {
  StaticJsonDocument<1024> doc;
  doc["datetime"] = datetime;
  doc["v_nominal"] = cfg.nominal_voltage;
  if (isnan(temperature)) {
    doc["temperature"] = nullptr;
  } else {
    doc["temperature"] = temperature;
  }

  const char *phaseNames[] = {"F1", "F2", "F3"};
  for (uint8_t i = 0; i < cfg.num_phases; i++) {
    JsonObject p = doc[phaseNames[i]].to<JsonObject>();
    if (phases[i].valid) {
      p["v"] = serialized(String(phases[i].v, 1));
      p["a"] = serialized(String(phases[i].a, 2));
      p["w"] = serialized(String(phases[i].w, 1));
      p["kwh"] = serialized(String(phases[i].kwh, 3));
      p["hz"] = serialized(String(phases[i].hz, 1));
      p["pf"] = serialized(String(phases[i].pf, 2));
      p["valid"] = true;
    } else {
      p["v"] = 0.0;
      p["a"] = 0.0;
      p["w"] = 0.0;
      p["kwh"] = 0.0;
      p["hz"] = 0.0;
      p["pf"] = 0.0;
      p["valid"] = false;
    }
  }

  String topic = buildTopic(cfg, "data");
  char buf[1536];
  size_t n = measureJson(doc);
  if (n >= sizeof(buf)) {
    Serial.println("MQTT data JSON too large, truncating");
    n = sizeof(buf) - 1;
  }
  serializeJson(doc, buf, sizeof(buf));
  m_mqtt.publish(topic.c_str(), buf, n);
}

void MqttManager::publishAlarm(const Config &cfg, const AlarmEvent &event,
                                const char *type, const String &datetime) {
  StaticJsonDocument<256> doc;
  doc["type"] = type;
  doc["alarm"] = AlarmManager::alarmName(event.type);
  doc["phase"] = event.phase;
  doc["datetime"] = datetime;

  if (event.type != ALARM_PHASE_LOSS) {
    doc["value"] = event.value;
    doc["threshold"] = event.threshold;
  }

  String topic = buildTopic(cfg, "alarm");
  char buf[256];
  size_t n = serializeJson(doc, buf);
  m_mqtt.publish(topic.c_str(), buf, n);
}

void MqttManager::publishConfig(const Config &cfg, const char *payload) {
  String topic = buildTopic(cfg, "config");
  m_mqtt.publish(topic.c_str(), payload);
  Serial.printf("MQTT publicado em %s\n", topic.c_str());
}

void MqttManager::publishStatus(const Config &cfg, bool online) {
  StaticJsonDocument<512> doc;
  doc["status"] = online ? "online" : "offline";
  if (online) {
    doc["ip"] = WiFi.localIP().toString();
  }
  doc["v_nominal"] = cfg.nominal_voltage;

  JsonObject limits = doc["limits"].to<JsonObject>();
  limits["v_max"] = cfg.v_max;
  limits["v_min"] = cfg.v_min;
  limits["a_max"] = cfg.a_max;
  limits["a_min"] = cfg.a_min;
  limits["temp_max"] = cfg.temp_max;

  String topic = buildTopic(cfg, "status");
  char buf[512];
  size_t n = serializeJson(doc, buf);
  m_mqtt.publish(topic.c_str(), (const uint8_t *)buf, n, true);
}

void MqttManager::callback(char *topic, byte *payload, unsigned int len) {
  if (!m_instance) return;

  String msg;
  for (unsigned i = 0; i < len; i++) {
    msg += (char)payload[i];
  }

  String t = String(topic);
  if (t.endsWith("/config")) {
    m_instance->handleConfig(msg);
  } else if (t.endsWith("/time")) {
    m_instance->handleTime(msg);
  }
}

void MqttManager::handleConfig(const String &msg) {
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, msg);
  if (err) return;

  if (doc.containsKey("status")) return;

  Config cfg = m_lastCfg;

  if (doc.containsKey("wifi_ssid")) {
    strlcpy(cfg.wifi_ssid, doc["wifi_ssid"], sizeof(cfg.wifi_ssid));
  }
  if (doc.containsKey("wifi_pass")) {
    strlcpy(cfg.wifi_pass, doc["wifi_pass"], sizeof(cfg.wifi_pass));
  }
  if (doc.containsKey("mqtt_host")) {
    strlcpy(cfg.mqtt_host, doc["mqtt_host"], sizeof(cfg.mqtt_host));
  }
  if (doc.containsKey("mqtt_port")) {
    cfg.mqtt_port = doc["mqtt_port"];
  }
  if (doc.containsKey("mqtt_user")) {
    strlcpy(cfg.mqtt_user, doc["mqtt_user"], sizeof(cfg.mqtt_user));
  }
  if (doc.containsKey("mqtt_pass")) {
    strlcpy(cfg.mqtt_pass, doc["mqtt_pass"], sizeof(cfg.mqtt_pass));
  }
  if (doc.containsKey("mqtt_topic_prefix")) {
    strlcpy(cfg.topic_prefix, doc["mqtt_topic_prefix"], sizeof(cfg.topic_prefix));
  }
  if (doc.containsKey("nominal_voltage")) {
    cfg.nominal_voltage = doc["nominal_voltage"];
  }
  if (doc.containsKey("send_interval_s")) {
    cfg.send_interval_s = doc["send_interval_s"];
    if (cfg.send_interval_s < 1) cfg.send_interval_s = 1;
    if (cfg.send_interval_s > 3600) cfg.send_interval_s = 3600;
  }
  if (doc.containsKey("alarm_interval_s")) {
    cfg.alarm_interval_s = doc["alarm_interval_s"];
    if (cfg.alarm_interval_s < 5) cfg.alarm_interval_s = 5;
    if (cfg.alarm_interval_s > 3600) cfg.alarm_interval_s = 3600;
  }
  if (doc.containsKey("v_max")) {
    cfg.v_max = doc["v_max"];
  }
  if (doc.containsKey("v_min")) {
    cfg.v_min = doc["v_min"];
  }
  if (doc.containsKey("a_max")) {
    cfg.a_max = doc["a_max"];
  }
  if (doc.containsKey("a_min")) {
    cfg.a_min = doc["a_min"];
  }
  if (doc.containsKey("temp_max")) {
    cfg.temp_max = doc["temp_max"];
  }

  if (doc.containsKey("v_factor")) {
    JsonArray arr = doc["v_factor"];
    for (uint8_t i = 0; i < MAX_PHASES && i < arr.size(); i++) {
      cfg.v_factor[i] = arr[i];
    }
  }
  if (doc.containsKey("a_factor")) {
    JsonArray arr = doc["a_factor"];
    for (uint8_t i = 0; i < MAX_PHASES && i < arr.size(); i++) {
      cfg.a_factor[i] = arr[i];
    }
  }
  if (doc.containsKey("w_factor")) {
    JsonArray arr = doc["w_factor"];
    for (uint8_t i = 0; i < MAX_PHASES && i < arr.size(); i++) {
      cfg.w_factor[i] = arr[i];
    }
  }

  ConfigManager cm;
  cm.save(cfg);

  if (m_configCb) {
    m_configCb(cfg);
  }

  m_lastCfg = cfg;

  String ts;
  {
    TimeManager tm;
    ts = tm.getTimestamp();
  }

  StaticJsonDocument<192> resp;
  resp["status"] = "ok";
  resp["message"] = "Config saved. Rebooting.";
  resp["timestamp"] = ts;

  String topic = buildTopic(cfg, "config");
  char buf[192];
  size_t n = serializeJson(resp, buf);
  m_mqtt.publish(topic.c_str(), buf, n);

  delay(500);
  ESP.restart();
}

void MqttManager::handleTime(const String &msg) {
  StaticJsonDocument<128> doc;

  if (msg.startsWith("{")) {
    DeserializationError err = deserializeJson(doc, msg);
    if (err) return;

    unsigned long epoch = 0;
    if (doc.containsKey("epoch")) {
      epoch = doc["epoch"];
    } else if (doc.containsKey("ts")) {
      epoch = doc["ts"];
    }

    if (epoch > 100000 && m_timeCb) {
      m_timeCb(epoch);
    }
  } else {
    struct tm ti;
    if (sscanf(msg.c_str(), "%d:%d %d/%d/%d", &ti.tm_hour, &ti.tm_min, &ti.tm_mday, &ti.tm_mon, &ti.tm_year) == 5) {
      ti.tm_mon -= 1;
      ti.tm_year -= 1900;
      ti.tm_sec = 0;
      ti.tm_isdst = -1;
      m_timeCb(mktime(&ti));
    }
  }
}

#pragma GCC diagnostic pop
