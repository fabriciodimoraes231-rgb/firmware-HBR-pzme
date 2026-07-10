#include "aht10_sensor.h"
#include "app_config.h"

Aht10Sensor::Aht10Sensor() : m_ahtOk(false) {}

void Aht10Sensor::init() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  m_ahtOk = m_aht.begin();
  if (m_ahtOk) {
    Serial.println("AHT10 OK");
  } else {
    Serial.println("AHT10 FAIL");
  }
}

float Aht10Sensor::getTemperature() {
  if (!m_ahtOk)
    return NAN;
  sensors_event_t humidity, temp;
  m_aht.getEvent(&humidity, &temp);
  return temp.temperature;
}
