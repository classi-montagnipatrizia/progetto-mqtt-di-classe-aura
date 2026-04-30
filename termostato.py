#!/usr/bin/env python3
"""
Termostato - MQTT Subscriber
Si abbona al topic 'casa/salotto/temperatura' e monitora la temperatura.
Genera un allarme se la temperatura supera 26°C.

Requisiti: pip install paho-mqtt
"""

import paho.mqtt.client as mqtt
import time
import sys

# ============================================================
# CONFIGURAZIONE BROKER
# ============================================================
BROKER_HOST = "localhost"
BROKER_PORT = 1883
BROKER_KEEPALIVE = 60

# Topic MQTT da monitorare
TOPIC_MONITORAGGIO = "casa/salotto/temperatura"

# Parametri termostato
SOGLIA_ALLARME = 26.0  # gradi Celsius
QOS_LEVEL = 1

# ============================================================
# VARIABILI DI STATO
# ============================================================
is_connected = False
ultima_temperatura = None


# ============================================================
# CALLBACK MQTT
# ============================================================

def on_connect(client, userdata, flags, rc):
    """Callback: connessione stabilita o fallita"""
    global is_connected

    if rc == 0:
        print(f"[✓] Connesso al broker MQTT su {BROKER_HOST}:{BROKER_PORT}")
        print(f"[✓] Sottoscrizione a: {TOPIC_MONITORAGGIO}")
        is_connected = True

        # Sottoscrizione al topic una volta connesso
        client.subscribe(TOPIC_MONITORAGGIO, qos=QOS_LEVEL)
    else:
        print(f"[✗] Errore di connessione (Codice: {rc})")
        is_connected = False


def on_disconnect(client, userdata, rc):
    """Callback: disconnessione dal broker"""
    global is_connected

    if rc != 0:
        print(f"[!] Disconnessione inaspettata (Codice: {rc})")
    else:
        print(f"[!] Disconnessione volontaria")

    is_connected = False


def on_message(client, userdata, msg):
    """
    Callback: messaggio ricevuto dal broker
    Analizza la temperatura e genera allarmi se necessario
    """
    global ultima_temperatura

    try:
        # Decodifica messaggio
        payload = msg.payload.decode("utf-8")
        topic = msg.topic

        # Converti temperatura da stringa a float
        try:
            temperatura = float(payload)
        except ValueError:
            print(f"[!] Errore: valore non numerico ricevuto: '{payload}'")
            return

        # Salva valore
        ultima_temperatura = temperatura

        # Ottieni timestamp
        timestamp = time.strftime("%H:%M:%S")

        # Verifica soglia allarme
        if temperatura > SOGLIA_ALLARME:
            # ⚠️ ALLARME TEMPERATURA ALTA
            print(f"\n{'='*60}")
            print(f"[{timestamp}] ⚠️  ALLARME: TEMPERATURA ELEVATA!")
            print(f"{'='*60}")
            print(f"Topic: {topic}")
            print(f"Temperatura attuale: {temperatura}°C")
            print(f"Soglia allarme: {SOGLIA_ALLARME}°C")
            print(f"ECCEDENZA: {temperatura - SOGLIA_ALLARME:.1f}°C")
            print(f"AZIONE CONSIGLIATA: Accendere condizionatore")
            print(f"{'='*60}\n")

        else:
            # ✓ Temperatura nella norma
            print(f"[{timestamp}] ✓ Temperatura OK: {temperatura}°C " +
                  f"(Soglia: {SOGLIA_ALLARME}°C)")

    except Exception as e:
        print(f"[✗] Errore nell'elaborazione messaggio: {e}")


def on_log(client, userdata, level, buf):
    """Callback: log MQTT (opzionale, per debug)"""
    if level == mqtt.MQTT_LOG_ERR:
        print(f"[MQTT ERROR] {buf}")


# ============================================================
# MAIN - LOOP PRINCIPALE
# ============================================================

def main():
    """Loop principale del termostato"""

    print("=" * 60)
    print("TERMOSTATO - MQTT Subscriber")
    print("=" * 60)
    print(f"Broker: {BROKER_HOST}:{BROKER_PORT}")
    print(f"Monitoraggio: {TOPIC_MONITORAGGIO}")
    print(f"Soglia allarme: {SOGLIA_ALLARME}°C")
    print("Premere Ctrl+C per interrompere")
    print("=" * 60)
    print()

    # Crea client MQTT
    try:
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
    except AttributeError:
        # Fallback per versioni vecchie di paho-mqtt
        client = mqtt.Client()

    # Registra callback
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    # client.on_log = on_log  # Decommentare per debug

    # Tenta connessione al broker
    try:
        print(f"[...] Connessione a {BROKER_HOST}:{BROKER_PORT}...")
        client.connect(BROKER_HOST, BROKER_PORT, BROKER_KEEPALIVE)
    except Exception as e:
        print(f"[✗] Errore di connessione: {e}")
        print("   Verifica che Mosquitto sia in esecuzione")
        sys.exit(1)

    try:
        # Avvia il loop di rete (bloccante)
        # loop_forever() mantiene la connessione attiva e processa i messaggi

        print("[...] In ascolto per messaggi di temperatura...\n")
        client.loop_forever()

    except KeyboardInterrupt:
        print("\n\n[!] Interruzione utente (Ctrl+C)")

    except Exception as e:
        print(f"\n[✗] Errore inaspettato: {e}")

    finally:
        # Chiusura pulita
        print("[...] Chiudo connessione...")
        client.disconnect()
        time.sleep(1)

        if ultima_temperatura is not None:
            print(f"[ℹ] Ultima temperatura registrata: {ultima_temperatura}°C")

        print("[✓] Disconnesso. Arrivederci!")


# ============================================================
# ENTRY POINT
# ============================================================

if __name__ == "__main__":
    main()
