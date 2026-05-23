/*
 * HW364A_Gateway.ino - Gateway Industriale Universale per ESP8266
 *
 * Hardware: HW-364A (ESP8266 NodeMCU + chip RS485 MAX3485 integrato)
 * Legge un PLC Siemens S7-300 via RS485 (Modbus RTU) ed espone i dati
 * su Wi-Fi tramite: Modbus TCP, MQTT, API JSON.
 *
 * Pin RS485: Serial0 (GPIO1=TX, GPIO3=RX)
 * Pin TX_ENABLE: GPIO2 (D4) o GPIO15 (D8) - configurabile
 *
 * Librerie richieste (da Library Manager Arduino):
 *   - PubSubClient  (Nick O'Leary)
 *   - ArduinoJson   (Benoit Blanchon) v6
 *   - ModbusMaster  (Doc Walker / 4-20ma)
 *
 * Istruzioni:
 *   1. Installa le librerie sopra elencate
 *   2. Seleziona scheda "NodeMCU 1.0 (ESP-12E)" o "Generic ESP8266"
 *   3. Carica lo sketch
 *   4. Collega RS485 al PLC (A+, B-, GND connessi)
 *   5. Al primo avvio si crea AP "HW364A_Config" per configurare il Wi-Fi
 *
 * OLED opzionale: SDA=GPIO14(D6), SCL=GPIO12(D5), installare libreria "U8g2"
 *   Per abilitare: decommentare #define USE_OLED qui sotto
 */

#define USE_OLED

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ModbusMaster.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "ConfigManager.h"
#include "WiFiManager.h"
#include "ModbusHandler.h"
#include "MQTTHandler.h"
#include "WebInterface.h"

#ifdef USE_OLED
#include "OLEDDisplay.h"
OLEDManager oledDisplay;
#endif

// Istanza globale della configurazione (persistente su LittleFS)
ConfigManager configManager;

// Gestione Wi-Fi con AP e captive portal
WiFiManagerClass wifiManager;

// Handler Modbus RTU (RS485) e Modbus TCP
ModbusHandler modbusHandler;

// Client MQTT per pubblicazione e sottoscrizione
MQTTHandler mqttHandler;

// Server Web, Dashboard, API
WebInterface webInterface;

/* ============================================================
 *  SETUP - Inizializzazione del sistema
 * ============================================================ */
void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== HW364A Gateway ===");

    // Monta LittleFS e carica la configurazione
    configManager.begin();

    // Collega il riferimento a ConfigManager nel WebInterface
    webInterface.configManagerRef = &configManager;

    // Avvia connessione Wi-Fi (o AP se non configurato)
    wifiManager.begin(&configManager.config);

    // Inizializza Modbus RTU su Serial0 (RS485) e TCP server
    modbusHandler.begin(&configManager.config);

    // Avvia web server su porta 80
    webInterface.begin(&configManager.config, &modbusHandler, &wifiManager);

    // Inizializza client MQTT se abilitato
    mqttHandler.begin(&configManager.config, &modbusHandler);

#ifdef USE_OLED
    oledDisplay.begin();
#endif

}

/* ============================================================
 *  LOOP - Ciclo principale non bloccante
 *  Ogni funzione handle() torna immediatamente (nessun delay)
 * ============================================================ */
void loop() {
    // Mantiene la connessione Wi-Fi o gestisce l'AP
    wifiManager.handle();

    // Gestisce richieste HTTP (Dashboard, Config, API)
    webInterface.handle();

    // Polling RS485 e server Modbus TCP
    modbusHandler.handle();

    // Pubblicazione MQTT e ascolto comandi
    mqttHandler.handle();

#ifdef USE_OLED
    oledDisplay.handle(&configManager.config, &modbusHandler, &wifiManager, mqttHandler.isConnected());
#endif

}
