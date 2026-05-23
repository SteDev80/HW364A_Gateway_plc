/*
 * MQTTHandler.h - Client MQTT per pubblicazione dati e scrittura comandi
 * Pubblica i registri del PLC su broker MQTT e riceve comandi di scrittura
 */

#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include "ConfigManager.h"
#include "ModbusHandler.h"

class MQTTHandler {
public:
    void begin(GatewayConfig* cfg, ModbusHandler* mh) {
        config = cfg;
        modbus = mh;
        mqttClient.setClient(wifiClient);
        if (config->mqttEnabled && strlen(config->mqttBroker) > 0) {
            mqttClient.setServer(config->mqttBroker, config->mqttPort);
            mqttClient.setCallback([this](char* t, byte* p, unsigned int l) {
                callbackMQTT(t, p, l);
            });
        }
    }

    void handle() {
        if (!config->mqttEnabled) return;
        if (WiFi.status() != WL_CONNECTED) return;

        if (!mqttClient.connected()) {
            connettiBroker();
            return;
        }

        mqttClient.loop();

        if (millis() - ultimaPubblicazione >= config->mqttPublishInterval) {
            pubblicaDati();
            ultimaPubblicazione = millis();
        }
    }

    String getStato() {
        if (!config->mqttEnabled) return "Disabilitato";
        if (!mqttClient.connected()) return "Disconnesso";
        return "Connesso a " + String(config->mqttBroker);
    }

    bool isConnected() {
        return config->mqttEnabled && mqttClient.connected();
    }

private:
    GatewayConfig* config = nullptr;
    ModbusHandler* modbus = nullptr;
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    uint32_t ultimaPubblicazione = 0;
    uint32_t ultimoTentativo = 0;
    static const uint32_t INTERVALLO_RICONNESSIONE = 10000;

    void connettiBroker() {
        if (millis() - ultimoTentativo < INTERVALLO_RICONNESSIONE) return;
        ultimoTentativo = millis();

        String clientId = "HW364A_" + String(ESP.getChipId(), HEX);
        bool connesso = false;

        if (strlen(config->mqttUser) > 0) {
            connesso = mqttClient.connect(clientId.c_str(),
                config->mqttUser, config->mqttPass,
                config->mqttPubTopic, 1, true, "{\"stato\":\"offline\"}");
        } else {
            connesso = mqttClient.connect(clientId.c_str(),
                config->mqttPubTopic, 1, true, "{\"stato\":\"offline\"}");
        }

        if (connesso) {
            mqttClient.subscribe(config->mqttSubTopic);
            StaticJsonDocument<128> doc;
            doc["stato"] = "online";
            doc["ip"] = WiFi.localIP().toString();
            String msg;
            serializeJson(doc, msg);
            mqttClient.publish(config->mqttPubTopic, msg.c_str(), true);
        }
    }

    void pubblicaDati() {
        if (!mqttClient.connected()) return;

        StaticJsonDocument<512> doc;
        JsonArray regs = doc.createNestedArray("registri");
        JsonArray scalati = doc.createNestedArray("valori");
        JsonArray nomi = doc.createNestedArray("nomi");

        for (int i = 0; i < NUM_REGS; i++) {
            regs.add(modbus->plcData.registers[i]);
            scalati.add(modbus->plcData.registers[i] * config->regScaling[i]);
            nomi.add(config->regNames[i]);
        }

        doc["timestamp"] = millis();
        doc["valid"] = modbus->plcData.valid;
        doc["ip"] = WiFi.localIP().toString();

        String json;
        serializeJson(doc, json);
        mqttClient.publish(config->mqttPubTopic, json.c_str());
    }

    void callbackMQTT(char* topic, byte* payload, unsigned int length) {
        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(doc, payload, length);
        if (err) return;

        uint16_t indirizzo = doc["address"] | doc["indirizzo"] | 0xFFFF;
        uint16_t valore = doc["value"] | doc["valore"] | 0;

        if (indirizzo != 0xFFFF && indirizzo < NUM_REGS) {
            modbus->requestWrite(indirizzo, valore);
        }

        const char* comando = doc["comando"] | "";
        if (strcmp(comando, "resetta") == 0) {
            for (int i = 0; i < NUM_REGS; i++) {
                modbus->requestWrite(i, 0);
            }
        }
    }
};

#endif
