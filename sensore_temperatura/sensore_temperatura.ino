/* ============================================================================
 * SENSORE TEMPERATURA - Publisher MQTT su ESP32
 * ----------------------------------------------------------------------------
 * Ruolo nel sistema:
 *   Questo nodo simula un sensore di temperatura installato in salotto.
 *   A intervalli regolari legge un valore (qui simulato) e lo PUBBLICA
 *   sul broker MQTT, in modo che qualunque client interessato
 *   (termostato, monitor, dashboard...) possa riceverlo iscrivendosi
 *   al topic corrispondente.
 *
 * Pattern MQTT:
 *   - Publisher: il sensore non sa chi lo sta ascoltando, si limita a
 *     pubblicare su un "topic" (canale gerarchico tipo file system).
 *   - Topic usato: casa/salotto/temperatura
 *   - QoS 0 in pubblicazione (fire-and-forget): per telemetria continua
 *     basta così; se un campione si perde, al prossimo ciclo ne arriva
 *     un altro.
 *   - LWT (Last Will & Testament): se il nodo muore senza disconnettersi
 *     pulitamente, è il BROKER a pubblicare "offline" sul topic di status
 *     in modo che gli altri client se ne accorgano.
 *
 * Librerie richieste (Arduino IDE -> Library Manager):
 *   - WiFi          (già inclusa nel core ESP32)
 *   - PubSubClient  (Nick O'Leary)
 *
 * Per usare un sensore REALE LM35 (TO-92, 10 mV/°C):
 *   - impostare USE_LM35 a true qui sotto
 *   - cablaggio: +Vs -> 5V (VIN), Vout -> GPIO 34, GND -> GND
 * ============================================================================ */

#include <WiFi.h>
#include <PubSubClient.h>

// =============================================================================
// CONFIGURAZIONE - da modificare in base alla propria rete
// =============================================================================

// Credenziali della rete WiFi a cui ESP32 deve collegarsi
const char* WIFI_SSID     = "TUO_WIFI";
const char* WIFI_PASSWORD = "TUA_PASSWORD";

// Indirizzo del broker Mosquitto.
// In produzione: IP del Raspberry Pi sulla LAN.
// In sviluppo:  IP della macchina dove gira Mosquitto (NON 127.0.0.1:
//               l'ESP32 lo interpreterebbe come "se stesso").
const char* MQTT_BROKER  = "192.168.1.100";
const uint16_t MQTT_PORT = 1883;  // Porta standard MQTT non cifrato

// Identificativo univoco di questo client sul broker.
// Due client con lo stesso ID si "kickano" a vicenda: ogni nodo deve averne uno diverso.
const char* MQTT_CLIENT_ID = "esp32-sensore-temperatura";

// Topic MQTT.
// Convenzione: gerarchia "casa/<stanza>/<grandezza>" per permettere wildcard
// (es. il monitor si iscrive a "casa/#" e riceve TUTTO).
const char* TOPIC_TEMPERATURA = "casa/salotto/temperatura";
const char* TOPIC_STATUS      = "casa/salotto/temperatura/status";

// Ogni quanto pubblicare una nuova lettura (in millisecondi)
const unsigned long PUBLISH_INTERVAL_MS = 5000;

// Sensore reale LM35: imposta a true per leggere dall'hardware invece di simulare.
// Cablaggio: +Vs -> 5V, Vout -> GPIO 34, GND -> GND.
const bool USE_LM35   = false;
const int  LM35_PIN   = 34;       // GPIO 34 (ADC1, input only) — sicuro col WiFi attivo
const float ADC_VREF  = 3.3f;     // tensione di riferimento ADC
const int   ADC_MAX   = 4095;     // ADC 12 bit (0..4095)
const int   LM35_SAMPLES = 16;    // media di 16 letture per ridurre rumore

// QoS (Quality of Service) usato per i messaggi di stato:
//   0 = at most once  (fire-and-forget, può perdersi)
//   1 = at least once (con ack, può duplicarsi)         <-- scelto qui
//   2 = exactly once  (handshake 4-vie, più overhead)
const uint8_t QOS_LEVEL = 1;

// =============================================================================
// STATO GLOBALE
// =============================================================================

WiFiClient wifiClient;             // socket TCP sottostante
PubSubClient mqtt(wifiClient);     // client MQTT che usa quel socket
unsigned long lastPublish = 0;     // timestamp dell'ultima pubblicazione (per scheduling non bloccante)

