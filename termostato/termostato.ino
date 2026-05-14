/*
 * Termostato - Subscriber MQTT su ESP32
 *
 * Si abbona a 'casa/salotto/temperatura', monitora le letture e pubblica
 * allarmi su 'casa/salotto/termostato/allarme' quando la temperatura esce
 * dall'intervallo [SOGLIA_MIN, SOGLIA_MAX]. Accende il LED on-board come
 * indicatore locale di allarme.
 */

#include <WiFi.h>
#include <PubSubClient.h>

const char* WIFI_SSID     = "TUO_WIFI";
const char* WIFI_PASSWORD = "TUA_PASSWORD";
const char* MQTT_BROKER   = "192.168.1.100";
const uint16_t MQTT_PORT  = 1883;
const char* MQTT_CLIENT_ID = "esp32-termostato";

const char* TOPIC_SUB_TEMPERATURA = "casa/salotto/temperatura";
const char* TOPIC_PUB_ALLARME     = "casa/salotto/termostato/allarme";
const char* TOPIC_STATUS          = "casa/salotto/termostato/status";

const float SOGLIA_MIN = 18.0f;
const float SOGLIA_MAX = 28.0f;
const uint8_t QOS_LEVEL = 1;
const int LED_PIN = 2;  // LED on-board ESP32 DevKit

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

void connectWiFi() {
  Serial.printf("[WiFi] Connessione a %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500); Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? "\n[WiFi] OK" : "\n[WiFi] FAIL");
}

void onMessage(char* topic, byte* payload, unsigned int length) {
  char buf[32] = {0};
  unsigned int n = length < sizeof(buf) - 1 ? length : sizeof(buf) - 1;
  memcpy(buf, payload, n);

  // Validazione: parsing numerico safe
  char* end;
  float t = strtof(buf, &end);
  if (end == buf) {
    Serial.printf("[WARN] Payload non numerico su %s: '%s'\n", topic, buf);
    return;
  }

  Serial.printf("[SUB] %s = %.2f °C", topic, t);

  if (t < SOGLIA_MIN || t > SOGLIA_MAX) {
    const char* tipo = t < SOGLIA_MIN ? "FREDDO" : "CALDO";
    char alarm[96];
    snprintf(alarm, sizeof(alarm),
             "{\"tipo\":\"%s\",\"valore\":%.2f,\"min\":%.1f,\"max\":%.1f}",
             tipo, t, SOGLIA_MIN, SOGLIA_MAX);
    mqtt.publish(TOPIC_PUB_ALLARME, alarm, false);
    digitalWrite(LED_PIN, HIGH);
    Serial.printf("  *** ALLARME %s -> %s\n", tipo, alarm);
  } else {
    digitalWrite(LED_PIN, LOW);
    Serial.println("  OK");
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
      mqtt.subscribe(TOPIC_SUB_TEMPERATURA, QOS_LEVEL);
      Serial.printf("[MQTT] Sottoscritto a %s\n", TOPIC_SUB_TEMPERATURA);
    } else {
      Serial.printf("FAIL rc=%d, retry in 3s\n", mqtt.state());
      delay(3000);
      if (WiFi.status() != WL_CONNECTED) return;
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  delay(200);
  Serial.println("\n=== Termostato - MQTT Subscriber ===");
  Serial.printf("Soglie: %.1f - %.1f °C\n", SOGLIA_MIN, SOGLIA_MAX);

  connectWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(onMessage);
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(10);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) { connectWiFi(); return; }
  if (!mqtt.connected())             { connectMQTT(); }
  mqtt.loop();
}
