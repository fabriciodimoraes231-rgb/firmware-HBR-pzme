#pragma once
#include <Arduino.h>
#include <PZEM004Tv30.h>
#include <Adafruit_AHTX0.h>
#include "ConfigManager.h"

struct PhaseData {
  float v = 0.0f;
  float a = 0.0f;
  float w = 0.0f;
  float kwh = 0.0f;
  float hz = 0.0f;
  float pf = 0.0f;
  bool valid = false;
  float temperature = NAN;
};

class SensorManager {
public:
  SensorManager();
  void init();
  void read(PhaseData *phases, uint8_t numPhases, const Config &cfg);
  float getTemperature();

private:
  PZEM004Tv30 *m_pzems[MAX_PHASES];
  Adafruit_AHTX0 m_aht;
  bool m_ahtOk;
};
