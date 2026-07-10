#include "offline_buffer.h"
#include <cstring>

void OfflineBuffer::pushSample(const PhaseData *phases, float temperature,
                               const String &datetime) {
  BufferedSample &slot = m_samples[m_sampleHead];
  strlcpy(slot.datetime, datetime.c_str(), sizeof(slot.datetime));
  slot.temperature = temperature;
  for (uint8_t i = 0; i < MAX_PHASES; i++) {
    slot.phases[i] = phases[i];
  }

  m_sampleHead = (m_sampleHead + 1) % OFFLINE_BUFFER_SAMPLES;
  if (m_sampleCount == OFFLINE_BUFFER_SAMPLES) {
    m_sampleTail = (m_sampleTail + 1) % OFFLINE_BUFFER_SAMPLES; // descarta mais antigo
  } else {
    m_sampleCount++;
  }
}

bool OfflineBuffer::popSample(BufferedSample &out) {
  if (m_sampleCount == 0)
    return false;

  out = m_samples[m_sampleTail];
  m_sampleTail = (m_sampleTail + 1) % OFFLINE_BUFFER_SAMPLES;
  m_sampleCount--;
  return true;
}

bool OfflineBuffer::hasSamples() const { return m_sampleCount > 0; }

size_t OfflineBuffer::sampleCount() const { return m_sampleCount; }

void OfflineBuffer::pushAlarmEvent(const AlarmEvent &event, const char *type,
                                   const String &datetime) {
  BufferedAlarmEvent &slot = m_alarmEvents[m_alarmHead];
  strlcpy(slot.datetime, datetime.c_str(), sizeof(slot.datetime));
  strlcpy(slot.type, type, sizeof(slot.type));
  slot.event = event;

  m_alarmHead = (m_alarmHead + 1) % OFFLINE_BUFFER_ALARM_EVENTS;
  if (m_alarmCount == OFFLINE_BUFFER_ALARM_EVENTS) {
    m_alarmTail = (m_alarmTail + 1) % OFFLINE_BUFFER_ALARM_EVENTS;
  } else {
    m_alarmCount++;
  }
}

bool OfflineBuffer::popAlarmEvent(BufferedAlarmEvent &out) {
  if (m_alarmCount == 0)
    return false;

  out = m_alarmEvents[m_alarmTail];
  m_alarmTail = (m_alarmTail + 1) % OFFLINE_BUFFER_ALARM_EVENTS;
  m_alarmCount--;
  return true;
}

bool OfflineBuffer::hasAlarmEvents() const { return m_alarmCount > 0; }

size_t OfflineBuffer::alarmEventCount() const { return m_alarmCount; }
