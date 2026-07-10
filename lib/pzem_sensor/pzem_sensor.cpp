#include "pzem_sensor.h"
#include "app_config.h"

PzemSensor::PzemSensor() {
  for (auto &p : m_pzems) {
    p = nullptr;
  }
}

void PzemSensor::init() {
  Serial2.begin(PZEM_UART_BAUD, SERIAL_8N1, PZEM_RX_PIN, PZEM_TX_PIN);

  for (uint8_t i = 0; i < MAX_PHASES; i++) {
    m_pzems[i] =
        new PZEM004Tv30(Serial2, PZEM_RX_PIN, PZEM_TX_PIN, PZEM_ADDR[i]);
  }
}

void PzemSensor::read(PhaseData *phases, uint8_t numPhases, const Config &cfg) {
  for (uint8_t i = 0; i < numPhases; i++) {
    auto &p = phases[i];
    auto *pzem = m_pzems[i];

    p.v = pzem->voltage();
    p.valid = !isnan(p.v);

    if (p.valid) {
      p.v *= cfg.v_factor[i];

      float rawA = pzem->current();
      if (isnan(rawA)) {
        p.valid = false;
        continue;
      }
      p.a = rawA * cfg.a_factor[i];

      float rawW = pzem->power();
      if (isnan(rawW)) {
        p.valid = false;
        continue;
      }
      p.w = rawW * cfg.w_factor[i];

      float rawKwh = pzem->energy();
      if (isnan(rawKwh)) {
        p.valid = false;
        continue;
      }
      p.kwh = rawKwh;

      float rawHz = pzem->frequency();
      if (isnan(rawHz)) {
        p.valid = false;
        continue;
      }
      p.hz = rawHz;

      float rawPf = pzem->pf();
      if (isnan(rawPf)) {
        p.valid = false;
        continue;
      }
      p.pf = rawPf;
    }
  }
}
