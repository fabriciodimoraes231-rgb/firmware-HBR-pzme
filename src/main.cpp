#include "aht10_sensor.h"
#include "alarm_manager.h"
#include "app_config.h"
#include "captive_portal.h"
#include "config_manager.h"
#include "mqtt_manager.h"
#include "pzem_sensor.h"
#include "time_manager.h"
#include "web_dashboard.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include <esp_task_wdt.h>

ConfigManager configManager;
PzemSensor pzemSensor;
Aht10Sensor aht10Sensor;
AlarmManager alarmManager;
TimeManager timeManager;
MqttManager mqttManager;
WifiManager wifiManager;
WebDashboard webDashboard;

Config config;
PhaseData phases[MAX_PHASES];
AlarmState alarmState;
AlarmState newAlarms;
AlarmState resolvedAlarms;
AlarmState activeAlarms;

unsigned long lastRead = 0;
unsigned long lastSend = 0;
unsigned long lastAlarmSend = 0;
unsigned long lastMqttTry = 0;

bool shouldRebootAp = false;
float currentTemp = NAN;

void checkGpioBoot() {
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  unsigned long start = millis();
  bool held = true;
  while (millis() - start < BOOT_AP_HOLD_MS) {
    if (digitalRead(BOOT_BUTTON_PIN) != LOW) {
      held = false;
    }
    delay(50);
  }
  if (held) {
    Serial.println("GPIO0 held >3s at boot -> AP mode");
    shouldRebootAp = true;
  }
}

void checkGpioRuntime() {
  static bool holding = false;
  static unsigned long holdStart = 0;

  bool pressed = digitalRead(BOOT_BUTTON_PIN) == LOW;

  if (pressed && !holding) {
    holding = true;
    holdStart = millis();
  } else if (!pressed) {
    holding = false;
  }

  if (holding && millis() - holdStart >= GPIO_HOLD_MS) {
    Serial.println("GPIO0 held >5s -> reset config + AP mode");
    configManager.reset();
    ESP.restart();
  }
}

void onConfigUpdated(const Config &cfg) { config = cfg; }

void onTimeUpdated(unsigned long epoch) { timeManager.setFromEpoch(epoch); }

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Vigiatech Monitor Trifasico v2.0 ===");

  configManager.load(config);
  configManager.incrementBootCount();

  Serial.println("--- CONFIG ---");
  Serial.printf("wifi_ssid: %s\n", config.wifi_ssid);
  Serial.printf("mqtt_host: %s\n", config.mqtt_host);
  Serial.printf("mqtt_port: %d\n", config.mqtt_port);
  Serial.printf("topic_prefix: %s\n", config.topic_prefix);
  Serial.printf("num_phases: %d\n", config.num_phases);
  Serial.printf("nominal_voltage: %.1f\n", config.nominal_voltage);
  Serial.printf("send_interval_s: %d\n", config.send_interval_s);
  Serial.printf("alarm_interval_s: %d\n", config.alarm_interval_s);
  Serial.printf("v_max: %.1f v_min: %.1f\n", config.v_max, config.v_min);
  Serial.printf("a_max: %.1f a_min: %.1f\n", config.a_max, config.a_min);
  Serial.printf("boot_count: %d\n", configManager.getBootCount());
  Serial.println("----------------------");

  checkGpioBoot();

  bool hasConfig = strlen(config.wifi_ssid) > 0;

  if (shouldRebootAp || !hasConfig) {
    Serial.println("Iniciando Captive Portal...");
    CaptivePortal portal;
    portal.start(configManager);
    return;
  }

  if (wifiManager.connect(config.wifi_ssid, config.wifi_pass)) {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
  } else {
    delay(1000);
    configManager.reset();
    CaptivePortal portal;
    portal.start(configManager);
    return;
  }

  esp_task_wdt_init(WDT_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);

  webDashboard.begin(configManager, alarmManager, timeManager, mqttManager,
                     config, phases, currentTemp, alarmState);

  timeManager.init();

  mqttManager.setConfigCallback(onConfigUpdated);
  mqttManager.setTimeCallback(onTimeUpdated);
  mqttManager.connect(config);
  pzemSensor.init();
  aht10Sensor.init();
}

void loop() {
  esp_task_wdt_reset();

  unsigned long now = millis();

  wifiManager.maintain(config.wifi_ssid, config.wifi_pass);
  digitalWrite(LED_PIN, wifiManager.isConnected() ? HIGH : LOW);

  mqttManager.loop();

  if (wifiManager.isConnected() && !mqttManager.isConnected()) {
    if (now - lastMqttTry >= MQTT_RETRY_MS) {
      lastMqttTry = now;
      mqttManager.connect(config);
    }
  }

  checkGpioRuntime();

  if (now - lastRead >= config.send_interval_s * 1000) {
    lastRead = now;
    pzemSensor.read(phases, config.num_phases, config);
    currentTemp = aht10Sensor.getTemperature();
    for (uint8_t i = 0; i < config.num_phases; i++) {
      phases[i].temperature = currentTemp;
    }
  }

  if (now - lastSend >= config.send_interval_s * 1000) {
    lastSend = now;
    if (mqttManager.isConnected()) {
      String ts = timeManager.getTimestamp();
      mqttManager.publishData(config, phases, currentTemp, ts);
    }
  }

  if (mqttManager.isConnected()) {
    alarmManager.check(phases, config.num_phases, currentTemp, config,
                       alarmState);

    alarmManager.getNewAlarms(alarmState, newAlarms);
    for (uint8_t i = 0; i < newAlarms.count; i++) {
      if (newAlarms.active[i]) {
        mqttManager.publishAlarm(config, newAlarms.events[i], "ALARM",
                                 timeManager.getTimestamp());
      }
    }

    alarmManager.getResolvedAlarms(alarmState, resolvedAlarms);
    for (uint8_t i = 0; i < resolvedAlarms.count; i++) {
      if (resolvedAlarms.active[i]) {
        mqttManager.publishAlarm(config, resolvedAlarms.events[i], "RESOLVED",
                                 timeManager.getTimestamp());
      }
    }

    if (now - lastAlarmSend >= config.alarm_interval_s * 1000) {
      lastAlarmSend = now;
      alarmManager.getActiveAlarms(alarmState, activeAlarms);
      for (uint8_t i = 0; i < activeAlarms.count; i++) {
        if (activeAlarms.active[i]) {
          mqttManager.publishAlarm(config, activeAlarms.events[i],
                                   "ALARM_REFRESH", timeManager.getTimestamp());
        }
      }
    }
  }
}
