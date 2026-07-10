#pragma once
#include "config_manager.h"
#include "phase_data.h"
#include <Arduino.h>
#include <PZEM004Tv30.h>

class PzemSensor {
public:
  PzemSensor();
  void init();
  void read(PhaseData *phases, uint8_t numPhases, const Config &cfg);

private:
  PZEM004Tv30 *m_pzems[MAX_PHASES];
};
