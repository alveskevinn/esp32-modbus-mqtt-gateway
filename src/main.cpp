// ESP32 Modbus RTU -> MQTT gateway.
//
// Polls one Modbus RTU slave over RS-485, decodes the register map in
// include/register_map.h and publishes JSON telemetry to MQTT.
//
// Topics (<t> = <base topic>/<device id>):
//   <t>/status     "online" / "offline" (retained, LWT)
//   <t>/info       firmware, network and register map (retained)
//   <t>/telemetry  {"ts":..,"values":{..},"errors":..}
//   <t>/cmd        {"interval":30} | {"reboot":true} | {"portal":true} | {"ota_url":"https://.."}

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <HTTPUpdate.h>
#include <ModbusRTU.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "config.h"
#include "poller.h"
#include "register_map.h"
#include "settings.h"

static Settings cfg;
static HardwareSerial Rs485(1);
static ModbusRTU mb;
static Poller poller(mb);
static WiFiClient netClient;
static PubSubClient mqtt(netClient);

static char topicBase[112];
static uint32_t lastPollMs = 0;
static uint32_t lastMqttAttemptMs = 0;
static uint32_t pollCount = 0;
static uint32_t failCount = 0;

static String topic(const char* leaf) {
  return String(topicBase) + "/" + leaf;
}

static bool sleepMode() {
  return cfg.sleepSeconds > 0;
}

static uint32_t epochNow() {
  const time_t now = time(nullptr);
  return now > 1700000000 ? (uint32_t)now : 0;  // 0 until SNTP has synced
}

static void publishInfo() {
  JsonDocument doc;
  doc["fw"] = FW_VERSION;
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();
  doc["slave"] = cfg.slaveId;
  doc["baud"] = cfg.baud;
  doc["poll_s"] = cfg.pollSeconds;
  doc["sleep_s"] = cfg.sleepSeconds;
  doc["blocks"] = poller.blockCount();
  JsonObject points = doc["points"].to<JsonObject>();
  for (const auto& p : device::POINTS) points[p.name] = p.unit;

  String out;
  serializeJson(doc, out);
  mqtt.publish(topic("info").c_str(), out.c_str(), true);
}

static void doHttpOta(const String& url) {
  mqtt.publish(topic("status").c_str(), "updating", true);
  httpUpdate.rebootOnUpdate(true);
  t_httpUpdate_return ret;
  if (url.startsWith("https://")) {
    WiFiClientSecure tls;
    tls.setInsecure();  // pin a CA in production; see README "Security"
    ret = httpUpdate.update(tls, url);
  } else {
    WiFiClient plain;
    ret = httpUpdate.update(plain, url);
  }
  // Only reached on failure: success reboots.
  log_e("OTA failed (%d): %s", ret, httpUpdate.getLastErrorString().c_str());
  mqtt.publish(topic("status").c_str(), "online", true);
}

static void onCommand(char*, byte* payload, unsigned int len) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, len)) return;

  if (doc["interval"].is<uint32_t>()) {
    const uint32_t s = doc["interval"];
    if (s >= 1 && s <= 86400) {
      cfg.pollSeconds = s;
      saveSettings(cfg);
      publishInfo();
    }
  }
  if (doc["reboot"] == true) ESP.restart();
  if (doc["portal"] == true) {
    connectWifi(cfg, true);
    ESP.restart();
  }
  if (doc["ota_url"].is<const char*>()) doHttpOta(doc["ota_url"].as<String>());
}

static bool ensureMqtt() {
  if (mqtt.connected()) return true;
  if (strlen(cfg.mqttHost) == 0) return false;
  if (millis() - lastMqttAttemptMs < 5000 && lastMqttAttemptMs != 0) return false;
  lastMqttAttemptMs = millis();

  mqtt.setServer(cfg.mqttHost, cfg.mqttPort);
  mqtt.setCallback(onCommand);
  mqtt.setBufferSize(MQTT_MAX_PACKET_SIZE);

  const String status = topic("status");
  const char* user = strlen(cfg.mqttUser) ? cfg.mqttUser : nullptr;
  const char* pass = strlen(cfg.mqttPass) ? cfg.mqttPass : nullptr;
  if (!mqtt.connect(cfg.deviceId, user, pass, status.c_str(), 1, true, "offline")) {
    log_w("MQTT connect failed, state=%d", mqtt.state());
    return false;
  }
  mqtt.publish(status.c_str(), "online", true);
  mqtt.subscribe(topic("cmd").c_str(), 1);
  publishInfo();
  return true;
}

