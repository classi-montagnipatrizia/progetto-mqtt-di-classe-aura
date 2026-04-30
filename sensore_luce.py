#!/usr/bin/env python3
"""
Sensore Luce - MQTT Publisher
Pubblica un valore di luminosità simulato (in lux) ogni 10 secondi
sul topic 'casa/salotto/luce'

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
TOPIC_LUCE = "casa/salotto/luce"

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
# FUNZIONI LETTURA LUMINOSITÀ
# ============================================================

def leggi_luce_simulata():
    """
    Simula la lettura di un sensore di luminosità.
    Restituisce un valore casuale tra 0 e 1000 lux

    Intervalli realistici:
    - Notte buia: 0-10 lux
    - Illuminazione interna: 50-200 lux
    - Luce naturale (interior): 200-500 lux
    - Luce solare (finestra): 500-1000+ lux
    """
    return random.randint(0, 1000)


# Alternativa (decommentare se usando BH1750):
# def leggi_luce_bh1750():
#     """
#     Legge luminosità da sensore BH1750 via I2C
#     Richiede: pip install BH1750
#     """
#     try:
#         from BH1750 import BH1750
#         sensor = BH1750(0x23)  # Indirizzo I2C default
#         lux = sensor.measure_high_res()
#         return int(lux)
#     except Exception as e:
#         print(f"[!] Errore lettura BH1750: {e}")
#         return None


# ============================================================
# MAIN - LOOP PRINCIPALE
# ============================================================

def main():
    """Loop principale del sensore luce"""

    print("=" * 60)
    print("SENSORE LUCE - MQTT Publisher")
    print("=" * 60)
    print(f"Broker: {BROKER_HOST}:{BROKER_PORT}")
    print(f"Topic: {TOPIC_LUCE}")
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

            # Leggi luminosità (simulata)
            luce = leggi_luce_simulata()

            # Converti a stringa per MQTT
            payload = str(luce)

            # Pubblica il valore
            try:
                result = client.publish(
                    TOPIC_LUCE,
                    payload,
                    qos=QOS_LEVEL,
                    retain=False
                )

                if result.rc == mqtt.MQTT_ERR_SUCCESS:
                    timestamp = time.strftime("%H:%M:%S")
                    print(f"[{timestamp}] Ciclo {ciclo}: {luce} lux pubblicato")
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
