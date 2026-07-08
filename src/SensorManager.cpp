#include "SensorManager.h"

SensorManager::SensorManager() : m_ahtOk(false) {
  for (auto &p : m_pzems) {
    p = nullptr;
  }
}

void SensorManager::init() {
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  Wire.begin(21, 22);

  for (uint8_t i = 0; i < MAX_PHASES; i++) {
    m_pzems[i] = new PZEM004Tv30(Serial2, 16, 17, PZEM_ADDR[i]);
  }

  m_ahtOk = m_aht.begin();
  if (m_ahtOk) {
    Serial.println("AHT10 OK");
  } else {
    Serial.println("AHT10 FAIL");
  }
}

void SensorManager::read(PhaseData *phases, uint8_t numPhases,
                          const Config &cfg) {
  for (uint8_t i = 0; i < numPhases; i++) {
    auto &p = phases[i];
    auto *pzem = m_pzems[i];

    p.v = pzem->voltage();
    p.valid = !isnan(p.v);

    if (p.valid) {
      p.v *= cfg.v_factor[i];

      float rawA = pzem->current();
      if (isnan(rawA)) { p.valid = false; continue; }
      p.a = rawA * cfg.a_factor[i];

      float rawW = pzem->power();
      if (isnan(rawW)) { p.valid = false; continue; }
      p.w = rawW * cfg.w_factor[i];

      float rawKwh = pzem->energy();
      if (isnan(rawKwh)) { p.valid = false; continue; }
      p.kwh = rawKwh;

      float rawHz = pzem->frequency();
      if (isnan(rawHz)) { p.valid = false; continue; }
      p.hz = rawHz;

      float rawPf = pzem->pf();
      if (isnan(rawPf)) { p.valid = false; continue; }
      p.pf = rawPf;
    }
  }
}

float SensorManager::getTemperature() {
  if (!m_ahtOk) return NAN;
  sensors_event_t humidity, temp;
  m_aht.getEvent(&humidity, &temp);
  return temp.temperature;
}