static void pollAndPublish() {
  PollResult r;
  poller.poll(cfg.slaveId, r);
  pollCount++;
  if (r.blocksFailed) failCount++;

  JsonDocument doc;
  const uint32_t ts = epochNow();
  if (ts) doc["ts"] = ts;
  doc["slave"] = cfg.slaveId;
  doc["ok"] = r.blocksFailed == 0;
  JsonObject values = doc["values"].to<JsonObject>();
  for (size_t i = 0; i < device::POINT_COUNT; ++i) {
    if (r.valid[i] && isfinite(r.values[i])) values[device::POINTS[i].name] = r.values[i];
  }
  doc["errors"] = failCount;
  doc["polls"] = pollCount;
  doc["rssi"] = WiFi.RSSI();
  doc["uptime_s"] = millis() / 1000;

  String out;
  serializeJson(doc, out);
  Serial.println(out);
  if (ensureMqtt()) mqtt.publish(topic("telemetry").c_str(), out.c_str());
}

static void setupOta() {
  ArduinoOTA.setHostname(cfg.deviceId);
  if (strlen(cfg.otaPassword)) ArduinoOTA.setPassword(cfg.otaPassword);
  ArduinoOTA.onStart([]() { mqtt.publish(topic("status").c_str(), "updating", true); });
  ArduinoOTA.begin();
}

static void deepSleep() {
  mqtt.disconnect();  // clean disconnect: broker does not fire the LWT
  WiFi.disconnect(true);
  esp_sleep_enable_timer_wakeup((uint64_t)cfg.sleepSeconds * 1000000ULL);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  pinMode(PORTAL_BUTTON_GPIO, INPUT_PULLUP);
  delay(50);
  const bool forcePortal = digitalRead(PORTAL_BUTTON_GPIO) == LOW;

  loadSettings(cfg);
  Serial.printf("\nModbus MQTT gateway v%s — %s\n", FW_VERSION, cfg.deviceId);

  if (!connectWifi(cfg, forcePortal || strlen(cfg.mqttHost) == 0)) {
    Serial.println("Wi-Fi unavailable; retrying after sleep/reboot");
    if (sleepMode()) deepSleep();
    ESP.restart();
  }
  snprintf(topicBase, sizeof topicBase, "%s/%s", cfg.baseTopic, cfg.deviceId);
  configTime(0, 0, "pool.ntp.org", "time.google.com");

  Rs485.begin(cfg.baud, SERIAL_8N1, RS485_RX_GPIO, RS485_TX_GPIO);
#if RS485_DE_GPIO >= 0
  mb.begin(&Rs485, RS485_DE_GPIO);  // library drives DE/RE high while transmitting
#else
  mb.begin(&Rs485);
#endif
  mb.master();

  if (!poller.begin()) {
    Serial.println("Register map does not fit the read plan buffers");
    while (true) delay(1000);
  }

  if (sleepMode()) {
    // One-shot cycle: read, publish, flush, sleep. OTA only via the portal.
    const uint32_t t0 = millis();
    while (!ensureMqtt() && millis() - t0 < 10000) delay(250);
    for (int i = 0; i < 20 && !epochNow(); ++i) delay(100);  // brief wait for SNTP
    pollAndPublish();
    for (int i = 0; i < 10; ++i) {
      mqtt.loop();
      delay(20);
    }
    deepSleep();
  }

  setupOta();
  ensureMqtt();
}

void loop() {
  ArduinoOTA.handle();
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(1000);
    return;
  }
  ensureMqtt();
  mqtt.loop();

  if (millis() - lastPollMs >= cfg.pollSeconds * 1000UL || lastPollMs == 0) {
    lastPollMs = millis();
    pollAndPublish();
  }
}
