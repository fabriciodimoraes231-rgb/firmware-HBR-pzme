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

void test_get_new_alarms_reports_newly_triggered(void) {
  AlarmManager alarm;
  Config cfg = makeConfig();
  AlarmState state, newAlarms;

  PhaseData okPhase[1] = {makeValidPhase(220.0f, 10.0f)};
  alarm.check(okPhase, 1, 25.0f, cfg, state);
  alarm.getNewAlarms(state, newAlarms);
  TEST_ASSERT_EQUAL_UINT8(0, newAlarms.count);

  PhaseData badPhase[1] = {makeValidPhase(250.0f, 10.0f)};
  alarm.check(badPhase, 1, 25.0f, cfg, state);
  alarm.getNewAlarms(state, newAlarms);

  TEST_ASSERT_EQUAL_UINT8(1, newAlarms.count);
  TEST_ASSERT_TRUE(newAlarms.active[0]);
  TEST_ASSERT_EQUAL(ALARM_OVERVOLTAGE, newAlarms.events[0].type);
}

void test_get_resolved_alarms_reports_cleared(void) {
  AlarmManager alarm;
  Config cfg = makeConfig();
  AlarmState state, resolved;

  PhaseData badPhase[1] = {makeValidPhase(250.0f, 10.0f)};
  alarm.check(badPhase, 1, 25.0f, cfg, state);

  PhaseData okPhase[1] = {makeValidPhase(220.0f, 10.0f)};
  alarm.check(okPhase, 1, 25.0f, cfg, state);
  alarm.getResolvedAlarms(state, resolved);

  TEST_ASSERT_EQUAL_UINT8(1, resolved.count);
  TEST_ASSERT_TRUE(resolved.active[0]);
  TEST_ASSERT_EQUAL(ALARM_OVERVOLTAGE, resolved.events[0].type);
}

void test_get_active_alarms_reports_current_state(void) {
  AlarmManager alarm;
  Config cfg = makeConfig();
  AlarmState state, active;

  PhaseData badPhase[1] = {makeValidPhase(250.0f, 10.0f)};
  alarm.check(badPhase, 1, 25.0f, cfg, state);
  alarm.getActiveAlarms(state, active);

  TEST_ASSERT_EQUAL_UINT8(1, active.count);
  TEST_ASSERT_TRUE(active.active[0]);
  TEST_ASSERT_EQUAL(ALARM_OVERVOLTAGE, active.events[0].type);
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
  RUN_TEST(test_get_new_alarms_reports_newly_triggered);
  RUN_TEST(test_get_resolved_alarms_reports_cleared);
  RUN_TEST(test_get_active_alarms_reports_current_state);
  return UNITY_END();
}