// =============================================================================
// CONNESSIONE WIFI
// -----------------------------------------------------------------------------
// Si tenta la connessione per max 20 secondi. Se fallisce non si blocca per
// sempre: si torna al loop() che ritenterà al ciclo successivo. Questo evita
// che il dispositivo si freezi se l'access point è momentaneamente irraggiungibile.
// =============================================================================
// Stampa l'evento WiFi (utile per capire perché una connessione fallisce).
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.printf("[WiFi] DISCONNECT reason=%d\n",
                    info.wifi_sta_disconnected.reason);
      // reason 2  = AUTH_EXPIRE
      // reason 15 = 4WAY_HANDSHAKE_TIMEOUT (password sbagliata)
      // reason 201/202 = NO_AP_FOUND      (SSID non visto)
      // reason 205 = CONNECTION_FAIL
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[WiFi] GOT IP %s\n", WiFi.localIP().toString().c_str());
      break;
    default: break;
  }
}

// Scansiona le reti WiFi visibili e stampa per ognuna SSID, RSSI e tipo auth.
// Utile per scoprire se il nome dell'AP che stiamo cercando è realmente lì
// e con che sicurezza (WPA2, WPA3, Open, Enterprise).
void scanWiFi() {
  Serial.println("[SCAN] Cerco reti visibili...");
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();
  if (n <= 0) { Serial.println("[SCAN] nessuna rete trovata"); return; }
  for (int i = 0; i < n; i++) {
    const char* auth = "?";
    switch (WiFi.encryptionType(i)) {
      case WIFI_AUTH_OPEN:            auth = "OPEN"; break;
      case WIFI_AUTH_WEP:             auth = "WEP"; break;
      case WIFI_AUTH_WPA_PSK:         auth = "WPA"; break;
      case WIFI_AUTH_WPA2_PSK:        auth = "WPA2"; break;
      case WIFI_AUTH_WPA_WPA2_PSK:    auth = "WPA/WPA2"; break;
      case WIFI_AUTH_WPA2_ENTERPRISE: auth = "WPA2-ENT"; break;
      case WIFI_AUTH_WPA3_PSK:        auth = "WPA3"; break;
      case WIFI_AUTH_WPA2_WPA3_PSK:   auth = "WPA2/WPA3"; break;
      default: break;
    }
    Serial.printf("  [%2d] RSSI=%4d  %-10s  '%s'\n",
                  i, WiFi.RSSI(i), auth, WiFi.SSID(i).c_str());
  }
  WiFi.scanDelete();
}

void connectWiFi() {
  Serial.println();
  Serial.printf("[WiFi] MAC ESP32: %s\n", WiFi.macAddress().c_str());
  scanWiFi();
  // Stampa SSID byte-per-byte per scoprire eventuali caratteri invisibili
  Serial.printf("[WiFi] SSID len=%d: '%s'\n", (int)strlen(WIFI_SSID), WIFI_SSID);
  Serial.print("[WiFi] SSID bytes:");
  for (size_t i = 0; i < strlen(WIFI_SSID); i++) {
    Serial.printf(" %02X", (uint8_t)WIFI_SSID[i]);
  }
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFi.onEvent(onWiFiEvent);
  WiFi.disconnect(true, true);   // pulisce credenziali salvate in NVS
  delay(200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("[WiFi] Connessione");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] OK  IP=%s  RSSI=%d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.printf("\n[WiFi] FALLITO (status=%d), riprovo\n", WiFi.status());
  }
}

// =============================================================================
// CONNESSIONE MQTT
// -----------------------------------------------------------------------------
// Si rimane nel while finché non si è connessi, MA con uscita di sicurezza
// se nel frattempo cade anche il WiFi (altrimenti si entra in un loop infinito).
//
// Particolarità: passiamo Last Will & Testament al broker. Significa:
// "se ti accorgi che mi sono disconnesso male, pubblica TU questo messaggio
//  ('offline') su questo topic con flag retained".
// Appena la connessione va a buon fine, sovrascriviamo lo status con "online".
// =============================================================================
void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.printf("[MQTT] Connessione a %s:%u ... ", MQTT_BROKER, MQTT_PORT);

    bool connected = mqtt.connect(
      MQTT_CLIENT_ID,
      nullptr, nullptr,            // username/password: non usati (broker aperto)
      TOPIC_STATUS,                // willTopic
      QOS_LEVEL,                   // willQos
      true,                        // willRetain = true: chi si iscrive dopo legge subito lo stato
      "offline"                    // willMessage
    );

    if (connected) {
      Serial.println("OK");
      // Pubblichiamo subito "online" come retained: sovrascrive il "offline" eventuale
      mqtt.publish(TOPIC_STATUS, "online", true);
    } else {
      // state() ritorna un codice negativo che spiega cosa è andato storto
      // (es. -2 = connessione TCP fallita, -4 = timeout, 5 = non autorizzato).
      Serial.printf("FAIL rc=%d, retry in 3s\n", mqtt.state());
      delay(3000);
      // Se nel frattempo è caduto anche il WiFi, usciamo: il loop principale
      // gestirà prima la riconnessione WiFi e poi ritenterà MQTT.
      if (WiFi.status() != WL_CONNECTED) return;
    }
  }
}

