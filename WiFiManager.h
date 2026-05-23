/*
 * WiFiManager.h - Gestione connessione Wi-Fi e modalita Access Point
 * Implementa il captive portal per la configurazione iniziale
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include "ConfigManager.h"

#define WIFI_TIMEOUT_MS 20000
#define AP_SSID "HW364A_Config"

class WiFiManagerClass {
public:
    GatewayConfig* config = nullptr;

    void begin(GatewayConfig* cfg) {
        config = cfg;
        WiFi.mode(WIFI_STA);
        WiFi.hostname("HW364A-Gateway");
        WiFi.persistent(true);

        if (strlen(config->wifiSSID) == 0) {
            avviaAP();
            return;
        }

        connettiWiFi();
    }

    void handle() {
        if (modalitaAP) {
            dnsServer.processNextRequest();
        } else if (WiFi.status() == WL_CONNECTED) {
            inConnessione = false;
            if (!mDNSStarted) {
                mDNSStarted = true;
                Serial.printf("[WiFi] Connesso! IP: %s\n", WiFi.localIP().toString().c_str());
                if (MDNS.begin("hw364a")) {
                    MDNS.addService("http", "tcp", 80);
                    Serial.println("[mDNS] http://hw364a.local");
                }
            }
            MDNS.update();
        } else if (WiFi.getMode() & WIFI_STA) {
            if (inConnessione && millis() - inizioConnessione > WIFI_TIMEOUT_MS) {
                Serial.println("[WiFi] Timeout connessione, avvio AP");
                avviaAP();
            } else if (!inConnessione) {
                Serial.println("[WiFi] Connessione persa, riconnessione...");
                connettiWiFi();
                mDNSStarted = false;
            }
        }
    }

    void disconnectAP() {
        if (modalitaAP) {
            modalitaAP = false;
            dnsServer.stop();
            WiFi.softAPdisconnect(true);
        }
    }

    bool isAPMode() {
        return modalitaAP;
    }

    String getStato() {
        if (modalitaAP) return "AP: " + String(AP_SSID);
        if (WiFi.status() == WL_CONNECTED) return "Connesso: " + WiFi.SSID();
        return "Disconnesso";
    }

    String getIP() {
        if (modalitaAP) return WiFi.softAPIP().toString();
        return WiFi.localIP().toString();
    }

    void connetti(const char* ssid, const char* password) {
        strlcpy(config->wifiSSID, ssid, sizeof(config->wifiSSID));
        strlcpy(config->wifiPassword, password, sizeof(config->wifiPassword));

        disconnectAP();
        WiFi.mode(WIFI_STA);
        connettiWiFi();
    }

private:
    DNSServer dnsServer;
    bool modalitaAP = false;
    bool inConnessione = false;
    bool mDNSStarted = false;
    uint32_t inizioConnessione = 0;

    void connettiWiFi() {
        inConnessione = true;
        inizioConnessione = millis();
        WiFi.begin(config->wifiSSID, config->wifiPassword);
    }

    void avviaAP() {
        modalitaAP = true;
        inConnessione = false;
        mDNSStarted = false;

        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(IPAddress(192, 168, 4, 1),
            IPAddress(192, 168, 4, 1),
            IPAddress(255, 255, 255, 0));
        WiFi.softAP(AP_SSID);

        dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
    }
};

#endif
