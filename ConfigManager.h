/*
 * ConfigManager.h - Gestione della configurazione persistente su LittleFS
 * Salva e carica tutti i parametri del gateway in formato JSON
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <ArduinoJson.h>
#include <LittleFS.h>

#define CONFIG_FILE "/config.json"
#define NUM_REGS 10

struct GatewayConfig {
    // WiFi
    char wifiSSID[32] = "";
    char wifiPassword[64] = "";

    // Modbus RTU (RS485)
    uint32_t rtuBaudRate = 9600;
    uint8_t rtuSlaveId = 1;
    uint16_t rtuStartAddress = 0;
    uint16_t rtuPollInterval = 1000;
    uint8_t rtuTxEnablePin = 2;

    // Abilitazione protocolli
    bool modbusTcpEnabled = true;
    bool mqttEnabled = false;
    bool jsonApiEnabled = true;

    // Modbus TCP
    uint16_t tcpPort = 502;

    // MQTT
    char mqttBroker[64] = "";
    uint16_t mqttPort = 1883;
    char mqttUser[32] = "";
    char mqttPass[32] = "";
    char mqttPubTopic[64] = "hw364a/data";
    char mqttSubTopic[64] = "hw364a/cmd";
    uint16_t mqttPublishInterval = 5000;

    // Nomi e scaling registri
    char regNames[NUM_REGS][16] = {
        "Temperatura1", "Temperatura2", "Pressione1", "Pressione2",
        "Velocita", "Contatore", "Stato_Ingressi", "Stato_Uscite",
        "Setpoint1", "Setpoint2"
    };
    float regScaling[NUM_REGS] = { 0.1, 0.1, 0.01, 0.01, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
    uint16_t regAddress[NUM_REGS] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
};

class ConfigManager {
public:
    GatewayConfig config;

    bool begin() {
        if (!LittleFS.begin()) {
            LittleFS.format();
            if (!LittleFS.begin()) return false;
        }
        return load();
    }

    bool load() {
        if (!LittleFS.exists(CONFIG_FILE)) {
            return save();
        }

        File file = LittleFS.open(CONFIG_FILE, "r");
        if (!file) return false;

        StaticJsonDocument<1536> doc;
        DeserializationError err = deserializeJson(doc, file);
        file.close();
        if (err) return false;

        strlcpy(config.wifiSSID, doc["wifiSSID"] | "", sizeof(config.wifiSSID));
        strlcpy(config.wifiPassword, doc["wifiPassword"] | "", sizeof(config.wifiPassword));

        config.rtuBaudRate = doc["rtuBaudRate"] | config.rtuBaudRate;
        config.rtuSlaveId = doc["rtuSlaveId"] | config.rtuSlaveId;
        config.rtuStartAddress = doc["rtuStartAddress"] | config.rtuStartAddress;
        config.rtuPollInterval = doc["rtuPollInterval"] | config.rtuPollInterval;
        config.rtuTxEnablePin = doc["rtuTxEnablePin"] | config.rtuTxEnablePin;

        config.modbusTcpEnabled = doc["modbusTcpEnabled"] | config.modbusTcpEnabled;
        config.mqttEnabled = doc["mqttEnabled"] | config.mqttEnabled;
        config.jsonApiEnabled = doc["jsonApiEnabled"] | config.jsonApiEnabled;

        config.tcpPort = doc["tcpPort"] | config.tcpPort;

        strlcpy(config.mqttBroker, doc["mqttBroker"] | "", sizeof(config.mqttBroker));
        config.mqttPort = doc["mqttPort"] | config.mqttPort;
        strlcpy(config.mqttUser, doc["mqttUser"] | "", sizeof(config.mqttUser));
        strlcpy(config.mqttPass, doc["mqttPass"] | "", sizeof(config.mqttPass));
        strlcpy(config.mqttPubTopic, doc["mqttPubTopic"] | config.mqttPubTopic, sizeof(config.mqttPubTopic));
        strlcpy(config.mqttSubTopic, doc["mqttSubTopic"] | config.mqttSubTopic, sizeof(config.mqttSubTopic));
        config.mqttPublishInterval = doc["mqttPublishInterval"] | config.mqttPublishInterval;

        JsonArray names = doc["regNames"];
        if (names.size() > 0) {
            for (int i = 0; i < NUM_REGS && i < (int)names.size(); i++) {
                strlcpy(config.regNames[i], names[i] | config.regNames[i], sizeof(config.regNames[i]));
            }
        }

        JsonArray scaling = doc["regScaling"];
        if (scaling.size() > 0) {
            for (int i = 0; i < NUM_REGS && i < (int)scaling.size(); i++) {
                config.regScaling[i] = scaling[i] | config.regScaling[i];
            }
        }

        JsonArray addrs = doc["regAddress"];
        if (addrs.size() > 0) {
            for (int i = 0; i < NUM_REGS && i < (int)addrs.size(); i++) {
                config.regAddress[i] = addrs[i] | config.regAddress[i];
            }
        }

        return true;
    }

    bool save() {
        File file = LittleFS.open(CONFIG_FILE, "w");
        if (!file) return false;

        StaticJsonDocument<1536> doc;

        doc["wifiSSID"] = config.wifiSSID;
        doc["wifiPassword"] = config.wifiPassword;

        doc["rtuBaudRate"] = config.rtuBaudRate;
        doc["rtuSlaveId"] = config.rtuSlaveId;
        doc["rtuStartAddress"] = config.rtuStartAddress;
        doc["rtuPollInterval"] = config.rtuPollInterval;
        doc["rtuTxEnablePin"] = config.rtuTxEnablePin;

        doc["modbusTcpEnabled"] = config.modbusTcpEnabled;
        doc["mqttEnabled"] = config.mqttEnabled;
        doc["jsonApiEnabled"] = config.jsonApiEnabled;

        doc["tcpPort"] = config.tcpPort;

        doc["mqttBroker"] = config.mqttBroker;
        doc["mqttPort"] = config.mqttPort;
        doc["mqttUser"] = config.mqttUser;
        doc["mqttPass"] = config.mqttPass;
        doc["mqttPubTopic"] = config.mqttPubTopic;
        doc["mqttSubTopic"] = config.mqttSubTopic;
        doc["mqttPublishInterval"] = config.mqttPublishInterval;

        JsonArray names = doc.createNestedArray("regNames");
        JsonArray scaling = doc.createNestedArray("regScaling");
        JsonArray addrs = doc.createNestedArray("regAddress");
        for (int i = 0; i < NUM_REGS; i++) {
            names.add(config.regNames[i]);
            scaling.add(config.regScaling[i]);
            addrs.add(config.regAddress[i]);
        }

        serializeJson(doc, file);
        file.close();
        return true;
    }

    bool resetToDefaults() {
        LittleFS.remove(CONFIG_FILE);
        *this = ConfigManager();
        return save();
    }
};

#endif
