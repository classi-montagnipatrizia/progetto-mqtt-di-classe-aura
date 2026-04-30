# Configurazione Broker MQTT - Mosquitto

## 📋 Installazione

```bash
sudo apt update
sudo apt install -y mosquitto mosquitto-clients
```

---

## 🚀 Avvia il Broker

```bash
# Opzione 1: Direttamente (consigliato per test)
mosquitto -v

# Opzione 2: Come servizio
sudo systemctl start mosquitto

# Opzione 3: In background
mosquitto -d
```

---

## ⚙️ Configurazione

Configurazione predefinita funziona già per:
- **Porta:** 1883
- **Autenticazione:** Disabilitata
- **QoS:** Supportato

Se necessario modificare, edita:
```bash
sudo nano /etc/mosquitto/mosquitto.conf
```

Assicurati che contenga:
```conf
listener 1883
protocol mqtt
allow_anonymous true
```

Riavvia:
```bash
sudo systemctl restart mosquitto
```

---

## ✅ Verifica Funzionamento

**Terminal 1 (Subscriber):**
```bash
mosquitto_sub -h localhost -p 1883 -t "test/topic"
```

**Terminal 2 (Publisher):**
```bash
mosquitto_pub -h localhost -p 1883 -t "test/topic" -m "Ciao!"
```

---

## 🐛 Troubleshooting

```bash
# Porta occupata
lsof -i :1883
kill -9 <PID>

# Check stato
sudo systemctl status mosquitto

# Vedi log
sudo tail -f /var/log/mosquitto/mosquitto.log

# Stop broker
sudo systemctl stop mosquitto
```
