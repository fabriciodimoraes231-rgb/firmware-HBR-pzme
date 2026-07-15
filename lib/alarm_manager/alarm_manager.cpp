#include "alarm_manager.h"
#include <cmath>
#include <cstring>

AlarmManager::AlarmManager() {
  resetState(m_open);
  resetState(m_newThisCycle);
}

void AlarmManager::resetState(AlarmState &state) {
  state.count = 0;
  for (uint8_t i = 0; i < MAX_ALARMS; i++) {
    state.active[i] = false;
    state.physicallyActive[i] = false;
  }
}

void AlarmManager::setAlarm(AlarmState &state, uint8_t idx, AlarmType type,
                            uint8_t phase, float value, float threshold) {
  if (idx >= MAX_ALARMS)
    return;
  state.events[idx].type = type;
  state.events[idx].phase = phase;
  state.events[idx].value = value;
  state.events[idx].threshold = threshold;
  state.active[idx] = true;
  state.physicallyActive[idx] = true;
  if (idx >= state.count)
    state.count = idx + 1;
}

const char *AlarmManager::alarmName(AlarmType type) {
  switch (type) {
  case ALARM_OVERVOLTAGE:
    return "OVERVOLTAGE";
  case ALARM_UNDERVOLTAGE:
    return "UNDERVOLTAGE";
  case ALARM_OVERCURRENT:
    return "OVERCURRENT";
  case ALARM_UNDERCURRENT:
    return "UNDERCURRENT";
  case ALARM_PHASE_LOSS:
    return "PHASE_LOSS";
  case ALARM_HIGH_TEMP:
    return "HIGH_TEMP";
  default:
    return "UNKNOWN";
  }
}

bool AlarmManager::alarmTypeFromName(const char *name, AlarmType &out) {
  if (strcmp(name, "OVERVOLTAGE") == 0) {
    out = ALARM_OVERVOLTAGE;
    return true;
  }
  if (strcmp(name, "UNDERVOLTAGE") == 0) {
    out = ALARM_UNDERVOLTAGE;
    return true;
  }
  if (strcmp(name, "OVERCURRENT") == 0) {
    out = ALARM_OVERCURRENT;
    return true;
  }
  if (strcmp(name, "UNDERCURRENT") == 0) {
    out = ALARM_UNDERCURRENT;
    return true;
  }
  if (strcmp(name, "PHASE_LOSS") == 0) {
    out = ALARM_PHASE_LOSS;
    return true;
  }
  if (strcmp(name, "HIGH_TEMP") == 0) {
    out = ALARM_HIGH_TEMP;
    return true;
  }
  return false;
}

static bool alarmMatch(const AlarmEvent &a, const AlarmEvent &b) {
  return a.type == b.type && a.phase == b.phase;
}

void AlarmManager::check(const PhaseData *phases, uint8_t numPhases,
                         float temperature, const Config &cfg,
                         AlarmState &state) {
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

  if (!std::isnan(temperature) && temperature > cfg.temp_max) {
    setAlarm(state, idx++, ALARM_HIGH_TEMP, 0, temperature, cfg.temp_max);
  }

  reconcileOpenAlarms(state);
}

void AlarmManager::reconcileOpenAlarms(const AlarmState &physical) {
  resetState(m_newThisCycle);
  uint8_t newIdx = 0;
  bool matched[MAX_ALARMS] = {false};

  for (uint8_t i = 0; i < physical.count; i++) {
    if (!physical.active[i])
      continue;

    bool found = false;
    for (uint8_t j = 0; j < m_open.count; j++) {
      if (m_open.active[j] &&
          alarmMatch(physical.events[i], m_open.events[j])) {
        m_open.events[j] = physical.events[i];
        m_open.physicallyActive[j] = true;
        matched[j] = true;
        found = true;
        break;
      }
    }

    if (!found && m_open.count < MAX_ALARMS) {
      uint8_t slot = m_open.count;
      m_open.events[slot] = physical.events[i];
      m_open.active[slot] = true;
      m_open.physicallyActive[slot] = true;
      m_open.count = slot + 1;
      matched[slot] = true;

      m_newThisCycle.events[newIdx] = physical.events[i];
      m_newThisCycle.active[newIdx] = true;
      m_newThisCycle.physicallyActive[newIdx] = true;
      m_newThisCycle.count = newIdx + 1;
      newIdx++;
    }
  }

  // Quem estava aberto mas nao apareceu no snapshot fisico deste ciclo:
  // a condicao normalizou, mas o alarme continua aberto ate vir o ack.
  for (uint8_t j = 0; j < m_open.count; j++) {
    if (m_open.active[j] && !matched[j]) {
      m_open.physicallyActive[j] = false;
    }
  }
}

void AlarmManager::getNewAlarms(AlarmState &newAlarms) {
  newAlarms = m_newThisCycle;
}

void AlarmManager::getRefreshAlarms(AlarmState &refresh) { refresh = m_open; }

void AlarmManager::removeOpenSlot(uint8_t idx) {
  uint8_t last = m_open.count - 1;
  if (idx != last) {
    m_open.events[idx] = m_open.events[last];
    m_open.active[idx] = m_open.active[last];
    m_open.physicallyActive[idx] = m_open.physicallyActive[last];
  }
  m_open.active[last] = false;
  m_open.physicallyActive[last] = false;
  m_open.count = last;
}

bool AlarmManager::resolve(AlarmType type, uint8_t phase,
                           AlarmState &resolved) {
  resetState(resolved);

  for (uint8_t j = 0; j < m_open.count; j++) {
    if (!m_open.active[j])
      continue;
    if (m_open.events[j].type == type && m_open.events[j].phase == phase) {
      resolved.events[0] = m_open.events[j];
      resolved.active[0] = true;
      resolved.physicallyActive[0] = m_open.physicallyActive[j];
      resolved.count = 1;
      removeOpenSlot(j);
      return true;
    }
  }
  return false;
}

void AlarmManager::resolveAll(AlarmState &resolved) {
  resolved = m_open;
  resetState(m_open);
}
