#include "time_manager.h"
#include "app_config.h"

TimeManager::TimeManager() : m_timeSet(false) {}

void TimeManager::init() {
  configTime(NTP_GMT_OFFSET_SEC, NTP_DST_OFFSET_SEC, NTP_SERVER_1,
             NTP_SERVER_2);
  Serial.print("NTP sync");
  time_t now = time(nullptr);
  int attempts = 0;
  while (now < 100000 && attempts < NTP_SYNC_MAX_ATTEMPTS) {
    delay(NTP_SYNC_RETRY_DELAY_MS);
    now = time(nullptr);
    attempts++;
    Serial.print(".");
  }
  if (now >= 100000) {
    m_timeSet = true;
    Serial.println(" OK");
  } else {
    Serial.println(" FAIL");
  }
}

void TimeManager::setFromEpoch(unsigned long epoch) {
  struct timeval tv;
  tv.tv_sec = epoch;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  m_timeSet = true;
}

bool TimeManager::setFromText(const char *text) {
  int h, m, d, mo, y;
  if (sscanf(text, "%d:%d %d/%d/%d", &h, &m, &d, &mo, &y) != 5) {
    return false;
  }
  struct tm ti;
  ti.tm_hour = h;
  ti.tm_min = m;
  ti.tm_sec = 0;
  ti.tm_mday = d;
  ti.tm_mon = mo - 1;
  ti.tm_year = y - 1900;
  ti.tm_isdst = -1;
  time_t epoch = mktime(&ti);
  if (epoch < 100000) {
    return false;
  }
  setFromEpoch(epoch);
  return true;
}

String TimeManager::getTimestamp() {
  time_t now = time(nullptr);
  struct tm *ti = localtime(&now);
  if (ti == nullptr) {
    return "00:00 01/01/1970";
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "%02d:%02d %02d/%02d/%04d", ti->tm_hour,
           ti->tm_min, ti->tm_mday, ti->tm_mon + 1, ti->tm_year + 1900);
  return String(buf);
}

unsigned long TimeManager::getUptimeS() { return millis() / 1000; }

bool TimeManager::isTimeValid() {
  if (!m_timeSet)
    return false;
  time_t now = time(nullptr);
  return now >= 100000;
}
