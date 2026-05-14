# Casa Intelligente — Sistema IoT MQTT su ESP32

Sistema IoT didattico per una casa intelligente basato su **MQTT** e microcontrollori **ESP32** (Arduino framework). Il broker **Mosquitto** gira su un Raspberry Pi (o su localhost per sviluppo); i nodi pubblicano e sottoscrivono eventi sotto il namespace `casa/`.

## Architettura

```
                      ┌─────────────────────────┐
                      │  Broker MQTT (Mosquitto)│
                      │  Raspberry Pi  :1883    │
                      └────────────┬────────────┘
                                   │
        ┌────────────┬─────────────┼──────────────┬──────────────┐
        │            │             │              │              │
   ┌────┴────┐  ┌────┴────┐   ┌────┴────┐   ┌─────┴────┐
   │ ESP32   │  │ ESP32   │   │ ESP32   │   │ ESP32    │
   │ Temp.   │  │ Luce    │   │ Termo-  │   │ Monitor  │
   │ (pub)   │  │ (pub)   │   │ stato   │   │ casa/#   │
   │         │  │         │   │ (sub)   │   │ (sub)    │
   └─────────┘  └─────────┘   └─────────┘   └──────────┘
```

## Nodi

| Sketch | Ruolo | Topic |
|---|---|---|
| `sensore_temperatura/` | Publisher | pub `casa/salotto/temperatura` |
| `sensore_luce/` | Publisher | pub `casa/salotto/luce` |
| `termostato/` | Subscriber + Publisher | sub `casa/salotto/temperatura`, pub `casa/salotto/termostato/allarme` |
| `monitor/` | Subscriber wildcard | sub `casa/#` |

Ogni nodo pubblica anche uno stato retained su `casa/<nodo>/status` (`online`/`offline`) tramite **Last Will & Testament**.

## Topic MQTT

| Topic | Direzione | Payload |
|---|---|---|
| `casa/salotto/temperatura` | sensore → broker | float (°C), es. `23.45` |
| `casa/salotto/luce` | sensore → broker | float (lux), es. `512.0` |
| `casa/salotto/termostato/allarme` | termostato → broker | JSON `{"tipo":"CALDO","valore":31.2,"min":18.0,"max":28.0}` |
| `casa/<nodo>/status` | nodo → broker (retained, LWT) | `online` / `offline` |
| `casa/#` | broker → monitor | tutti i messaggi sopra |

## Hardware

- 4× **ESP32** (DevKit v1 o equivalente)
- 1× **Raspberry Pi** (qualsiasi modello con rete) per il broker — opzionale in sviluppo
- Cavi micro-USB per alimentazione/flash

I sensori sono **simulati** via `random()` come da consegna scolastica. Per uso reale:
- temperatura: DHT22 / DS18B20 (vedi nota nel `.ino`)
- luminosità: fotoresistore (LDR) su ADC, oppure BH1750 via I²C

## Cablaggio

**Il Raspberry e l'ESP32 non si collegano fisicamente fra loro**: comunicano via WiFi attraverso il broker MQTT, devono solo trovarsi sulla stessa LAN.

### Raspberry Pi (broker)

| Pin / porta | Uso |
|---|---|
| USB‑C (Pi 4/5) o micro‑USB (Pi 3) | Alimentazione |
| Ethernet o WiFi | Rete LAN (stessa dell'ESP32) |

Nessun GPIO utilizzato: Mosquitto gira come servizio sulla porta TCP `1883`.

### ESP32 DevKit v1 — pin usati negli sketch attuali

| Pin | Funzione | Sketch |
|---|---|---|
| `GPIO 2` | LED on‑board (indicatore allarme) | `termostato.ino` |
| `5V` / `3V3` / `GND` | Alimentazione | tutti |
| micro‑USB | Alimentazione + flash + Serial Monitor | tutti |

I sensori sono **simulati** con `random()`, quindi non serve cablare nulla per testare il sistema.

### Cablaggio per hardware reale (opzionale)

**DHT22 — temperatura** (sostituisce `readTemperature()` nello sketch del sensore temperatura):

| DHT22 | ESP32 |
|---|---|
| pin 1 (VCC) | `3V3` |
| pin 2 (DATA) | `GPIO 4` + resistenza pull‑up 10 kΩ verso `3V3` |
| pin 4 (GND) | `GND` |

**Fotoresistore LDR — luminosità** (sostituisce `readLight()` nello sketch luce):

| LDR | ESP32 |
|---|---|
| un capo | `3V3` |
| altro capo | `GPIO 34` (ADC1) + resistenza 10 kΩ verso `GND` (partitore di tensione) |

> **Nota**: su ESP32 con WiFi attivo usare solo pin `ADC1` (GPIO 32–39) per `analogRead`. ADC2 entra in conflitto col WiFi.

**LED/buzzer esterno per allarme termostato** (in aggiunta al LED on‑board):

| Componente | ESP32 |
|---|---|
| Anodo LED (via R 220 Ω) | `GPIO 5` |
| Catodo LED | `GND` |

## Software richiesto

- **Arduino IDE 2.x** (o PlatformIO)
- Board manager: `esp32 by Espressif Systems`
- Librerie:
  - `PubSubClient` (Nick O'Leary)

## Setup rapido

1. **Broker** — installare Mosquitto sul Raspberry: vedi [`broker_config.md`](./broker_config.md).
2. **Sketch** — in ognuno dei 4 `.ino` modificare la sezione di configurazione:
   ```cpp
   const char* WIFI_SSID     = "TUO_WIFI";
   const char* WIFI_PASSWORD = "TUA_PASSWORD";
   const char* MQTT_BROKER   = "192.168.1.100"; // IP del Raspberry
   ```
3. **Flash** — selezionare board "ESP32 Dev Module", porta seriale, e caricare.
4. **Monitor seriale** a 115200 baud per vedere i log di ciascun nodo.

## Gestione errori e robustezza

Tutti gli sketch implementano:

- **Riconnessione WiFi automatica** con timeout e ritentativi nel `loop()`.
- **Riconnessione MQTT automatica** con backoff (3 s) e log dei codici di errore (`PubSubClient::state()`).
- **Last Will & Testament**: se un nodo si disconnette anormalmente, il broker pubblica `offline` (retained) sul topic di status del nodo.
- **Validazione del payload** ricevuto (termostato): parsing numerico safe con `strtof` e scarto messaggi non validi.
- **Buffer fissi** con `snprintf`/limiti di lunghezza, niente `String` concatenate in hot path.
- **Keep-alive 30 s** + `socketTimeout` per rilevare disconnessioni silenziose.
- **QoS 1** sulle sottoscrizioni critiche (consegna almeno una volta).
- **`messageCount`** nel monitor per individuare eventi persi.
- **Retained status** per conoscere lo stato dei nodi anche dopo il boot del monitor.

## Test del sistema

```bash
# Terminale 1: ascolta tutto
mosquitto_sub -h <IP_BROKER> -t "casa/#" -v

# Terminale 2: simula un picco anomalo per attivare l'allarme
mosquitto_pub -h <IP_BROKER> -t "casa/salotto/temperatura" -m "35.0"
```

Il monitor seriale del **termostato** deve mostrare `*** ALLARME CALDO` e su `casa/salotto/termostato/allarme` compare il JSON dell'allarme; il **monitor** logga entrambi i messaggi.

## Struttura repo

```
.
├── README.md
├── broker_config.md
├── sensore_temperatura/sensore_temperatura.ino
├── sensore_luce/sensore_luce.ino
├── termostato/termostato.ino
└── monitor/monitor.ino
```
