#!/usr/bin/env python3
"""
App Monitor - MQTT Subscriber con Wildcard
Si abbona al topic 'casa/#' (tutti gli eventi della casa) e logga i messaggi
in arrivo con timestamp sia su console che su file.

Requisiti: pip install paho-mqtt
"""

import paho.mqtt.client as mqtt
import time
import sys
from datetime import datetime

# ============================================================
# CONFIGURAZIONE BROKER
# ============================================================
BROKER_HOST = "localhost"
BROKER_PORT = 1883
BROKER_KEEPALIVE = 60

# Topic MQTT da monitorare (wildcard = tutti i topic sotto 'casa/')
TOPIC_WILDCARD = "casa/#"

# Parametri logging
FILE_LOG = "mqtt_events.log"  # File dove salvare gli eventi
QOS_LEVEL = 1
LOG_A_CONSOLE = True
LOG_A_FILE = True

# ============================================================
# VARIABILI DI STATO
# ============================================================
is_connected = False
contatore_messaggi = 0


# ============================================================
# FUNZIONI UTILITY
# ============================================================

def scrivi_log(messaggio):
    """
    Scrive un messaggio sia su console che su file di log

    Args:
        messaggio (str): Il messaggio da loggare
    """
    global contatore_messaggi

    contatore_messaggi += 1

    # Log a console
    if LOG_A_CONSOLE:
        print(messaggio)

    # Log a file
    if LOG_A_FILE:
        try:
            with open(FILE_LOG, 'a', encoding='utf-8') as f:
                f.write(messaggio + '\n')
                f.flush()
        except IOError as e:
            print(f"[✗] Errore scrittura su file: {e}")


def formatta_timestamp():
    """Restituisce timestamp formattato HH:MM:SS"""
    return datetime.now().strftime("%H:%M:%S")


# ============================================================
# CALLBACK MQTT
# ============================================================

def on_connect(client, userdata, flags, rc):
    """Callback: connessione stabilita o fallita"""
    global is_connected

    if rc == 0:
        msg = f"[✓] Connesso al broker MQTT su {BROKER_HOST}:{BROKER_PORT}"
        scrivi_log(msg)

        msg = f"[✓] Sottoscrizione a: {TOPIC_WILDCARD} (ascolta TUTTI gli eventi)"
        scrivi_log(msg)

        is_connected = True

        # Sottoscrizione al topic wildcard
        client.subscribe(TOPIC_WILDCARD, qos=QOS_LEVEL)

        # Stampa separatore
        scrivi_log("=" * 70)
        scrivi_log("MONITORAGGIO INIZIATO")
        scrivi_log("=" * 70)

    else:
        msg = f"[✗] Errore di connessione (Codice: {rc})"
        scrivi_log(msg)
        is_connected = False


def on_disconnect(client, userdata, rc):
    """Callback: disconnessione dal broker"""
    global is_connected

    if rc != 0:
        msg = f"[!] Disconnessione inaspettata (Codice: {rc})"
    else:
        msg = f"[!] Disconnessione volontaria"

    scrivi_log(msg)
    is_connected = False


def on_message(client, userdata, msg):
    """
    Callback: messaggio ricevuto dal broker
    Logga il messaggio con timestamp
    """
    try:
        # Estrai informazioni messaggio
        topic = msg.topic
        payload = msg.payload.decode("utf-8")
        qos = msg.qos
        retain = msg.retain

        # Formatta il log
        timestamp = formatta_timestamp()

        # Formato: [HH:MM:SS] Topic: {topic} → Valore: {payload}
        log_message = f"[{timestamp}] Topic: {topic:30} → Valore: {payload:20}"

        # Informazioni aggiuntive (opzionali, commentate di default)
        # log_message += f" | QoS: {qos}, Retain: {retain}"

        # Scrivi log
        scrivi_log(log_message)

    except Exception as e:
        msg = f"[✗] Errore nell'elaborazione messaggio: {e}"
        scrivi_log(msg)


def on_log(client, userdata, level, buf):
    """Callback: log MQTT (opzionale, per debug)"""
    if level == mqtt.MQTT_LOG_ERR:
        msg = f"[MQTT ERROR] {buf}"
        scrivi_log(msg)


# ============================================================
# MAIN - LOOP PRINCIPALE
# ============================================================

def main():
    """Loop principale del monitor MQTT"""

    global contatore_messaggi

    print("=" * 70)
    print("APP MONITOR - MQTT Subscriber con Wildcard")
    print("=" * 70)
    print(f"Broker: {BROKER_HOST}:{BROKER_PORT}")
    print(f"Topic: {TOPIC_WILDCARD}")
    print(f"Log file: {FILE_LOG}")
    print("Premere Ctrl+C per interrompere")
    print("=" * 70)
    print()

    # Inizializza file di log
    if LOG_A_FILE:
        try:
            # Cancella il file log precedente (opzionale)
            # with open(FILE_LOG, 'w') as f:
            #     f.write("")

            # Oppure aggiungi header di inizio sessione
            with open(FILE_LOG, 'a', encoding='utf-8') as f:
                f.write("\n")
                f.write("=" * 70 + "\n")
                f.write(f"INIZIO MONITORAGGIO: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                f.write("=" * 70 + "\n")
        except IOError as e:
            print(f"[!] Avviso: impossibile creare file log: {e}")

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
        print("[...] In ascolto per messaggi MQTT...\n")
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

        # Resoconto finale
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        msg = f"\n[ℹ] FINE MONITORAGGIO: {timestamp}"
        msg += f"\n[ℹ] Totale messaggi ricevuti: {contatore_messaggi}"
        scrivi_log(msg)

        print("[✓] Disconnesso. Arrivederci!")


# ============================================================
# ENTRY POINT
# ============================================================

if __name__ == "__main__":
    main()
