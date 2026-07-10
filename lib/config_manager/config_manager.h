#pragma once
#include "config.h"
#include <Arduino.h>
#include <Preferences.h>

class ConfigManager {
public:
  ConfigManager();
  void load(Config &cfg);
  void save(const Config &cfg);
  void reset();
  uint32_t getBootCount();
  void incrementBootCount();

private:
  Preferences prefs;
};
