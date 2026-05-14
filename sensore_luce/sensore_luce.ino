/* ============================================================================
 * SENSORE LUCE - Publisher MQTT su ESP32
 * ----------------------------------------------------------------------------
 * Ruolo nel sistema:
 *   Simula un sensore di luminosità ambientale (in lux) installato in salotto.
 *   Pubblica letture periodiche sul broker MQTT al topic casa/salotto/luce.
 *
 * Differenze rispetto al sensore di temperatura:
 *   - topic diverso
 *   - range simulato 0-1000 lux (illuminazione tipica indoor)
 *   - nessuna anomalia simulata: qui non c'è un "termostato" che generi
 *     allarmi, è solo telemetria.
 *
 * Per uso REALE con un fotoresistore (LDR) su pin analogico:
 *   const int LDR_PIN = 34;                  // pin ADC capace
 *   int raw = analogRead(LDR_PIN);           // 0..4095 su ESP32
 *   float lux = map(raw, 0, 4095, 0, 1000);  // mapping grezzo
 *
 * Per uso REALE con BH1750 (I2C) la lettura è già in lux veri.
 * ============================================================================ */

#include <WiFi.h>
#include <PubSubClient.h>

// =============================================================================
// CONFIGURAZIONE
// =============================================================================
const char* WIFI_SSID     = "TUO_WIFI";
const char* WIFI_PASSWORD = "TUA_PASSWORD";
const char* MQTT_BROKER   = "192.168.1.100";  // IP del broker
const uint16_t MQTT_PORT  = 1883;

const char* MQTT_CLIENT_ID = "esp32-sensore-luce";  // ID univoco sul broker
const char* TOPIC_LUCE   = "casa/salotto/luce";
const char* TOPIC_STATUS = "casa/salotto/luce/status";

const unsigned long PUBLISH_INTERVAL_MS = 5000;
const uint8_t QOS_LEVEL = 1;

// =============================================================================
// STATO GLOBALE
// =============================================================================
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
unsigned long lastPublish = 0;

// =============================================================================
// CONNESSIONE WIFI (vedi sensore_temperatura.ino per dettagli)
// =============================================================================
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

// =============================================================================
// CONNESSIONE MQTT con Last Will & Testament "offline" retained
// =============================================================================
void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.printf("[MQTT] Connessione a %s:%u ... ", MQTT_BROKER, MQTT_PORT);

    // LWT: se il nodo si disconnette in modo anomalo, il broker
    // pubblica "offline" retained sul topic di status.
    bool connected = mqtt.connect(
      MQTT_CLIENT_ID,
      nullptr, nullptr,
      TOPIC_STATUS, QOS_LEVEL, true, "offline"
    );

    if (connected) {
      Serial.println("OK");
      mqtt.publish(TOPIC_STATUS, "online", true);  // retained: visibile a chi si iscrive dopo
    } else {
      Serial.printf("FAIL rc=%d, retry in 3s\n", mqtt.state());
      delay(3000);
      if (WiFi.status() != WL_CONNECTED) return;
    }
  }
}

// =============================================================================
// LETTURA SIMULATA DELLA LUMINOSITA'
// -----------------------------------------------------------------------------
// Random walk attorno a 500 lux con clamp tra 0 e 1000.
// =============================================================================
float readLight() {
  static float base = 500.0f;
  base += random(-50, 51);            // variazione di ±50 lux per campione
  if (base < 0)    base = 0;
  if (base > 1000) base = 1000;
  return base;
}

// =============================================================================
// PUBBLICAZIONE
// =============================================================================
void publishLight(float lux) {
  char payload[16];
  snprintf(payload, sizeof(payload), "%.1f", lux);  // 1 decimale basta per lux
  bool ok = mqtt.publish(TOPIC_LUCE, payload, false);
  Serial.printf("[PUB] %s -> %s lux  %s\n",
                TOPIC_LUCE, payload, ok ? "OK" : "FAIL");
}

// =============================================================================
// SETUP
// =============================================================================
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

// =============================================================================
// LOOP - non bloccante, schedulato con millis()
// =============================================================================
void loop() {
  if (WiFi.status() != WL_CONNECTED) { connectWiFi(); return; }
  if (!mqtt.connected())             { connectMQTT(); }
  mqtt.loop();  // mantiene viva la connessione e processa pacchetti

  unsigned long now = millis();
  if (now - lastPublish >= PUBLISH_INTERVAL_MS) {
    lastPublish = now;
    publishLight(readLight());
  }
}
