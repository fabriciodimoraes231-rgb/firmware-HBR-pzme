#pragma once
#include "alarm_manager.h"
#include "app_config.h"
#include "phase_data.h"
#include <Arduino.h>
#include <cstddef>
#include <cstdint>

constexpr size_t OFFLINE_BUFFER_SAMPLES = 360;      // ~30 min a 5s/leitura
constexpr size_t OFFLINE_BUFFER_ALARM_EVENTS = 30;
constexpr size_t OFFLINE_DATETIME_LEN = 20;
constexpr uint8_t OFFLINE_FLUSH_BATCH = 5; // amostras drenadas por chamada

struct BufferedSample {
  char datetime[OFFLINE_DATETIME_LEN];
  float temperature;
  PhaseData phases[MAX_PHASES];
};

struct BufferedAlarmEvent {
  char datetime[OFFLINE_DATETIME_LEN];
  char type[12]; // "ALARM" ou "RESOLVED"
  AlarmEvent event;
};

// Fila circular em RAM para segurar leituras/alarmes enquanto o MQTT esta
// desconectado. Quando cheia, descarta a entrada mais antiga (mantem os
// dados mais recentes). Nao sobrevive a reboot/queda de energia.
class OfflineBuffer {
public:
  void pushSample(const PhaseData *phases, float temperature,
                  const String &datetime);
  bool popSample(BufferedSample &out);
  bool hasSamples() const;
  size_t sampleCount() const;

  void pushAlarmEvent(const AlarmEvent &event, const char *type,
                      const String &datetime);
  bool popAlarmEvent(BufferedAlarmEvent &out);
  bool hasAlarmEvents() const;
  size_t alarmEventCount() const;

private:
  BufferedSample m_samples[OFFLINE_BUFFER_SAMPLES];
  size_t m_sampleHead = 0;
  size_t m_sampleTail = 0;
  size_t m_sampleCount = 0;

  BufferedAlarmEvent m_alarmEvents[OFFLINE_BUFFER_ALARM_EVENTS];
  size_t m_alarmHead = 0;
  size_t m_alarmTail = 0;
  size_t m_alarmCount = 0;
};
