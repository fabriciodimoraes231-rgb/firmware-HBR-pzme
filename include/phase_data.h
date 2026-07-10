#pragma once
#include <cmath>

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
