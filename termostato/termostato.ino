/* ============================================================================
 * TERMOSTATO - Subscriber MQTT su ESP32
 * ----------------------------------------------------------------------------
 * Ruolo nel sistema:
 *   Vigila sulla temperatura della casa. Non legge sensori propri:
 *   si ISCRIVE al topic del sensore di temperatura e, per ogni messaggio
 *   ricevuto, controlla se il valore esce dall'intervallo accettabile.
 *   In caso di anomalia:
 *     - accende il LED on-board (segnale locale)
 *     - PUBBLICA un allarme strutturato (JSON) su un topic dedicato
 *
 * Pattern MQTT in gioco:
 *   - Subscriber: registra una "callback" che il client chiama ogni volta
 *     che arriva un messaggio sui topic sottoscritti.
 *   - Publisher: a sua volta pubblica gli allarmi su un altro topic.
 *   - Un client MQTT può fare entrambe le cose contemporaneamente.
 *
 * Robustezza:
 *   - Validazione del payload con strtof (scartiamo messaggi non numerici).
 *   - Buffer di dimensione fissa: niente String dinamiche -> no heap fragmentation.
 * ============================================================================ */

#include <WiFi.h>
#include <PubSubClient.h>

// =============================================================================
// CONFIGURAZIONE
// =============================================================================
const char* WIFI_SSID     = "TUO_WIFI";
const char* WIFI_PASSWORD = "TUA_PASSWORD";
const char* MQTT_BROKER   = "192.168.1.100";
const uint16_t MQTT_PORT  = 1883;
const char* MQTT_CLIENT_ID = "esp32-termostato";

// Topic a cui ci ISCRIVIAMO per ricevere le temperature
const char* TOPIC_SUB_TEMPERATURA = "casa/salotto/temperatura";

// Topic su cui PUBBLICHIAMO eventuali allarmi (in JSON)
const char* TOPIC_PUB_ALLARME     = "casa/salotto/termostato/allarme";

// Topic di stato (online/offline) gestito tramite LWT
const char* TOPIC_STATUS          = "casa/salotto/termostato/status";

// Soglie di allarme: valori sensati per una casa.
// Sotto 18 -> rischio "freddo"; sopra 28 -> rischio "caldo".
const float SOGLIA_MIN = 18.0f;
const float SOGLIA_MAX = 28.0f;

const uint8_t QOS_LEVEL = 1;   // QoS 1 sulla sub: garantisce consegna almeno una volta
const int LED_PIN = 2;         // LED on-board ESP32 DevKit (GPIO2)

// =============================================================================
// STATO GLOBALE
// =============================================================================
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// =============================================================================
// CONNESSIONE WIFI
// =============================================================================
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

// =============================================================================
// CALLBACK MESSAGGI MQTT
// -----------------------------------------------------------------------------
// PubSubClient chiama questa funzione AUTOMATICAMENTE ogni volta che arriva
// un messaggio su uno dei topic a cui siamo iscritti. La firma è fissa:
//   - topic:   stringa C-style del topic ricevuto
//   - payload: bytes (NON terminato da \0! attenzione)
//   - length:  numero di byte del payload
//
// Per usare il payload come stringa lo copiamo in un buffer locale e aggiungiamo
// il terminatore '\0'.
// =============================================================================
void onMessage(char* topic, byte* payload, unsigned int length) {
  // 1) Copia sicura del payload con limite (evita buffer overflow se il
  //    publisher manda più di 31 byte).
  char buf[32] = {0};
  unsigned int n = length < sizeof(buf) - 1 ? length : sizeof(buf) - 1;
  memcpy(buf, payload, n);

  // 2) Parsing numerico safe.
  //    strtof setta 'end' = inizio del buffer se NON ha trovato alcun numero:
  //    in quel caso scartiamo il messaggio (es. payload "ABC").
  char* end;
  float t = strtof(buf, &end);
  if (end == buf) {
    Serial.printf("[WARN] Payload non numerico su %s: '%s'\n", topic, buf);
    return;
  }

  Serial.printf("[SUB] %s = %.2f °C", topic, t);

  // 3) Logica del termostato: confronto con le soglie.
  if (t < SOGLIA_MIN || t > SOGLIA_MAX) {
    const char* tipo = t < SOGLIA_MIN ? "FREDDO" : "CALDO";

    // Costruiamo un payload JSON con le informazioni utili a un consumer
    // generico (es. una dashboard) per capire e visualizzare l'allarme.
    char alarm[96];
    snprintf(alarm, sizeof(alarm),
             "{\"tipo\":\"%s\",\"valore\":%.2f,\"min\":%.1f,\"max\":%.1f}",
             tipo, t, SOGLIA_MIN, SOGLIA_MAX);

    mqtt.publish(TOPIC_PUB_ALLARME, alarm, false);
    digitalWrite(LED_PIN, HIGH);  // segnalazione locale
    Serial.printf("  *** ALLARME %s -> %s\n", tipo, alarm);
  } else {
    // Temperatura nel range: spegne il LED (chiude un eventuale allarme precedente)
    digitalWrite(LED_PIN, LOW);
    Serial.println("  OK");
  }
}

// =============================================================================
// CONNESSIONE MQTT
// -----------------------------------------------------------------------------
// IMPORTANTE: la SUBSCRIBE va rifatta ogni volta che ci si riconnette.
// Il broker NON memorizza le sottoscrizioni di un client che si disconnette
// (a meno di usare clean_session = false, qui non lo facciamo).
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
      // Iscrizione al topic della temperatura: da qui in poi onMessage()
      // verrà chiamata per ogni messaggio pubblicato dal sensore.
      mqtt.subscribe(TOPIC_SUB_TEMPERATURA, QOS_LEVEL);
      Serial.printf("[MQTT] Sottoscritto a %s\n", TOPIC_SUB_TEMPERATURA);
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
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);          // LED spento al boot
  delay(200);
  Serial.println("\n=== Termostato - MQTT Subscriber ===");
  Serial.printf("Soglie: %.1f - %.1f °C\n", SOGLIA_MIN, SOGLIA_MAX);

  connectWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(onMessage);          // registra il callback per i messaggi in arrivo
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(10);
}

// =============================================================================
// LOOP
// =============================================================================
void loop() {
  if (WiFi.status() != WL_CONNECTED) { connectWiFi(); return; }
  if (!mqtt.connected())             { connectMQTT(); }
  mqtt.loop();  // qui dentro vengono "consegnati" i messaggi alla callback
}
