/*
 * Sensore Luce - Publisher MQTT su ESP32
 *
 * Pubblica letture simulate di luminosità (lux) sul topic 'casa/salotto/luce'.
 * Per usare un LDR/BH1750 reale, sostituire readLight() con la lettura ADC.
 */

#include <WiFi.h>
#include <PubSubClient.h>

const char* WIFI_SSID     = "TUO_WIFI";
const char* WIFI_PASSWORD = "TUA_PASSWORD";
const char* MQTT_BROKER   = "192.168.1.100";
const uint16_t MQTT_PORT  = 1883;
const char* MQTT_CLIENT_ID = "esp32-sensore-luce";
const char* TOPIC_LUCE   = "casa/salotto/luce";
const char* TOPIC_STATUS = "casa/salotto/luce/status";

const unsigned long PUBLISH_INTERVAL_MS = 5000;
const uint8_t QOS_LEVEL = 1;

// Per LDR reale: const int LDR_PIN = 34;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
unsigned long lastPublish = 0;

void connectWiFi() {
  Serial.printf("[WiFi] Connessione a %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] OK  IP=%s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi] FALLITO");
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
    } else {
      Serial.printf("FAIL rc=%d, retry in 3s\n", mqtt.state());
      delay(3000);
      if (WiFi.status() != WL_CONNECTED) return;
    }
  }
}

float readLight() {
  // Simulazione: 0-1000 lux con variazione realistica
  // Per LDR reale: int raw = analogRead(LDR_PIN); return map(raw, 0, 4095, 0, 1000);
  static float base = 500.0f;
  base += random(-50, 51);
  if (base < 0)    base = 0;
  if (base > 1000) base = 1000;
  return base;
}

void publishLight(float lux) {
  char payload[16];
  snprintf(payload, sizeof(payload), "%.1f", lux);
  bool ok = mqtt.publish(TOPIC_LUCE, payload, false);
  Serial.printf("[PUB] %s -> %s lux  %s\n",
                TOPIC_LUCE, payload, ok ? "OK" : "FAIL");
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Sensore Luce - MQTT Publisher ===");
  randomSeed(esp_random());
  connectWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(10);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) { connectWiFi(); return; }
  if (!mqtt.connected())             { connectMQTT(); }
  mqtt.loop();

  unsigned long now = millis();
  if (now - lastPublish >= PUBLISH_INTERVAL_MS) {
    lastPublish = now;
    publishLight(readLight());
  }
}
