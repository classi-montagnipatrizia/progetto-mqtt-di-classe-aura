[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/XsB2LnaZ)
[![Open in Visual Studio Code](https://classroom.github.com/assets/open-in-vscode-2e0aaae1b6195c2367325f4f02e2d04e9abb55f0b24a779b69b11b9e10269abc.svg)](https://classroom.github.com/online_ide?assignment_repo_id=23688769&assignment_repo_type=AssignmentRepo)

# Progetto Smart Home con MQTT

## 📋 Descrizione Progetto
Sistema IoT completo per una casa intelligente basato su MQTT con:
- Sensori di temperatura e luminosità che pubblicano dati periodicamente
- Termostato che monitora anomalie di temperatura
- Monitor centralizzato che logga tutti gli eventi della casa
- Broker MQTT locale Mosquitto su Raspberry Pi (o localhost per sviluppo)

---

## 🎯 Architettura e Specifiche

### Configurazione Broker MQTT
**File:** `broker_config.md`

- **Indirizzo:** localhost
- **Porta:** 1883
- **Autenticazione:** Disabilitata (ambiente di test)
- **Software:** Mosquitto (MQTT broker)
- **Target:** Raspberry Pi (con istruzioni di installazione e configurazione)
- **Contenuti richiesti:**
  - Installazione di Mosquitto su Raspberry Pi
  - Configurazione del file `/etc/mosquitto/mosquitto.conf`
  - Comandi per avviare/fermare il broker
  - Verifica della connessione con client di test

---

## 🔧 Sensori e Script

### 1. Sensore Temperatura - `sensore_temperatura.py`

**Funzionalità:**
- Pubblica un valore di temperatura ogni 10 secondi
- Topic MQTT: `casa/salotto/temperatura`
- Libreria MQTT: `paho-mqtt`

**Fonte dati:**
- Opzione 1: Simulazione con numero casuale (15-30°C)
- Opzione 2: Sensore reale DHT22 collegato a GPIO (se disponibile)

**Requisiti tecnici:**
- Gestione errori di connessione al broker
- Riconnessione automatica in caso di disconnessione
- Commenti in italiano
- Funzionamento indipendente
- Log dei messaggi pubblicati

**Dipendenze Python:**
- `paho-mqtt`
- `time` (per delay)
- `random` (per simulazione) o `Adafruit_DHT` (per sensore reale)

---

### 2. Sensore Luce - `sensore_luce.py`

**Funzionalità:**
- Pubblica un valore di luminosità (lux) ogni 10 secondi
- Topic MQTT: `casa/salotto/luce`
- Libreria MQTT: `paho-mqtt`

**Fonte dati:**
- Simulazione con numero casuale (0-1000 lux)
- Alternativa: Sensore BH1750 o LDR su pin analogico

**Requisiti tecnici:**
- Same as sensore_temperatura.py (gestione errori, reconnect, commenti, indipendenza)

**Dipendenze Python:**
- `paho-mqtt`
- `time`
- `random`

---

### 3. Termostato - `termostato.py`

**Funzionalità:**
- Subscriber al topic: `casa/salotto/temperatura`
- Monitora i valori ricevuti
- Stampa un avviso quando la temperatura supera 26°C
- Attivo continuamente in ascolto

**Logica:**
```
Se temperatura > 26°C:
    Stampa: "⚠️ ALLARME: Temperatura salotto elevata! {valore}°C"
Altrimenti:
    Stampa: "✓ Temperatura OK: {valore}°C"
```

**Requisiti tecnici:**
- Gestione errori di connessione
- Riconnessione automatica
- Commenti in italiano
- Callback per messaggi ricevuti
- Funzionamento indipendente

**Dipendenze Python:**
- `paho-mqtt`
- `time` (per keepalive)

---

### 4. App Monitor - `app_monitor.py`

**Funzionalità:**
- Subscriber con wildcard: `casa/#` (ascolta TUTTI i topic sotto casa/)
- Logga ogni messaggio ricevuto con timestamp
- Formato log: `[HH:MM:SS] Topic: {topic} → Valore: {payload}`

**Requisiti tecnici:**
- Gestione errori di connessione
- Riconnessione automatica
- Timestamp formattato
- Log a console e/o file (opzionale: su file `mqtt_events.log`)
- Commenti in italiano
- Funzionamento indipendente

**Dipendenze Python:**
- `paho-mqtt`
- `datetime` (per timestamp)
- `time`

---

## 🏗️ Struttura di Cartelle

```
progetto-mqtt-di-classe-aura/
├── README.md (questo file con specifiche)
├── broker_config.md
├── sensore_temperatura.py
├── sensore_luce.py
├── termostato.py
├── app_monitor.py
└── mqtt_events.log (generato da app_monitor.py)
```

---

## 📦 Dipendenze Comuni

Tutti gli script Python richiedono:
```bash
pip install paho-mqtt
```

Opzionali (per sensori veri):
```bash
pip install Adafruit_DHT      # Sensore temperatura DHT22
pip install BH1750            # Sensore luce BH1750
```

---

## 🚀 Modalità di Utilizzo

### Fase 1: Avviare il Broker
1. Installare Mosquitto seguendo `broker_config.md`
2. Lanciare il broker: `mosquitto` (o `mosquitto -v` per verbose)
3. Verificare con: `mosquitto_sub -h localhost -p 1883 -t "#"`

### Fase 2: Eseguire gli Script

**Terminal 1 - Sensore Temperatura:**
```bash
python3 sensore_temperatura.py
```

**Terminal 2 - Sensore Luce:**
```bash
python3 sensore_luce.py
```

**Terminal 3 - Termostato:**
```bash
python3 termostato.py
```

**Terminal 4 - App Monitor:**
```bash
python3 app_monitor.py
```

Osservare i dati pubblicati e gli allarmi in tempo reale.

---

## 🔌 Topic MQTT - Struttura

| Topic | Producer | Consumer | Tipo Dato | Esempio |
|-------|----------|----------|-----------|---------|
| `casa/salotto/temperatura` | sensore_temperatura.py | termostato.py, app_monitor.py | float | `22.5` |
| `casa/salotto/luce` | sensore_luce.py | app_monitor.py | int | `750` |
| `casa/#` | tutti i sensori | app_monitor.py (wildcard) | vari | tutti |

---

## ⚙️ Parametri Configurabili

- **Intervallo pubblicazione sensori:** 10 sec (modificabile in sensore_temperatura.py e sensore_luce.py)
- **Soglia allarme termostato:** 26°C (modificabile in termostato.py)
- **Valori simulati:**
  - Temperatura: 15-30°C
  - Luce: 0-1000 lux
- **QoS MQTT:** 1 (consegna garantita almeno una volta)
- **Keep-alive:** 60 sec (standard paho-mqtt)

---

## ✅ Checklist Implementazione

Per ogni script Python:
- [ ] Importazioni necessarie (paho-mqtt, time, datetime, random, etc.)
- [ ] Configurazione connessione broker (host, port, QoS)
- [ ] Callback on_connect (gestire successo/errore connessione)
- [ ] Callback on_message (per subscriber)
- [ ] Callback on_disconnect (gestire disconnessioni)
- [ ] Try-except per eccezioni
- [ ] Commenti in italiano su logica principale
- [ ] Loop principale con try-finally o while True
- [ ] Controllo delay/intervallo (10 sec per sensori)
- [ ] Print informativi su console

---

## 🐛 Gestione Errori Previsti

1. **Connessione rifiutata:** Broker non avviato → istruzioni in broker_config.md
2. **Connection lost:** Rete instabile → riconnessione automatica
3. **Message not received:** Verificare topic e wildcards
4. **Sensori non disponibili:** Usare simulazione (random)
5. **Port already in use:** Cambiar porta o killare processo precedente

---

## 📖 Riferimenti Paho-MQTT

- **Client:** `mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)`
- **Connect:** `client.connect(host, port, keepalive)`
- **Subscribe:** `client.subscribe(topic, qos=1)`
- **Publish:** `client.publish(topic, payload, qos=1)`
- **Loop:** `client.loop_forever()` o `client.loop_start()`
- **Disconnect:** `client.disconnect()`

---

## 🎤 Note per lo Sviluppo

- Testare ogni script singolarmente prima di integrarli
- Usare `mosquitto_sub` da terminale per debug
- Abilitare callback `on_log` per vedere debug dettagliati
- Timestamp UTC coerente tra script
- Payload come stringa (es: `"22.5"` non `22.5` in bytes)
- Gestire graceful shutdown (Ctrl+C) con try-finally
