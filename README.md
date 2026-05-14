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

## Checklist di setup completa

Segui i passi nell'ordine. Spunta `[x]` man mano che procedi.

### Fase 1 — Materiale da procurarsi

- [ ] 4 × ESP32 DevKit v1 (o equivalente con WiFi)
- [ ] 4 × cavi micro‑USB (dati, non solo alimentazione)
- [ ] 1 × Raspberry Pi (qualsiasi modello con rete) + alimentatore + microSD ≥ 8 GB
- [ ] 1 × router/switch (o WiFi della scuola) a cui collegare tutti i dispositivi sulla **stessa LAN**
- [ ] PC con Windows / macOS / Linux per flashare gli ESP32
- [ ] *(opzionale, per hardware reale)* DHT22, fotoresistore + R 10 kΩ, LED + R 220 Ω, breadboard, jumper

### Fase 2 — Preparare il Raspberry Pi (broker)

- [ ] Flashare Raspberry Pi OS sulla microSD (Raspberry Pi Imager)
- [ ] Avviare il Pi, connetterlo alla LAN (Ethernet o WiFi)
- [ ] Aprire un terminale (SSH o monitor diretto) e aggiornare il sistema:
      `sudo apt update && sudo apt upgrade -y`
- [ ] Installare Mosquitto:
      `sudo apt install -y mosquitto mosquitto-clients`
- [ ] Creare la configurazione `/etc/mosquitto/conf.d/local.conf` (vedi [`broker_config.md`](./broker_config.md)) con `listener 1883 0.0.0.0` e `allow_anonymous true`
- [ ] Abilitare e avviare al boot: `sudo systemctl enable --now mosquitto`
- [ ] Verificare che sia attivo: `sudo systemctl status mosquitto`
- [ ] Annotare l'IP del Pi: `hostname -I` → es. `192.168.1.100`
- [ ] *(se UFW attivo)* aprire la porta: `sudo ufw allow 1883/tcp`
- [ ] Test broker da Pi stesso (in due shell):
  - shell A: `mosquitto_sub -h localhost -t "test"`
  - shell B: `mosquitto_pub -h localhost -t "test" -m "ok"` → A deve stampare `ok`

### Fase 3 — Preparare l'ambiente di sviluppo (PC)

- [ ] Installare **Arduino IDE 2.x** da [arduino.cc/en/software](https://www.arduino.cc/en/software)
- [ ] Aprire `File → Preferences` e aggiungere alla casella *Additional boards manager URLs*:
      `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
- [ ] `Tools → Board → Boards Manager`, cercare `esp32` e installare il pacchetto **esp32 by Espressif Systems**
- [ ] `Tools → Manage Libraries`, cercare e installare **PubSubClient** (Nick O'Leary)
- [ ] *(macOS)* installare i driver USB‑UART (CP210x o CH340) se la porta non compare
- [ ] *(Windows)* installare il driver del chip USB‑UART del proprio ESP32 (CP210x o CH340)

### Fase 4 — Configurare gli sketch

- [ ] Clonare/scaricare questa repo sul PC
- [ ] Aprire i 4 file `.ino` (uno per cartella) e in ciascuno modificare:
  ```cpp
  const char* WIFI_SSID     = "TUO_WIFI";       // SSID della LAN
  const char* WIFI_PASSWORD = "TUA_PASSWORD";   // password WiFi
  const char* MQTT_BROKER   = "192.168.1.100";  // IP del Raspberry annotato prima
  ```
- [ ] *(opzionale)* nel termostato regolare `SOGLIA_MIN` / `SOGLIA_MAX` se vuoi soglie diverse

### Fase 5 — Flashare gli ESP32 (uno per volta)

Per ognuno dei 4 sketch ripeti:

- [ ] Collegare l'ESP32 al PC via micro‑USB
- [ ] In Arduino IDE: `Tools → Board → ESP32 Arduino → ESP32 Dev Module`
- [ ] `Tools → Port` → selezionare la porta corrispondente (es. `/dev/cu.usbserial-XXXX` su mac, `COMx` su Windows)
- [ ] Aprire lo sketch `<nodo>/<nodo>.ino`
- [ ] Premere **Upload** (icona freccia). Se chiede, tenere premuto il tasto BOOT dell'ESP32 durante l'upload
- [ ] Aprire il **Serial Monitor** a **115200 baud**: deve apparire
      `[WiFi] OK IP=…` e `[MQTT] Connessione a 192.168.1.100:1883 ... OK`
- [ ] Etichettare fisicamente l'ESP32 (post‑it) con il ruolo flashato

Ordine consigliato di flash:

1. Monitor (così cattura anche i primi `online` degli altri)
2. Termostato
3. Sensore temperatura
4. Sensore luce

### Fase 6 — *(opzionale)* Cablaggio hardware reale

Vedi la sezione **Cablaggio** sopra. Da fare a ESP32 spento.

- [ ] Cablare DHT22 sul sensore temperatura (GPIO 4 + pull‑up 10 kΩ)
- [ ] Cablare LDR sul sensore luce (GPIO 34 + partitore 10 kΩ)
- [ ] Cablare eventuale LED esterno sul termostato (GPIO 5 + R 220 Ω)
- [ ] Modificare le funzioni `readTemperature()` / `readLight()` per usare la lettura reale (vedi commenti nei file)

### Fase 7 — Verifica finale del sistema

- [ ] Tutti i nodi sono accesi e mostrano `MQTT ... OK` sul Serial Monitor
- [ ] Sul Pi (o su un PC della LAN) lanciare:
      `mosquitto_sub -h <IP_BROKER> -t "casa/#" -v`
      → si devono vedere arrivare:
      - `casa/<nodo>/status online` (uno per ogni ESP32)
      - `casa/salotto/temperatura <valore>` ogni 5 s
      - `casa/salotto/luce <valore>` ogni 5 s
- [ ] Forzare un allarme dal PC:
      `mosquitto_pub -h <IP_BROKER> -t "casa/salotto/temperatura" -m "35.0"`
      → sul termostato compare `*** ALLARME CALDO`, il LED on‑board (GPIO 2) si accende e arriva un messaggio JSON su `casa/salotto/termostato/allarme`
- [ ] Staccare l'alimentazione di un ESP32: dopo ~45 s il suo topic `casa/<nodo>/status` deve passare a `offline` (LWT)
- [ ] Riattaccare l'alimentazione: torna `online` automaticamente

### Avvio quotidiano (a sistema già configurato)

1. Accendere il Raspberry: Mosquitto parte da solo (`enable --now` fatto in Fase 2)
2. Alimentare gli ESP32 (USB o powerbank): si connettono in autonomia, riconnessione automatica se la rete cade
3. *(per visualizzare gli eventi)* su un PC della LAN: `mosquitto_sub -h <IP_BROKER> -t "casa/#" -v`

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
