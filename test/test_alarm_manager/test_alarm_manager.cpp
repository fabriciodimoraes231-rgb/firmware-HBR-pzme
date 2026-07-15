#include "alarm_manager.h"
#include <cmath>
#include <unity.h>

namespace {

Config makeConfig() {
  Config cfg;
  cfg.num_phases = 1;
  cfg.v_max = 242.0f;
  cfg.v_min = 198.0f;
  cfg.a_max = 100.0f;
  cfg.a_min = 0.5f;
  cfg.temp_max = 80.0f;
  return cfg;
}

PhaseData makeValidPhase(float v, float a) {
  PhaseData p;
  p.valid = true;
  p.v = v;
  p.a = a;
  p.w = v * a;
  p.hz = 60.0f;
  p.pf = 1.0f;
  return p;
}

} // namespace

void setUp(void) {}
void tearDown(void) {}

void test_no_alarm_when_within_limits(void) {
  AlarmManager alarm;
  Config cfg = makeConfig();
  PhaseData phases[1] = {makeValidPhase(220.0f, 10.0f)};
  AlarmState state;

  alarm.check(phases, 1, 25.0f, cfg, state);

  TEST_ASSERT_EQUAL_UINT8(0, state.count);
}

void test_overvoltage_triggers_alarm(void) {
  AlarmManager alarm;
  Config cfg = makeConfig();
  PhaseData phases[1] = {makeValidPhase(250.0f, 10.0f)};
  AlarmState state;

  alarm.check(phases, 1, 25.0f, cfg, state);

  TEST_ASSERT_EQUAL_UINT8(1, state.count);
  TEST_ASSERT_TRUE(state.active[0]);
  TEST_ASSERT_EQUAL(ALARM_OVERVOLTAGE, state.events[0].type);
  TEST_ASSERT_EQUAL_UINT8(1, state.events[0].phase);
  TEST_ASSERT_EQUAL_FLOAT(250.0f, state.events[0].value);
  TEST_ASSERT_EQUAL_FLOAT(242.0f, state.events[0].threshold);
}

void test_undervoltage_triggers_alarm(void) {
  AlarmManager alarm;
  Config cfg = makeConfig();
  PhaseData phases[1] = {makeValidPhase(150.0f, 10.0f)};
  AlarmState state;

  alarm.check(phases, 1, 25.0f, cfg, state);

  TEST_ASSERT_EQUAL_UINT8(1, state.count);
  TEST_ASSERT_EQUAL(ALARM_UNDERVOLTAGE, state.events[0].type);
}

void test_overcurrent_triggers_alarm(void) {
  AlarmManager alarm;
  Config cfg = makeConfig();
  PhaseData phases[1] = {makeValidPhase(220.0f, 150.0f)};
  AlarmState state;

  alarm.check(phases, 1, 25.0f, cfg, state);

  TEST_ASSERT_EQUAL_UINT8(1, state.count);
  TEST_ASSERT_EQUAL(ALARM_OVERCURRENT, state.events[0].type);
}

void test_undercurrent_triggers_alarm(void) {
  AlarmManager alarm;
  Config cfg = makeConfig();
  PhaseData phases[1] = {makeValidPhase(220.0f, 0.1f)};
  AlarmState state;

  alarm.check(phases, 1, 25.0f, cfg, state);

  TEST_ASSERT_EQUAL_UINT8(1, state.count);
  TEST_ASSERT_EQUAL(ALARM_UNDERCURRENT, state.events[0].type);
}

void test_zero_current_does_not_trigger_undercurrent(void) {
  // a == 0 e tratado como "sem carga", nao como falta de corrente (ver AlarmManager::check).
  AlarmManager alarm;
  Config cfg = makeConfig();
  PhaseData phases[1] = {makeValidPhase(220.0f, 0.0f)};
  AlarmState state;

  alarm.check(phases, 1, 25.0f, cfg, state);

  TEST_ASSERT_EQUAL_UINT8(0, state.count);
}

