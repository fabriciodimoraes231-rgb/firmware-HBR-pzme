#include "AlarmManager.h"

AlarmManager::AlarmManager() {
  resetState(m_previous);
}

void AlarmManager::resetState(AlarmState &state) {
  state.count = 0;
  for (uint8_t i = 0; i < MAX_ALARMS; i++) {
    state.active[i] = false;
  }
}

void AlarmManager::setAlarm(AlarmState &state, uint8_t idx, AlarmType type,
                             uint8_t phase, float value, float threshold) {
  if (idx >= MAX_ALARMS) return;
  state.events[idx].type = type;
  state.events[idx].phase = phase;
  state.events[idx].value = value;
  state.events[idx].threshold = threshold;
  state.active[idx] = true;
  if (idx >= state.count) state.count = idx + 1;
}

const char *AlarmManager::alarmName(AlarmType type) {
  switch (type) {
    case ALARM_OVERVOLTAGE:  return "OVERVOLTAGE";
    case ALARM_UNDERVOLTAGE: return "UNDERVOLTAGE";
    case ALARM_OVERCURRENT:  return "OVERCURRENT";
    case ALARM_UNDERCURRENT: return "UNDERCURRENT";
    case ALARM_PHASE_LOSS:   return "PHASE_LOSS";
    case ALARM_HIGH_TEMP:    return "HIGH_TEMP";
    default:                 return "UNKNOWN";
  }
}

void AlarmManager::check(const PhaseData *phases, uint8_t numPhases,
                          float temperature, const Config &cfg, AlarmState &state) {
  resetState(state);
  uint8_t idx = 0;

  for (uint8_t i = 0; i < numPhases; i++) {
    const auto &p = phases[i];
    uint8_t phaseId = i + 1;

    if (!p.valid) {
      if (cfg.v_min > 0) {
        setAlarm(state, idx++, ALARM_PHASE_LOSS, phaseId, 0.0f, cfg.v_min);
      }
      continue;
    }

    if (p.v > cfg.v_max) {
      setAlarm(state, idx++, ALARM_OVERVOLTAGE, phaseId, p.v, cfg.v_max);
    }

    if (p.v < cfg.v_min) {
      setAlarm(state, idx++, ALARM_UNDERVOLTAGE, phaseId, p.v, cfg.v_min);
    }

    if (p.a > cfg.a_max) {
      setAlarm(state, idx++, ALARM_OVERCURRENT, phaseId, p.a, cfg.a_max);
    }

    if (p.a > 0 && p.a < cfg.a_min) {
      setAlarm(state, idx++, ALARM_UNDERCURRENT, phaseId, p.a, cfg.a_min);
    }
  }

  if (!isnan(temperature) && temperature > cfg.temp_max) {
    setAlarm(state, idx++, ALARM_HIGH_TEMP, 0, temperature, cfg.temp_max);
  }

  m_previous = state;
}

bool alarmMatch(const AlarmEvent &a, const AlarmEvent &b) {
  return a.type == b.type && a.phase == b.phase;
}

void AlarmManager::getNewAlarms(const AlarmState &current, AlarmState &newAlarms) {
  resetState(newAlarms);
  uint8_t idx = 0;

  for (uint8_t i = 0; i < current.count; i++) {
    if (!current.active[i]) continue;

    bool found = false;
    for (uint8_t j = 0; j < m_previous.count; j++) {
      if (m_previous.active[j] && alarmMatch(current.events[i], m_previous.events[j])) {
        found = true;
        break;
      }
    }

    if (!found) {
      newAlarms.events[idx] = current.events[i];
      newAlarms.active[idx] = true;
      if (idx >= newAlarms.count) newAlarms.count = idx + 1;
      idx++;
    }
  }
}

void AlarmManager::getResolvedAlarms(const AlarmState &current, AlarmState &resolved) {
  resetState(resolved);
  uint8_t idx = 0;

  for (uint8_t i = 0; i < m_previous.count; i++) {
    if (!m_previous.active[i]) continue;

    bool stillActive = false;
    for (uint8_t j = 0; j < current.count; j++) {
      if (current.active[j] && alarmMatch(m_previous.events[i], current.events[j])) {
        stillActive = true;
        break;
      }
    }

    if (!stillActive) {
      resolved.events[idx] = m_previous.events[i];
      resolved.active[idx] = true;
      if (idx >= resolved.count) resolved.count = idx + 1;
      idx++;
    }
  }
}

void AlarmManager::getActiveAlarms(const AlarmState &current, AlarmState &active) {
  resetState(active);
  uint8_t idx = 0;

  for (uint8_t i = 0; i < current.count; i++) {
    if (current.active[i]) {
      active.events[idx] = current.events[i];
      active.active[idx] = true;
      if (idx >= active.count) active.count = idx + 1;
      idx++;
    }
  }
}
