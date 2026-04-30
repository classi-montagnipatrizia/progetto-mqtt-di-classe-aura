#!/usr/bin/env python3
"""
Sensore Temperatura - MQTT Publisher
Pubblica un valore di temperatura simulato (o da DHT22) ogni 10 secondi
sul topic 'casa/salotto/temperatura'

Requisiti: pip install paho-mqtt
"""

import paho.mqtt.client as mqtt
import time
import random
import sys

# ============================================================
# CONFIGURAZIONE BROKER
# ============================================================
BROKER_HOST = "localhost"
BROKER_PORT = 1883
BROKER_KEEPALIVE = 60

# Topic MQTT dove pubblicare
TOPIC_TEMPERATURA = "casa/salotto/temperatura"

# Parametri pubblicazione
INTERVALLO_PUBBLICAZIONE = 10  # secondi
QOS_LEVEL = 1  # Qualità di servizio: almeno una volta

# ============================================================
# VARIABILI DI STATO
# ============================================================
is_connected = False


# ============================================================
# CALLBACK MQTT
# ============================================================

def on_connect(client, userdata, flags, rc):
    """Callback: connessione stabilita o fallita"""
    global is_connected

    if rc == 0:
        print(f"[✓] Connesso al broker MQTT su {BROKER_HOST}:{BROKER_PORT}")
        is_connected = True
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


def on_publish(client, userdata, mid):
    """Callback: messaggio pubblicato con successo"""
    print(f"[→] Messaggio pubblicato (ID: {mid})")


def on_log(client, userdata, level, buf):
    """Callback: log MQTT (opzionale, per debug)"""
    if level == mqtt.MQTT_LOG_ERR:
        print(f"[MQTT ERROR] {buf}")


# ============================================================
# FUNZIONI LETTURA TEMPERATURA
# ============================================================

def leggi_temperatura_simulata():
    """
    Simula la lettura di un sensore di temperatura.
    Restituisce un valore casuale tra 15°C e 30°C
    """
    return round(random.uniform(15, 30), 1)


# Alternativa (decommentare se usando DHT22):
# def leggi_temperatura_dht22():
#     """
#     Legge temperatura da sensore DHT22
#     Richiede: pip install Adafruit_DHT
#     """
#     import Adafruit_DHT
#     DHT_SENSOR = Adafruit_DHT.DHT22
#     DHT_PIN = 4  # GPIO4
#
#     humidity, temperature = Adafruit_DHT.read_retry(DHT_SENSOR, DHT_PIN)
#
#     if temperature is not None:
#         return round(temperature, 1)
#     else:
#         print("[!] Errore lettura sensore DHT22")
#         return None


# ============================================================
# MAIN - LOOP PRINCIPALE
# ============================================================

def main():
    """Loop principale del sensore temperatura"""

    print("=" * 60)
    print("SENSORE TEMPERATURA - MQTT Publisher")
    print("=" * 60)
    print(f"Broker: {BROKER_HOST}:{BROKER_PORT}")
    print(f"Topic: {TOPIC_TEMPERATURA}")
    print(f"Intervallo: {INTERVALLO_PUBBLICAZIONE} sec")
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
    client.on_publish = on_publish
    # client.on_log = on_log  # Decommentare per debug

    # Tenta connessione al broker
    try:
        print(f"[...] Connessione a {BROKER_HOST}:{BROKER_PORT}...")
        client.connect(BROKER_HOST, BROKER_PORT, BROKER_KEEPALIVE)
    except Exception as e:
        print(f"[✗] Errore di connessione: {e}")
        print("   Verifica che Mosquitto sia in esecuzione")
        sys.exit(1)

    # Avvia il loop di rete in background
    client.loop_start()

    try:
        # Attendi connessione
        counter = 0
        while not is_connected and counter < 5:
            time.sleep(1)
            counter += 1

        if not is_connected:
            print("[✗] Connessione al broker fallita dopo 5 secondi")
            sys.exit(1)

        # Loop di pubblicazione
        ciclo = 0
        while True:
            ciclo += 1

            # Leggi temperatura (simulata)
            temperatura = leggi_temperatura_simulata()

            # Converti a stringa per MQTT
            payload = str(temperatura)

            # Pubblica il valore
            try:
                result = client.publish(
                    TOPIC_TEMPERATURA,
                    payload,
                    qos=QOS_LEVEL,
                    retain=False
                )

                if result.rc == mqtt.MQTT_ERR_SUCCESS:
                    timestamp = time.strftime("%H:%M:%S")
                    print(f"[{timestamp}] Ciclo {ciclo}: {temperatura}°C pubblicato")
                else:
                    print(f"[!] Errore pubblicazione (Codice: {result.rc})")

            except Exception as e:
                print(f"[✗] Errore durante pubblicazione: {e}")

            # Attendi prima di prossima pubblicazione
            time.sleep(INTERVALLO_PUBBLICAZIONE)

    except KeyboardInterrupt:
        print("\n\n[!] Interruzione utente (Ctrl+C)")

    except Exception as e:
        print(f"\n[✗] Errore inaspettato: {e}")

    finally:
        # Chiusura pulita
        print("[...] Chiudo connessione...")
        client.loop_stop()
        client.disconnect()
        time.sleep(1)
        print("[✓] Disconnesso. Arrivederci!")


# ============================================================
# ENTRY POINT
# ============================================================

if __name__ == "__main__":
    main()
