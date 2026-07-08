#pragma once
#include <Arduino.h>
#include <time.h>
#include <sys/time.h>

class TimeManager {
public:
  TimeManager();
  void init();
  void setFromEpoch(unsigned long epoch);
  bool setFromText(const char *text);
  String getTimestamp();
  unsigned long getUptimeS();
  bool isTimeValid();

private:
  bool m_timeSet;
};
