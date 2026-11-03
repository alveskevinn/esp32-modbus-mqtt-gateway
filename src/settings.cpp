#include "settings.h"

#include <Preferences.h>
#include <WiFiManager.h>

#include "config.h"

static const char* NS = "mbgw";

static void copyString(char* dst, size_t size, const String& src) {
  strncpy(dst, src.c_str(), size - 1);
  dst[size - 1] = '\0';
}

void loadSettings(Settings& s) {
  Preferences p;
  p.begin(NS, true);

  // Default device id derived from the MAC: unique without provisioning.
  char defId[32];
  snprintf(defId, sizeof defId, "mbgw-%06llx", ESP.getEfuseMac() & 0xFFFFFFULL);

  copyString(s.deviceId, sizeof s.deviceId, p.getString("id", defId));
  copyString(s.mqttHost, sizeof s.mqttHost, p.getString("host", ""));
  s.mqttPort = p.getUShort("port", DEFAULT_MQTT_PORT);
  copyString(s.mqttUser, sizeof s.mqttUser, p.getString("user", ""));
  copyString(s.mqttPass, sizeof s.mqttPass, p.getString("pass", ""));
  copyString(s.baseTopic, sizeof s.baseTopic, p.getString("topic", DEFAULT_BASE_TOPIC));
  s.slaveId = p.getUChar("slave", DEFAULT_SLAVE_ID);
  s.baud = p.getUInt("baud", DEFAULT_BAUD);
  s.pollSeconds = p.getUInt("poll", DEFAULT_POLL_S);
  s.sleepSeconds = p.getUInt("sleep", DEFAULT_SLEEP_S);
  copyString(s.otaPassword, sizeof s.otaPassword, p.getString("ota", ""));
  p.end();
}

void saveSettings(const Settings& s) {
  Preferences p;
  p.begin(NS, false);
  p.putString("id", s.deviceId);
  p.putString("host", s.mqttHost);
  p.putUShort("port", s.mqttPort);
  p.putString("user", s.mqttUser);
  p.putString("pass", s.mqttPass);
  p.putString("topic", s.baseTopic);
  p.putUChar("slave", s.slaveId);
  p.putUInt("baud", s.baud);
  p.putUInt("poll", s.pollSeconds);
  p.putUInt("sleep", s.sleepSeconds);
  p.putString("ota", s.otaPassword);
  p.end();
}

static uint32_t parseBounded(const char* v, uint32_t min, uint32_t max, uint32_t fallback) {
  const long n = strtol(v, nullptr, 10);
  return (n >= (long)min && n <= (long)max) ? (uint32_t)n : fallback;
}

bool connectWifi(Settings& s, bool forcePortal) {
  WiFiManager wm;
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  wm.setConnectTimeout(20);
  wm.setTitle("Modbus MQTT Gateway");

  char port[6], slave[4], baud[8], poll[8], sleep[8];
  snprintf(port, sizeof port, "%u", s.mqttPort);
  snprintf(slave, sizeof slave, "%u", s.slaveId);
  snprintf(baud, sizeof baud, "%lu", (unsigned long)s.baud);
  snprintf(poll, sizeof poll, "%lu", (unsigned long)s.pollSeconds);
  snprintf(sleep, sizeof sleep, "%lu", (unsigned long)s.sleepSeconds);

  WiFiManagerParameter hdrMqtt("<h3>MQTT</h3>");
  WiFiManagerParameter pHost("host", "Broker host", s.mqttHost, sizeof s.mqttHost - 1);
  WiFiManagerParameter pPort("port", "Broker port", port, 5);
  WiFiManagerParameter pUser("user", "Username", s.mqttUser, sizeof s.mqttUser - 1);
  WiFiManagerParameter pPass("pass", "Password", s.mqttPass, sizeof s.mqttPass - 1,
                             "type='password'");
  WiFiManagerParameter pTopic("topic", "Base topic", s.baseTopic, sizeof s.baseTopic - 1);
  WiFiManagerParameter pId("id", "Device id", s.deviceId, sizeof s.deviceId - 1);
  WiFiManagerParameter hdrModbus("<h3>Modbus RTU</h3>");
  WiFiManagerParameter pSlave("slave", "Slave id (1-247)", slave, 3);
  WiFiManagerParameter pBaud("baud", "Baud rate", baud, 7);
  WiFiManagerParameter hdrPower("<h3>Timing</h3>");
  WiFiManagerParameter pPoll("poll", "Poll interval (s)", poll, 7);
  WiFiManagerParameter pSleep("sleep", "Deep sleep (s, 0 = off)", sleep, 7);
  WiFiManagerParameter pOta("ota", "OTA password", s.otaPassword, sizeof s.otaPassword - 1,
                            "type='password'");

  WiFiManagerParameter* params[] = {&hdrMqtt,   &pHost,  &pPort, &pUser,    &pPass, &pTopic, &pId,
                                    &hdrModbus, &pSlave, &pBaud, &hdrPower, &pPoll, &pSleep, &pOta};
  for (auto* param : params) wm.addParameter(param);

  bool dirty = false;
  wm.setSaveParamsCallback([&dirty]() { dirty = true; });
  wm.setSaveConfigCallback([&dirty]() { dirty = true; });

  char ap[32];
  snprintf(ap, sizeof ap, PORTAL_AP_PREFIX "%06llX", ESP.getEfuseMac() & 0xFFFFFFULL);

  const bool online = forcePortal ? wm.startConfigPortal(ap) : wm.autoConnect(ap);

  if (dirty) {
    copyString(s.mqttHost, sizeof s.mqttHost, pHost.getValue());
    s.mqttPort = parseBounded(pPort.getValue(), 1, 65535, s.mqttPort);
    copyString(s.mqttUser, sizeof s.mqttUser, pUser.getValue());
    copyString(s.mqttPass, sizeof s.mqttPass, pPass.getValue());
    copyString(s.baseTopic, sizeof s.baseTopic, pTopic.getValue());
    if (strlen(pId.getValue()) > 0) copyString(s.deviceId, sizeof s.deviceId, pId.getValue());
    s.slaveId = parseBounded(pSlave.getValue(), 1, 247, s.slaveId);
    s.baud = parseBounded(pBaud.getValue(), 1200, 115200, s.baud);
    s.pollSeconds = parseBounded(pPoll.getValue(), 1, 86400, s.pollSeconds);
    s.sleepSeconds = parseBounded(pSleep.getValue(), 0, 86400, s.sleepSeconds);
    copyString(s.otaPassword, sizeof s.otaPassword, pOta.getValue());
    saveSettings(s);
  }
  return online;
}