void test_phase_loss_triggers_alarm(void) {
  AlarmManager alarm;
  Config cfg = makeConfig();
  PhaseData phases[1];
  phases[0].valid = false;
  AlarmState state;

  alarm.check(phases, 1, 25.0f, cfg, state);

  TEST_ASSERT_EQUAL_UINT8(1, state.count);
  TEST_ASSERT_EQUAL(ALARM_PHASE_LOSS, state.events[0].type);
}

void test_high_temperature_triggers_alarm(void) {
  AlarmManager alarm;
  Config cfg = makeConfig();
  PhaseData phases[1] = {makeValidPhase(220.0f, 10.0f)};
  AlarmState state;

  alarm.check(phases, 1, 95.0f, cfg, state);

  TEST_ASSERT_EQUAL_UINT8(1, state.count);
  TEST_ASSERT_EQUAL(ALARM_HIGH_TEMP, state.events[0].type);
  TEST_ASSERT_EQUAL_UINT8(0, state.events[0].phase);
}

void test_nan_temperature_does_not_trigger_alarm(void) {
  AlarmManager alarm;
  Config cfg = makeConfig();
  PhaseData phases[1] = {makeValidPhase(220.0f, 10.0f)};
  AlarmState state;

  alarm.check(phases, 1, NAN, cfg, state);

  TEST_ASSERT_EQUAL_UINT8(0, state.count);
}

void test_alarm_name_strings(void) {
  TEST_ASSERT_EQUAL_STRING("OVERVOLTAGE",
                           AlarmManager::alarmName(ALARM_OVERVOLTAGE));
  TEST_ASSERT_EQUAL_STRING("UNDERVOLTAGE",
                           AlarmManager::alarmName(ALARM_UNDERVOLTAGE));
  TEST_ASSERT_EQUAL_STRING("OVERCURRENT",
                           AlarmManager::alarmName(ALARM_OVERCURRENT));
  TEST_ASSERT_EQUAL_STRING("UNDERCURRENT",
                           AlarmManager::alarmName(ALARM_UNDERCURRENT));
  TEST_ASSERT_EQUAL_STRING("PHASE_LOSS",
                           AlarmManager::alarmName(ALARM_PHASE_LOSS));
  TEST_ASSERT_EQUAL_STRING("HIGH_TEMP",
                           AlarmManager::alarmName(ALARM_HIGH_TEMP));
}

void test_alarm_type_from_name_roundtrip(void) {
  AlarmType roundtrip[] = {ALARM_OVERVOLTAGE,  ALARM_UNDERVOLTAGE,
                           ALARM_OVERCURRENT,  ALARM_UNDERCURRENT,
                           ALARM_PHASE_LOSS,   ALARM_HIGH_TEMP};

  for (AlarmType t : roundtrip) {
    AlarmType parsed;
    TEST_ASSERT_TRUE(AlarmManager::alarmTypeFromName(AlarmManager::alarmName(t),
                                                      parsed));
    TEST_ASSERT_EQUAL(t, parsed);
  }

  AlarmType unused;
  TEST_ASSERT_FALSE(AlarmManager::alarmTypeFromName("NOT_A_TYPE", unused));
}

void test_get_new_alarms_reports_newly_triggered(void) {
  AlarmManager alarm;
  Config cfg = makeConfig();
  AlarmState state, newAlarms;

  PhaseData okPhase[1] = {makeValidPhase(220.0f, 10.0f)};
  alarm.check(okPhase, 1, 25.0f, cfg, state);
  alarm.getNewAlarms(newAlarms);
  TEST_ASSERT_EQUAL_UINT8(0, newAlarms.count);

  PhaseData badPhase[1] = {makeValidPhase(250.0f, 10.0f)};
  alarm.check(badPhase, 1, 25.0f, cfg, state);
  alarm.getNewAlarms(newAlarms);

  TEST_ASSERT_EQUAL_UINT8(1, newAlarms.count);
  TEST_ASSERT_TRUE(newAlarms.active[0]);
  TEST_ASSERT_TRUE(newAlarms.physicallyActive[0]);
  TEST_ASSERT_EQUAL(ALARM_OVERVOLTAGE, newAlarms.events[0].type);
}

