/*
 * Monitor Centralizzato - Subscriber MQTT su ESP32
 *
 * Si abbona a 'casa/#' e logga tutti gli eventi su Serial con timestamp
 * (millis dal boot). In assenza di RTC, il timestamp è relativo al boot;
 * abilitare NTP_ENABLED per usare orario reale via NTP.
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>

const char* WIFI_SSID     = "TUO_WIFI";
const char* WIFI_PASSWORD = "TUA_PASSWORD";
const char* MQTT_BROKER   = "192.168.1.100";
const uint16_t MQTT_PORT  = 1883;
const char* MQTT_CLIENT_ID = "esp32-monitor";

const char* TOPIC_WILDCARD = "casa/#";
const char* TOPIC_STATUS   = "casa/monitor/status";
const uint8_t QOS_LEVEL = 1;

const bool NTP_ENABLED = true;
const char* NTP_SERVER = "pool.ntp.org";
const long  GMT_OFFSET_SEC = 3600;       // CET
const int   DST_OFFSET_SEC = 3600;       // CEST

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
unsigned long messageCount = 0;

void timestamp(char* out, size_t n) {
  if (NTP_ENABLED) {
    struct tm tm;
    if (getLocalTime(&tm, 50)) {
      strftime(out, n, "%H:%M:%S", &tm);
      return;
    }
  }
  unsigned long s = millis() / 1000;
  snprintf(out, n, "+%lu s", s);
}

void onMessage(char* topic, byte* payload, unsigned int length) {
  char buf[256] = {0};
  unsigned int n = length < sizeof(buf) - 1 ? length : sizeof(buf) - 1;
  memcpy(buf, payload, n);
  messageCount++;

  char ts[32];
  timestamp(ts, sizeof(ts));
  Serial.printf("[%s] #%lu  %-40s -> %s\n", ts, messageCount, topic, buf);
}

void connectWiFi() {
  Serial.printf("[WiFi] Connessione a %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500); Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] OK  IP=%s\n", WiFi.localIP().toString().c_str());
    if (NTP_ENABLED) configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);
  } else {
    Serial.println("\n[WiFi] FAIL");
  }
}

void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.printf("[MQTT] Connessione a %s:%u ... ", MQTT_BROKER, MQTT_PORT);
    if (mqtt.connect(MQTT_CLIENT_ID,
                     nullptr, nullptr,
                     TOPIC_STATUS, QOS_LEVEL, true, "offline")) {
      Serial.println("OK");
      mqtt.publish(TOPIC_STATUS, "online", true);
      mqtt.subscribe(TOPIC_WILDCARD, QOS_LEVEL);
      Serial.printf("[MQTT] Sottoscritto a %s (logga TUTTI gli eventi)\n",
                    TOPIC_WILDCARD);
    } else {
      Serial.printf("FAIL rc=%d, retry in 3s\n", mqtt.state());
      delay(3000);
      if (WiFi.status() != WL_CONNECTED) return;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Monitor Centralizzato - MQTT Logger ===");
  connectWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(onMessage);
  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(30);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) { connectWiFi(); return; }
  if (!mqtt.connected())             { connectMQTT(); }
  mqtt.loop();
}
