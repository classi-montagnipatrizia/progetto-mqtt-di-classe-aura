# Configurazione Broker MQTT — Mosquitto su Raspberry Pi

## Installazione

```bash
sudo apt update
sudo apt install -y mosquitto mosquitto-clients
```

## Configurazione di rete

Per permettere agli ESP32 della LAN di connettersi, abilitare il listener sull'IP del Raspberry e disabilitare l'autenticazione anonima solo se necessario.

`/etc/mosquitto/conf.d/local.conf`:

```conf
listener 1883 0.0.0.0
protocol mqtt
allow_anonymous true
persistence true
persistence_location /var/lib/mosquitto/
log_dest file /var/log/mosquitto/mosquitto.log
log_type all
```

Riavvio e abilitazione al boot:

```bash
sudo systemctl enable --now mosquitto
sudo systemctl restart mosquitto
sudo systemctl status mosquitto
```

Trova l'IP del Raspberry (da mettere come `MQTT_BROKER` negli sketch):

```bash
hostname -I
```

## Test rapido da CLI

Subscriber su tutta la casa:

```bash
mosquitto_sub -h <IP_BROKER> -t "casa/#" -v
```

Publisher di prova:

```bash
mosquitto_pub -h <IP_BROKER> -t "casa/salotto/temperatura" -m "23.5"
```

## Firewall (se attivo)

```bash
sudo ufw allow 1883/tcp
```

## Sviluppo su localhost (macOS)

```bash
brew install mosquitto
brew services start mosquitto
```

Negli sketch impostare `MQTT_BROKER` all'IP del Mac (non `127.0.0.1`: gli ESP non lo raggiungono).

## Troubleshooting

```bash
# Vedi log live
sudo tail -f /var/log/mosquitto/mosquitto.log

# Porta occupata
sudo lsof -i :1883

# Verifica ascolto su tutte le interfacce
ss -tlnp | grep 1883
```