void test_new_alarm_not_reported_twice_while_still_open(void) {
  AlarmManager alarm;
  Config cfg = makeConfig();
  AlarmState state, newAlarms;

  PhaseData badPhase[1] = {makeValidPhase(250.0f, 10.0f)};
  alarm.check(badPhase, 1, 25.0f, cfg, state);
  alarm.getNewAlarms(newAlarms);
  TEST_ASSERT_EQUAL_UINT8(1, newAlarms.count);

  // Continua fora do limite no ciclo seguinte: nao deve gerar ALARM de novo,
  // ja esta aberto.
  alarm.check(badPhase, 1, 25.0f, cfg, state);
  alarm.getNewAlarms(newAlarms);
  TEST_ASSERT_EQUAL_UINT8(0, newAlarms.count);
}

void test_condition_clearing_does_not_auto_resolve(void) {
  // Mudanca de comportamento: a leitura voltar ao normal NAO gera RESOLVED
  // sozinha. O alarme continua "aberto" (aparece no refresh) ate um ack
  // explicito via resolve()/resolveAll().
  AlarmManager alarm;
  Config cfg = makeConfig();
  AlarmState state, refresh;

  PhaseData badPhase[1] = {makeValidPhase(250.0f, 10.0f)};
  alarm.check(badPhase, 1, 25.0f, cfg, state);

  PhaseData okPhase[1] = {makeValidPhase(220.0f, 10.0f)};
  alarm.check(okPhase, 1, 25.0f, cfg, state);

  alarm.getRefreshAlarms(refresh);
  TEST_ASSERT_EQUAL_UINT8(1, refresh.count);
  TEST_ASSERT_TRUE(refresh.active[0]);
  TEST_ASSERT_FALSE(refresh.physicallyActive[0]);
  TEST_ASSERT_EQUAL(ALARM_OVERVOLTAGE, refresh.events[0].type);
}

void test_get_refresh_alarms_reports_open_regardless_of_physical_state(void) {
  AlarmManager alarm;
  Config cfg = makeConfig();
  AlarmState state, refresh;

  PhaseData badPhase[1] = {makeValidPhase(250.0f, 10.0f)};
  alarm.check(badPhase, 1, 25.0f, cfg, state);
  alarm.getRefreshAlarms(refresh);

  TEST_ASSERT_EQUAL_UINT8(1, refresh.count);
  TEST_ASSERT_TRUE(refresh.active[0]);
  TEST_ASSERT_TRUE(refresh.physicallyActive[0]);
  TEST_ASSERT_EQUAL(ALARM_OVERVOLTAGE, refresh.events[0].type);
}

void test_resolve_closes_matching_open_alarm(void) {
  AlarmManager alarm;
  Config cfg = makeConfig();
  AlarmState state, resolved, refresh;

  PhaseData badPhase[1] = {makeValidPhase(250.0f, 10.0f)};
  alarm.check(badPhase, 1, 25.0f, cfg, state);

  bool found = alarm.resolve(ALARM_OVERVOLTAGE, 1, resolved);
  TEST_ASSERT_TRUE(found);
  TEST_ASSERT_EQUAL_UINT8(1, resolved.count);
  TEST_ASSERT_EQUAL(ALARM_OVERVOLTAGE, resolved.events[0].type);

  alarm.getRefreshAlarms(refresh);
  TEST_ASSERT_EQUAL_UINT8(0, refresh.count);
}

void test_resolve_returns_false_when_no_matching_alarm(void) {
  AlarmManager alarm;
  AlarmState resolved;

  bool found = alarm.resolve(ALARM_OVERVOLTAGE, 1, resolved);
  TEST_ASSERT_FALSE(found);
}