// =============================================================================
// LETTURA REALE LM35
// -----------------------------------------------------------------------------
// LM35: uscita lineare 10 mV/°C, riferita a GND.
//   V_out = T_C * 0.010   =>   T_C = V_out / 0.010 = V_out * 100
// Calcolo della tensione dalla lettura ADC:
//   V_out = analogRead() * V_REF / ADC_MAX
// Per ridurre il rumore facciamo una media su LM35_SAMPLES letture.
// =============================================================================
float readTemperatureLM35() {
  uint32_t acc = 0;
  for (int i = 0; i < LM35_SAMPLES; i++) {
    acc += analogRead(LM35_PIN);
  }
  float raw = acc / (float)LM35_SAMPLES;
  float vout = raw * ADC_VREF / ADC_MAX;
  return vout * 100.0f;   // °C
}

// =============================================================================
// LETTURA SIMULATA DELLA TEMPERATURA (fallback didattico)
// -----------------------------------------------------------------------------
// In assenza di un sensore reale, generiamo un valore plausibile:
//   - base che oscilla lentamente tra 18 e 28 gradi (deriva casuale)
//   - ogni tanto (1 volta su 20) un picco anomalo > 30 gradi per testare
//     che il termostato lo rilevi come allarme.
// =============================================================================
float readTemperatureSimulated() {
  static float base = 22.0f;
  base += (random(-100, 101) / 1000.0f);
  if (base < 18) base = 18;
  if (base > 28) base = 28;
  if (random(0, 20) == 0) {
    return base + random(5, 12);
  }
  return base;
}

// Dispatcher: sceglie la sorgente in base al flag USE_LM35
float readTemperature() {
  return USE_LM35 ? readTemperatureLM35() : readTemperatureSimulated();
}

// =============================================================================
// PUBBLICAZIONE DEL VALORE
// -----------------------------------------------------------------------------
// Convertiamo il float in stringa con 2 decimali (payload MQTT è binario,
// ma per leggibilità qui usiamo ASCII).
// =============================================================================
void publishTemperature(float t) {
  char payload[16];
  snprintf(payload, sizeof(payload), "%.2f", t);

  // publish() ritorna true se il messaggio è stato inviato al broker.
  // NB: con QoS 0 (default qui) non c'è ack, quindi "true" significa solo
  // "scritto sul socket TCP", non "consegnato al broker".
  bool ok = mqtt.publish(TOPIC_TEMPERATURA, payload, false);

  Serial.printf("[PUB] %s -> %s  %s\n",
                TOPIC_TEMPERATURA, payload, ok ? "OK" : "FAIL");
}

// =============================================================================
// SETUP - eseguito una sola volta al boot
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Sensore Temperatura - MQTT Publisher ===");

  // Seed PRNG con rumore hardware: numeri "più casuali" tra un boot e l'altro
  randomSeed(esp_random());

  if (USE_LM35) {
    analogReadResolution(12);                 // 0..4095
    analogSetPinAttenuation(LM35_PIN, ADC_11db); // full-scale ~3.3V su quel pin
    Serial.printf("[LM35] Lettura reale attiva su GPIO %d\n", LM35_PIN);
  } else {
    Serial.println("[SIM] Lettura simulata (USE_LM35=false)");
  }

  connectWiFi();

  // Configurazione client MQTT
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setKeepAlive(30);      // ping al broker ogni 30s (default 15s)
  mqtt.setSocketTimeout(10);  // dopo 10s di silenzio sul socket -> disconnesso
}

// =============================================================================
// LOOP - eseguito ripetutamente
// -----------------------------------------------------------------------------
// Pattern non bloccante: niente delay() lunghi nel loop principale, così
// MQTT.loop() viene chiamato spesso e il client risponde tempestivamente
// ai PING e mantiene la connessione viva.
// =============================================================================
void loop() {
  // 1) Se il WiFi è caduto, ritenta e ricomincia il ciclo
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    return;
  }

  // 2) Se MQTT è caduto, ritenta
  if (!mqtt.connected()) {
    connectMQTT();
  }

  // 3) mqtt.loop() processa pacchetti in ingresso e mantiene il keep-alive.
  //    DEVE essere chiamato regolarmente.
  mqtt.loop();

  // 4) Scheduling temporale non bloccante con millis():
  //    pubblica solo se è passato PUBLISH_INTERVAL_MS dall'ultima volta.
  unsigned long now = millis();
  if (now - lastPublish >= PUBLISH_INTERVAL_MS) {
    lastPublish = now;
    float t = readTemperature();
    publishTemperature(t);
  }
}
