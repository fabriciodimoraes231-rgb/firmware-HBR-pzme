#pragma once
#include <Adafruit_AHTX0.h>
#include <Arduino.h>

class Aht10Sensor {
public:
  Aht10Sensor();
  void init();
  float getTemperature();

private:
  Adafruit_AHTX0 m_aht;
  bool m_ahtOk;
};