void test_resolve_all_closes_every_open_alarm(void) {
  AlarmManager alarm;
  Config cfg = makeConfig();
  AlarmState state, resolved, refresh;

  PhaseData badPhases[2] = {makeValidPhase(250.0f, 10.0f),
                            makeValidPhase(150.0f, 10.0f)};
  alarm.check(badPhases, 2, 25.0f, cfg, state);
  alarm.getRefreshAlarms(refresh);
  TEST_ASSERT_EQUAL_UINT8(2, refresh.count);

  alarm.resolveAll(resolved);
  TEST_ASSERT_EQUAL_UINT8(2, resolved.count);

  alarm.getRefreshAlarms(refresh);
  TEST_ASSERT_EQUAL_UINT8(0, refresh.count);
}

void test_open_alarm_registry_handles_worst_case_capacity(void) {
  // Pior caso real: 5 tipos por fase (overvoltage/undervoltage/overcurrent/
  // undercurrent/phase_loss) x 3 fases + high_temp = 16 combinacoes
  // distintas, todas abertas ao mesmo tempo sem nenhum ack.
  AlarmManager alarm;
  Config cfg = makeConfig();
  AlarmState state, refresh;

  // Ciclo 1: overvoltage + overcurrent em todas as fases (2 x 3 = 6 abertos).
  PhaseData overPhases[3] = {makeValidPhase(250.0f, 150.0f),
                             makeValidPhase(250.0f, 150.0f),
                             makeValidPhase(250.0f, 150.0f)};
  alarm.check(overPhases, 3, 95.0f, cfg, state);

  // Ciclo 2: undervoltage + undercurrent em todas as fases (mais 2 x 3 = 6).
  PhaseData underPhases[3] = {makeValidPhase(150.0f, 0.1f),
                              makeValidPhase(150.0f, 0.1f),
                              makeValidPhase(150.0f, 0.1f)};
  alarm.check(underPhases, 3, 95.0f, cfg, state);

  // Ciclo 3: phase_loss em todas as fases (mais 3) + high_temp (mais 1) = 16.
  PhaseData lostPhases[3];
  lostPhases[0].valid = lostPhases[1].valid = lostPhases[2].valid = false;
  alarm.check(lostPhases, 3, 95.0f, cfg, state);

  alarm.getRefreshAlarms(refresh);
  TEST_ASSERT_EQUAL_UINT8(MAX_ALARMS, refresh.count);
  TEST_ASSERT_EQUAL_UINT8(16, MAX_ALARMS);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_no_alarm_when_within_limits);
  RUN_TEST(test_overvoltage_triggers_alarm);
  RUN_TEST(test_undervoltage_triggers_alarm);
  RUN_TEST(test_overcurrent_triggers_alarm);
  RUN_TEST(test_undercurrent_triggers_alarm);
  RUN_TEST(test_zero_current_does_not_trigger_undercurrent);
  RUN_TEST(test_phase_loss_triggers_alarm);
  RUN_TEST(test_high_temperature_triggers_alarm);
  RUN_TEST(test_nan_temperature_does_not_trigger_alarm);
  RUN_TEST(test_alarm_name_strings);
  RUN_TEST(test_alarm_type_from_name_roundtrip);
  RUN_TEST(test_get_new_alarms_reports_newly_triggered);
  RUN_TEST(test_new_alarm_not_reported_twice_while_still_open);
  RUN_TEST(test_condition_clearing_does_not_auto_resolve);
  RUN_TEST(test_get_refresh_alarms_reports_open_regardless_of_physical_state);
  RUN_TEST(test_resolve_closes_matching_open_alarm);
  RUN_TEST(test_resolve_returns_false_when_no_matching_alarm);
  RUN_TEST(test_resolve_all_closes_every_open_alarm);
  RUN_TEST(test_open_alarm_registry_handles_worst_case_capacity);
  return UNITY_END();
}
