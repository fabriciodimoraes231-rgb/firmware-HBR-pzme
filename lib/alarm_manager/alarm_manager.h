#pragma once
#include "config.h"
#include "phase_data.h"
#include <cstdint>

constexpr uint8_t MAX_ALARMS = 12;

enum AlarmType : uint8_t {
  ALARM_OVERVOLTAGE,
  ALARM_UNDERVOLTAGE,
  ALARM_OVERCURRENT,
  ALARM_UNDERCURRENT,
  ALARM_PHASE_LOSS,
  ALARM_HIGH_TEMP
};

struct AlarmEvent {
  AlarmType type;
  uint8_t phase;
  float value;
  float threshold;
};

struct AlarmState {
  bool active[MAX_ALARMS];
  AlarmEvent events[MAX_ALARMS];
  uint8_t count;
};

class AlarmManager {
public:
  AlarmManager();
  void check(const PhaseData *phases, uint8_t numPhases, float temperature,
             const Config &cfg, AlarmState &state);
  void getNewAlarms(const AlarmState &current, AlarmState &newAlarms);
  void getResolvedAlarms(const AlarmState &current, AlarmState &resolved);
  void getActiveAlarms(const AlarmState &current, AlarmState &active);
  static const char *alarmName(AlarmType type);

private:
  AlarmState m_previous;
  AlarmState m_lastComputed;
  void resetState(AlarmState &state);
  void setAlarm(AlarmState &state, uint8_t idx, AlarmType type, uint8_t phase,
                float value, float threshold);
};
