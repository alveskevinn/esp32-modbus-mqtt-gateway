#pragma once

#include <Arduino.h>

struct Settings {
  char deviceId[32];
  char mqttHost[64];
  uint16_t mqttPort;
  char mqttUser[32];
  char mqttPass[64];
  char baseTopic[64];
  uint8_t slaveId;
  uint32_t baud;
  uint32_t pollSeconds;
  uint32_t sleepSeconds;
  char otaPassword[32];
};

// Loads settings from NVS, falling back to config.h defaults.
void loadSettings(Settings& s);
void saveSettings(const Settings& s);

// Connects to Wi-Fi with the stored credentials. When there are none, the
// connection fails, or `forcePortal` is true, opens a captive portal where
// Wi-Fi, MQTT and Modbus settings can be edited. Returns true when online.
bool connectWifi(Settings& s, bool forcePortal);
