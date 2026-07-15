#pragma once
#include "config.h"
#include "phase_data.h"
#include <cstdint>

// Teto de combinacoes distintas (tipo, fase) que podem existir simultaneamente
// abertas: 5 tipos por fase (OVERVOLTAGE/UNDERVOLTAGE/OVERCURRENT/
// UNDERCURRENT/PHASE_LOSS) x 3 fases + HIGH_TEMP = 16. Como um alarme so sai
// do registro via ack (nao se auto-limpa mais), esse teto deixou de ser
// hipotetico e precisa cobrir o pior caso real.
constexpr uint8_t MAX_ALARMS = 16;

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
  bool physicallyActive[MAX_ALARMS];
  AlarmEvent events[MAX_ALARMS];
  uint8_t count;
};

class AlarmManager {
public:
  AlarmManager();

  // Calcula o snapshot fisico do ciclo (o que realmente esta fora do limite
  // agora) e reconcilia contra o registro persistente de alarmes abertos.
  void check(const PhaseData *phases, uint8_t numPhases, float temperature,
             const Config &cfg, AlarmState &state);

  // Alarmes que abriram neste ciclo (transicao fechado -> aberto).
  void getNewAlarms(AlarmState &newAlarms);

  // Todos os alarmes abertos (aguardando ack), ativos fisicamente ou nao.
  // Usado para o ALARM_REFRESH periodico.
  void getRefreshAlarms(AlarmState &refresh);

  // Fecha um alarme aberto especifico (tipo+fase) via ack externo.
  // Devolve o evento fechado em `resolved` (count 0 ou 1).
  bool resolve(AlarmType type, uint8_t phase, AlarmState &resolved);

  // Fecha todos os alarmes abertos via ack "all". Devolve tudo que estava
  // aberto em `resolved`.
  void resolveAll(AlarmState &resolved);

  static const char *alarmName(AlarmType type);
  static bool alarmTypeFromName(const char *name, AlarmType &out);

private:
  AlarmState m_open;
  AlarmState m_newThisCycle;

  void resetState(AlarmState &state);
  void setAlarm(AlarmState &state, uint8_t idx, AlarmType type, uint8_t phase,
                float value, float threshold);
  void reconcileOpenAlarms(const AlarmState &physical);
  void removeOpenSlot(uint8_t idx);
};
