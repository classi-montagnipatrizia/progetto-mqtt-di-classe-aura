/* ============================================================================
 * MONITOR CENTRALIZZATO - Subscriber MQTT su ESP32
 * ----------------------------------------------------------------------------
 * Ruolo nel sistema:
 *   "Scatola nera" della casa. Si iscrive con WILDCARD a tutti i topic sotto
 *   "casa/" e logga ogni messaggio sulla seriale, con timestamp.
 *
 * Wildcard MQTT:
 *   - '+'  -> sostituisce UN livello       (es. casa/+/temperatura)
 *   - '#'  -> sostituisce ZERO o piu' livelli, deve stare a fine topic
 *            (es. casa/#  =>  riceve casa/salotto/temperatura,
 *                             casa/cucina/luce, casa/salotto/termostato/allarme, ...)
 *
 * Timestamp:
 *   - Se NTP_ENABLED = true e c'è connessione internet, si usa l'ora reale
 *     sincronizzata da pool.ntp.org (CET/CEST per l'Italia).
 *   - Altrimenti si usa il tempo dal boot (millis()/1000) come fallback.
 *
 * Buffer:
 *   - Aumentiamo il buffer interno di PubSubClient (default 256 byte) a 512
 *     per evitare di scartare messaggi piu' lunghi (es. i JSON di allarme).
 * ============================================================================ */

#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>

// =============================================================================
// CONFIGURAZIONE
// =============================================================================
const char* WIFI_SSID     = "TUO_WIFI";
const char* WIFI_PASSWORD = "TUA_PASSWORD";
const char* MQTT_BROKER   = "192.168.1.100";
const uint16_t MQTT_PORT  = 1883;
const char* MQTT_CLIENT_ID = "esp32-monitor";

// Wildcard '#' = ascolta TUTTO sotto "casa/"
const char* TOPIC_WILDCARD = "casa/#";
const char* TOPIC_STATUS   = "casa/monitor/status";
const uint8_t QOS_LEVEL = 1;

// Configurazione NTP (Network Time Protocol) per timestamp reali
const bool NTP_ENABLED       = true;
const char* NTP_SERVER       = "pool.ntp.org";
const long  GMT_OFFSET_SEC   = 3600;   // CET  = UTC+1 (3600 secondi)
const int   DST_OFFSET_SEC   = 3600;   // CEST = +1 ora di solare legale

// =============================================================================
// STATO GLOBALE
// =============================================================================
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
unsigned long messageCount = 0;   // contatore eventi totali ricevuti

// =============================================================================
// COSTRUZIONE TIMESTAMP
// -----------------------------------------------------------------------------
// Tenta di ottenere l'ora reale via NTP; se non disponibile (no sincronizzazione
// o NTP disabilitato), fallback ai secondi dal boot.
// =============================================================================
void timestamp(char* out, size_t n) {
  if (NTP_ENABLED) {
    struct tm tm;
    // getLocalTime attende fino a 50ms che l'orologio sia sincronizzato
    if (getLocalTime(&tm, 50)) {
      strftime(out, n, "%H:%M:%S", &tm);
      return;
    }
  }
  // Fallback: secondi dal boot
  unsigned long s = millis() / 1000;
  snprintf(out, n, "+%lu s", s);
}

// =============================================================================
// CALLBACK MESSAGGI
// -----------------------------------------------------------------------------
// Riceviamo qualsiasi cosa sotto "casa/" e la stampiamo formattata.
// Il payload viene copiato in un buffer locale per terminarlo con '\0'
// (vedi nota in termostato.ino).
// =============================================================================
void onMessage(char* topic, byte* payload, unsigned int length) {
  char buf[256] = {0};
  unsigned int n = length < sizeof(buf) - 1 ? length : sizeof(buf) - 1;
  memcpy(buf, payload, n);

  messageCount++;

  char ts[32];
  timestamp(ts, sizeof(ts));

  // Layout colonne: data | numero progressivo | topic (allineato) | payload
  Serial.printf("[%s] #%lu  %-40s -> %s\n", ts, messageCount, topic, buf);
}

// =============================================================================
// CONNESSIONE WIFI + avvio NTP
// =============================================================================
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
    // Avviamo la sincronizzazione NTP UNA VOLTA, appena il WiFi è su.
    // configTime imposta il fuso orario + DST e fa partire un task in background
    // che mantiene aggiornato l'orologio interno.
    if (NTP_ENABLED) configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);
  } else {
    Serial.println("\n[WiFi] FAIL");
  }
}

// =============================================================================
// CONNESSIONE MQTT
// -----------------------------------------------------------------------------
// All'avvio (e a ogni riconnessione) ci ri-iscriviamo alla wildcard.
// =============================================================================
void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.printf("[MQTT] Connessione a %s:%u ... ", MQTT_BROKER, MQTT_PORT);

    bool connected = mqtt.connect(
      MQTT_CLIENT_ID,
      nullptr, nullptr,
      TOPIC_STATUS, QOS_LEVEL, true, "offline"  // LWT
    );

    if (connected) {
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

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Monitor Centralizzato - MQTT Logger ===");

  connectWiFi();

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(onMessage);
  // Buffer ampliato: i JSON di allarme + topic lunghi possono superare i 256 byte di default
  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(30);
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  if (WiFi.status() != WL_CONNECTED) { connectWiFi(); return; }
  if (!mqtt.connected())             { connectMQTT(); }
  mqtt.loop();   // qui vengono consegnati i messaggi alla callback onMessage()
}
