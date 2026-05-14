/*
 * Sensore Temperatura - Publisher MQTT su ESP32
 *
 * Pubblica letture simulate di temperatura sul topic 'casa/salotto/temperatura'
 * a intervalli regolari. Gestione robusta di riconnessione WiFi e MQTT.
 *
 * Librerie richieste (Library Manager):
 *   - WiFi (built-in ESP32)
 *   - PubSubClient (Nick O'Leary)
 *
 * Per usare un DHT22 reale: scommentare la sezione DHT e installare DHT sensor library.
 */

#include <WiFi.h>
#include <PubSubClient.h>

// ====== Configurazione (modificare!) ======
const char* WIFI_SSID     = "TUO_WIFI";
const char* WIFI_PASSWORD = "TUA_PASSWORD";
const char* MQTT_BROKER   = "192.168.1.100";  // IP del Raspberry Pi con Mosquitto
const uint16_t MQTT_PORT  = 1883;
const char* MQTT_CLIENT_ID = "esp32-sensore-temperatura";
const char* TOPIC_TEMPERATURA = "casa/salotto/temperatura";
const char* TOPIC_STATUS      = "casa/salotto/temperatura/status";

const unsigned long PUBLISH_INTERVAL_MS = 5000;
const uint8_t QOS_LEVEL = 1;

// ====== Stato ======
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
unsigned long lastPublish = 0;

// ====== WiFi ======
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
    Serial.printf("\n[WiFi] OK  IP=%s  RSSI=%d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println("\n[WiFi] FALLITO, riprovo al prossimo loop");
  }
}

// ====== MQTT ======
void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.printf("[MQTT] Connessione a %s:%u ... ", MQTT_BROKER, MQTT_PORT);
    // Last Will: se il sensore muore, il broker pubblica "offline"
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

// ====== Lettura simulata ======
float readTemperature() {
  // Simulazione: temperatura ambiente 18-26 °C con leggera deriva
  static float base = 22.0f;
  base += (random(-100, 101) / 1000.0f);
  if (base < 18) base = 18;
  if (base > 28) base = 28;
  // Ogni tanto un picco anomalo per testare il termostato
  if (random(0, 20) == 0) return base + random(5, 12);
  return base;
}

void publishTemperature(float t) {
  char payload[16];
  snprintf(payload, sizeof(payload), "%.2f", t);
  bool ok = mqtt.publish(TOPIC_TEMPERATURA, payload, false);
  Serial.printf("[PUB] %s -> %s  %s\n",
                TOPIC_TEMPERATURA, payload, ok ? "OK" : "FAIL");
}

// ====== Arduino ======
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Sensore Temperatura - MQTT Publisher ===");
  randomSeed(esp_random());
  connectWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(10);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    return;
  }
  if (!mqtt.connected()) {
    connectMQTT();
  }
  mqtt.loop();

  unsigned long now = millis();
  if (now - lastPublish >= PUBLISH_INTERVAL_MS) {
    lastPublish = now;
    float t = readTemperature();
    publishTemperature(t);
  }
}
