/*
 * WebInterface.h - Server Web, Dashboard, Configurazione e API JSON
 * Gestisce tutte le interfacce utente via HTTP
 */

#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <ESP8266WebServer.h>
#include "ConfigManager.h"
#include "ModbusHandler.h"
#include "WiFiManager.h"

class WebInterface {
public:
    void begin(GatewayConfig* cfg, ModbusHandler* mh, WiFiManagerClass* wm) {
        config = cfg;
        modbus = mh;
        wifiMgr = wm;

        server.on("/", [this]() { handleDashboard(); });
        server.on("/config", HTTP_GET, [this]() { handleConfigPage(); });
        server.on("/config", HTTP_POST, [this]() { handleConfigSave(); });
        server.on("/api/data", [this]() { handleApiData(); });
        server.on("/api/config", HTTP_GET, [this]() { handleApiGetConfig(); });
        server.on("/api/config", HTTP_POST, [this]() { handleApiSetConfig(); });
        server.on("/api/write", HTTP_POST, [this]() { handleApiWrite(); });
        server.on("/api/reset", HTTP_POST, [this]() { handleApiReset(); });
        server.on("/api/reboot", HTTP_POST, [this]() {
            server.send(200, "text/plain", "Riavvio...");
            delay(100);
            ESP.restart();
        });
        server.on("/wifi", [this]() { handleWifiConfig(); });
        server.onNotFound([this]() { handleNotFound(); });

        server.begin();
    }

    void handle() {
        server.handleClient();
    }

private:
    GatewayConfig* config = nullptr;
    ModbusHandler* modbus = nullptr;
    WiFiManagerClass* wifiMgr = nullptr;
    ESP8266WebServer server;

    void inviaHTML(const String& html) {
        server.send(200, "text/html; charset=utf-8", html);
    }

    void inviaJSON(int code, const String& json) {
        server.send(code, "application/json", json);
    }

    // --- DASHBOARD ---
    void handleDashboard() {
        if (wifiMgr->isAPMode()) {
            server.sendHeader("Location", "/wifi", true);
            server.send(302, "text/plain", "Redirect");
            return;
        }
        String h =
            "<!DOCTYPE html><html><head><meta charset=UTF-8>"
            "<meta name=viewport content='width=device-width,initial-scale=1'>"
            "<title>Dashboard - HW364A</title>"
            "<style>"
            "body{font-family:sans-serif;background:#eef1f5;color:#333;padding:16px;margin:0}"
            "h2{color:#1a237e}.card{background:#fff;border-radius:8px;padding:16px;margin:12px 0;box-shadow:0 2px 6px #00000014}"
            "table{width:100%;border-collapse:collapse;font-size:.9em}"
            "th{background:#e8eaf6;padding:8px;text-align:left;font-weight:600}"
            "td{padding:6px;border-bottom:1px solid #e0e0e0;text-align:right}"
            "td:first-child{text-align:center}td:nth-child(2){text-align:left}"
            ".ok{color:#2e7d32}.err{color:#c62828}.warn{color:#f57f17}"
            "footer{text-align:center;margin-top:24px;font-size:.8em;color:#999}"
            ".nav{background:#1a237e;border-radius:8px;padding:8px 0;margin-bottom:16px}"
            ".nav a{color:#fff;padding:8px 16px;text-decoration:none;display:inline-block}"
            "</style></head><body>"
            "<div class=nav><a href='/' class=on>Dashboard</a><a href='/config'>Configurazione</a><a href='/api/data' target=_blank>API JSON</a></div>"
            "<div class=card>"
            "<h2>Gateway HW364A</h2>"
            "<p><b>IP:</b> " + wifiMgr->getIP() + " &nbsp; <b>WiFi:</b> " + wifiMgr->getStato()
            + "<br><b>MQTT:</b> " + String(config->mqttEnabled ? "abilitato" : "disabilitato")
            + " &nbsp; <b>ModbusTCP:</b> " + String(config->modbusTcpEnabled ? "porta " + String(config->tcpPort) : "disabilitato")
            + " &nbsp; <b id=agg>aggiornamento...</b></p>"
            "</div>"
            "<div class=card><h2>Registri PLC"
            "<span style='float:right;font-size:.7em;font-weight:normal' id=plcStat>attendere...</span></h2>"
            "<table><tr><th>#</th><th>Nome</th><th>Raw</th><th>Scalato</th></tr>";
        for (int i = 0; i < NUM_REGS; i++) {
            h += "<tr><td>" + String(i) + "</td><td>" + String(config->regNames[i]) + "</td>"
                "<td id=r" + String(i) + ">--</td><td id=s" + String(i) + ">--</td></tr>";
        }
        h += "</table></div>"
            "<footer>HW364A Gateway</footer>"
            "<script>"
            "var XHR=new XMLHttpRequest();"
            "function aggiorna(){"
            "XHR.onreadystatechange=function(){if(XHR.readyState==4&&XHR.status==200){"
            "var d=JSON.parse(XHR.responseText);"
            "document.getElementById('agg').textContent=new Date(d.ts).toLocaleTimeString();"
            "document.getElementById('plcStat').textContent=d.valid?'OK':'NO DATI';"
            "document.getElementById('plcStat').className=d.valid?'ok':'err';"
            "for(var i=0;i<d.regs.length;i++){"
            "document.getElementById('r'+i).textContent=d.regs[i];"
            "document.getElementById('s'+i).textContent=d.scaled[i].toFixed(1);"
            "}"
            "}};"
            "XHR.open('GET','/api/data',true);"
            "XHR.send();"
            "}"
            "setInterval(aggiorna,3000);"
            "aggiorna();"
            "</script>"
            "</body></html>";
        server.send(200, "text/html", h);
    }

    // --- PAGINA CONFIGURAZIONE ---
    void handleConfigPage() {
        String h =
            "<!DOCTYPE html><html><head><meta charset=UTF-8>"
            "<meta name=viewport content='width=device-width,initial-scale=1'>"
            "<title>Configurazione - HW364A</title>"
            "<style>"
            "body{font-family:sans-serif;background:#eef1f5;color:#333;padding:16px;margin:0}"
            "h2{color:#1a237e}.card{background:#fff;border-radius:8px;padding:16px;margin:12px 0;box-shadow:0 2px 6px #00000014}"
            "table{width:100%;border-collapse:collapse;font-size:.9em}"
            "th{background:#e8eaf6;padding:8px;text-align:left}"
            "td{padding:6px;border-bottom:1px solid #e0e0e0}"
            "label{display:block;margin:8px 0 4px;font-weight:600;font-size:.9em}"
            "input,select{width:100%;padding:8px;border:1px solid #ccc;border-radius:4px;font-size:.95em;box-sizing:border-box}"
            ".btn{background:#1a237e;color:#fff;border:none;padding:10px 24px;border-radius:4px;cursor:pointer;font-size:.95em;margin:12px 4px 0 0}"
            "footer{text-align:center;margin-top:24px;font-size:.8em;color:#999}"
            ".nav{background:#1a237e;border-radius:8px;padding:8px 0;margin-bottom:16px}"
            ".nav a{color:#fff;padding:8px 16px;text-decoration:none;display:inline-block}"
            ".due-col{display:flex;gap:12px;flex-wrap:wrap}.due-col>*{flex:1;min-width:200px}"
            "</style></head><body>"
            "<div class=nav><a href='/'>Dashboard</a><a href='/config' class=on>Configurazione</a><a href='/api/data' target=_blank>API JSON</a></div>"
            "<div class=card><h2>Configurazione Gateway</h2>"
            "<p>Modifica i parametri e clicca Salva. Il dispositivo si riavvia.</p></div>"
            "<form method=POST action=/config>"
            "<div class=card><h2>Wi-Fi</h2>"
            "<label>SSID</label><input type=text name=wifi_ssid value='" + escHtml(config->wifiSSID) + "'>"
            "<label>Password</label><input type=password name=wifi_password value='" + escHtml(config->wifiPassword) + "'>"
            "</div>"
            "<div class=card><h2>Modbus RTU (RS485)</h2><div class=due-col>"
            "<div><label>Baud Rate</label><select name=rtu_baud>"
            "<option value=1200" + String(config->rtuBaudRate == 1200 ? " selected" : "") + ">1200</option>"
            "<option value=2400" + String(config->rtuBaudRate == 2400 ? " selected" : "") + ">2400</option>"
            "<option value=4800" + String(config->rtuBaudRate == 4800 ? " selected" : "") + ">4800</option>"
            "<option value=9600" + String(config->rtuBaudRate == 9600 ? " selected" : "") + ">9600</option>"
            "<option value=19200" + String(config->rtuBaudRate == 19200 ? " selected" : "") + ">19200</option>"
            "<option value=38400" + String(config->rtuBaudRate == 38400 ? " selected" : "") + ">38400</option>"
            "<option value=57600" + String(config->rtuBaudRate == 57600 ? " selected" : "") + ">57600</option>"
            "<option value=115200" + String(config->rtuBaudRate == 115200 ? " selected" : "") + ">115200</option>"
            "</select></div>"
            "<div><label>Slave ID PLC</label><input type=number name=rtu_slave_id min=1 max=247 value='" + String(config->rtuSlaveId) + "'></div>"
            "<div><label>Indirizzo Iniziale</label><input type=number name=rtu_addr_ini min=0 max=65535 value='" + String(config->rtuStartAddress) + "'></div>"
            "<div><label>Polling (ms)</label><input type=number name=rtu_poll min=100 max=60000 value='" + String(config->rtuPollInterval) + "'></div>"
            "<div><label>Pin TX Enable</label><select name=rtu_tx_pin>"
            "<option value=2" + String(config->rtuTxEnablePin == 2 ? " selected" : "") + ">GPIO2 (D4)</option>"
            "<option value=15" + String(config->rtuTxEnablePin == 15 ? " selected" : "") + ">GPIO15 (D8)</option>"
            "</select></div></div></div>"
            "<div class=card><h2>Abilitazione Protocolli Wi-Fi</h2>"
            "<div class=gruppo-cb><label class=cb><input type=checkbox class=cb name=modbus_tcp value=1" + String(config->modbusTcpEnabled ? " checked" : "") + "> Modbus TCP (porta " + String(config->tcpPort) + ")</label></div>"
            "<div class=gruppo-cb><label class=cb><input type=checkbox class=cb name=mqtt_enabled value=1" + String(config->mqttEnabled ? " checked" : "") + "> MQTT Client</label></div>"
            "<div class=gruppo-cb><label class=cb><input type=checkbox class=cb name=json_api value=1" + String(config->jsonApiEnabled ? " checked" : "") + "> API JSON</label></div>"
            "<label>Porta Modbus TCP</label><input type=number name=tcp_port min=1 max=65535 value='" + String(config->tcpPort) + "'>"
            "</div>"
            "<div class=card><h2>MQTT</h2><div class=due-col>"
            "<div><label>Broker</label><input type=text name=mqtt_broker value='" + escHtml(config->mqttBroker) + "'></div>"
            "<div><label>Porta</label><input type=number name=mqtt_port min=1 max=65535 value='" + String(config->mqttPort) + "'></div>"
            "<div><label>Username</label><input type=text name=mqtt_user value='" + escHtml(config->mqttUser) + "'></div>"
            "<div><label>Password</label><input type=password name=mqtt_pass value='" + escHtml(config->mqttPass) + "'></div>"
            "<div><label>Topic Pubblicazione</label><input type=text name=mqtt_pub value='" + escHtml(config->mqttPubTopic) + "'></div>"
            "<div><label>Topic Sottoscrizione</label><input type=text name=mqtt_sub value='" + escHtml(config->mqttSubTopic) + "'></div>"
            "<div><label>Intervallo pubbl. (ms)</label><input type=number name=mqtt_int min=1000 max=60000 value='" + String(config->mqttPublishInterval) + "'></div>"
            "</div></div>"
            "<div class=card><h2>Configurazione Registri</h2>"
            "<table><tr><th>#</th><th>Indirizzo Modbus</th><th>Nome</th><th>Scala</th></tr>";
        for (int i = 0; i < NUM_REGS; i++) {
            h += "<tr><td>" + String(i) + "</td>"
                "<td><input type=number name=addr_" + String(i) + " min=0 max=65535 value='" + String(config->regAddress[i]) + "' style='width:80px'></td>"
                "<td><input type=text name=name_" + String(i) + " value='" + escHtml(config->regNames[i]) + "' style='width:140px'></td>"
                "<td><input type=number step=any name=scale_" + String(i) + " value='" + String(config->regScaling[i], 4) + "' style='width:100px'></td></tr>";
        }
        h += "</table></div>"
            "<button type=submit class=btn>Salva e Riavvia</button> "
            "<a href='/' class=btn style='background:#666'>Annulla</a>"
            "</form>"
            "<footer>HW364A Gateway</footer>"
            "</body></html>";
        server.send(200, "text/html", h);
    }

    // --- SALVATAGGIO CONFIG ---
    void handleConfigSave() {
        if (server.hasArg("wifi_ssid"))
            strlcpy(config->wifiSSID, server.arg("wifi_ssid").c_str(), sizeof(config->wifiSSID));
        if (server.hasArg("wifi_password"))
            strlcpy(config->wifiPassword, server.arg("wifi_password").c_str(), sizeof(config->wifiPassword));

        if (server.hasArg("rtu_baud"))
            config->rtuBaudRate = server.arg("rtu_baud").toInt();
        if (server.hasArg("rtu_slave_id"))
            config->rtuSlaveId = server.arg("rtu_slave_id").toInt();
        if (server.hasArg("rtu_addr_ini"))
            config->rtuStartAddress = server.arg("rtu_addr_ini").toInt();
        if (server.hasArg("rtu_poll"))
            config->rtuPollInterval = server.arg("rtu_poll").toInt();
        if (server.hasArg("rtu_tx_pin"))
            config->rtuTxEnablePin = server.arg("rtu_tx_pin").toInt();

        config->modbusTcpEnabled = server.hasArg("modbus_tcp");
        config->mqttEnabled = server.hasArg("mqtt_enabled");
        config->jsonApiEnabled = server.hasArg("json_api");

        if (server.hasArg("tcp_port"))
            config->tcpPort = server.arg("tcp_port").toInt();

        if (server.hasArg("mqtt_broker"))
            strlcpy(config->mqttBroker, server.arg("mqtt_broker").c_str(), sizeof(config->mqttBroker));
        if (server.hasArg("mqtt_port"))
            config->mqttPort = server.arg("mqtt_port").toInt();
        if (server.hasArg("mqtt_user"))
            strlcpy(config->mqttUser, server.arg("mqtt_user").c_str(), sizeof(config->mqttUser));
        if (server.hasArg("mqtt_pass"))
            strlcpy(config->mqttPass, server.arg("mqtt_pass").c_str(), sizeof(config->mqttPass));
        if (server.hasArg("mqtt_pub"))
            strlcpy(config->mqttPubTopic, server.arg("mqtt_pub").c_str(), sizeof(config->mqttPubTopic));
        if (server.hasArg("mqtt_sub"))
            strlcpy(config->mqttSubTopic, server.arg("mqtt_sub").c_str(), sizeof(config->mqttSubTopic));
        if (server.hasArg("mqtt_int"))
            config->mqttPublishInterval = server.arg("mqtt_int").toInt();

        for (int i = 0; i < NUM_REGS; i++) {
            if (server.hasArg("name_" + String(i)))
                strlcpy(config->regNames[i], server.arg("name_" + String(i)).c_str(), sizeof(config->regNames[i]));
            if (server.hasArg("scale_" + String(i)))
                config->regScaling[i] = server.arg("scale_" + String(i)).toFloat();
            if (server.hasArg("addr_" + String(i)))
                config->regAddress[i] = server.arg("addr_" + String(i)).toInt();
        }

        configManagerRef->save();

        String h =
            "<!DOCTYPE html><html><head><meta charset=UTF-8>"
            "<meta name=viewport content='width=device-width,initial-scale=1'>"
            "<title>Riavvio - HW364A</title></head><body>"
            "<h2>Configurazione Salvata</h2>"
            "<p>Il dispositivo si sta riavviando con le nuove impostazioni...</p>"
            "<p>Attendi circa 10 secondi, poi riconnettiti alla rete.</p>"
            "<p><span id=c>5</span> secondi...</p>"
            "<script>var c=5;setInterval(function(){c--;document.getElementById('c').textContent=c;"
            "if(c==0)location='/';},1000);</script>"
            "</body></html>";
        server.send(200, "text/html", h);
        server.client().stop();
        delay(100);
        ESP.restart();
    }

    // --- WIFI CONFIG (CAPTIVE PORTAL) ---
    void handleWifiConfig() {
        if (server.method() == HTTP_POST && server.hasArg("ssid")) {
            String ssid = server.arg("ssid");
            String pass = server.arg("password");
            strlcpy(config->wifiSSID, ssid.c_str(), sizeof(config->wifiSSID));
            strlcpy(config->wifiPassword, pass.c_str(), sizeof(config->wifiPassword));
            configManagerRef->save();
            String html = "<html><head><meta charset=UTF-8><meta http-equiv=refresh content='15;url=/'>"
                "<title>Riavvio...</title></head><body style='font-family:sans-serif;padding:20px;text-align:center;margin-top:60px'>"
                "<h1>Riavvio in corso...</h1>"
                "<p>Credenziali salvate. Il gateway si riavvia e tenta la connessione a <b>"
                + escHtml(ssid) + "</b>.</p></body></html>";
            inviaHTML(html);
            server.client().stop();
            delay(100);
            ESP.restart();
            return;
        }

        String html = "<html><head><meta charset=UTF-8>"
            "<meta name=viewport content='width=device-width,initial-scale=1'>"
            "<title>Configura Wi-Fi - HW364A</title>"
            "<style>body{font-family:sans-serif;padding:20px;max-width:400px;margin:40px auto}"
            "h1{color:#1a237e}label{display:block;margin:10px 0 4px;font-weight:bold}"
            "input{width:100%;padding:8px;border:1px solid #ccc;border-radius:4px;font-size:16px;box-sizing:border-box}"
            "button{width:100%;padding:10px;background:#1a237e;color:#fff;border:none;border-radius:4px;font-size:16px;margin-top:12px;cursor:pointer}"
            "</style></head><body>"
            "<h1>Configurazione Wi-Fi</h1>"
            "<p>Il gateway non &egrave; connesso. Inserisci le credenziali della tua rete.</p>"
            "<form method=POST action=/wifi>"
            "<label>SSID</label><input type=text name=ssid placeholder='Nome rete' required>"
            "<label>Password</label><input type=password name=password placeholder='Password'>"
            "<button type=submit>Connetti</button>"
            "</form></body></html>";
        inviaHTML(html);
    }

    // --- /api/data ---
    void handleApiData() {
        if (!config->jsonApiEnabled) {
            server.send(403, "application/json", "{\"errore\":\"API disabilitata\"}");
            return;
        }

        StaticJsonDocument<1024> doc;
        JsonArray regs = doc.createNestedArray("regs");
        JsonArray scalati = doc.createNestedArray("scaled");
        JsonArray nomi = doc.createNestedArray("names");
        JsonArray indirizzi = doc.createNestedArray("addrs");

        for (int i = 0; i < NUM_REGS; i++) {
            regs.add(modbus->plcData.registers[i]);
            scalati.add(modbus->plcData.registers[i] * config->regScaling[i]);
            nomi.add(config->regNames[i]);
            indirizzi.add(config->regAddress[i]);
        }

        doc["ts"] = millis();
        doc["valid"] = modbus->plcData.valid;
        doc["wifi"] = wifiMgr->getStato();
        doc["ip"] = wifiMgr->getIP();

        String json;
        serializeJson(doc, json);
        server.send(200, "application/json; charset=utf-8", json);
    }

    // --- GET /api/config ---
    void handleApiGetConfig() {
        StaticJsonDocument<1536> doc;

        doc["wifiSSID"] = config->wifiSSID;
        doc["rtuBaudRate"] = config->rtuBaudRate;
        doc["rtuSlaveId"] = config->rtuSlaveId;
        doc["rtuPollInterval"] = config->rtuPollInterval;
        doc["rtuTxEnablePin"] = (int)config->rtuTxEnablePin;
        doc["modbusTcpEnabled"] = config->modbusTcpEnabled;
        doc["mqttEnabled"] = config->mqttEnabled;
        doc["jsonApiEnabled"] = config->jsonApiEnabled;
        doc["tcpPort"] = config->tcpPort;
        doc["mqttBroker"] = config->mqttBroker;
        doc["mqttPort"] = config->mqttPort;
        doc["mqttPubTopic"] = config->mqttPubTopic;
        doc["mqttSubTopic"] = config->mqttSubTopic;
        doc["mqttPublishInterval"] = config->mqttPublishInterval;

        String json;
        serializeJsonPretty(doc, json);
        server.send(200, "application/json; charset=utf-8", json);
    }

    // --- POST /api/config ---
    void handleApiSetConfig() {
        StaticJsonDocument<1536> doc;
        DeserializationError err = deserializeJson(doc, server.arg("plain"));
        if (err) {
            server.send(400, "application/json", "{\"errore\":\"JSON non valido\"}");
            return;
        }

        if (doc.containsKey("wifiSSID"))
            strlcpy(config->wifiSSID, doc["wifiSSID"] | "", sizeof(config->wifiSSID));
        if (doc.containsKey("wifiPassword"))
            strlcpy(config->wifiPassword, doc["wifiPassword"] | "", sizeof(config->wifiPassword));
        if (doc.containsKey("rtuBaudRate"))
            config->rtuBaudRate = doc["rtuBaudRate"];
        if (doc.containsKey("rtuSlaveId"))
            config->rtuSlaveId = doc["rtuSlaveId"];
        if (doc.containsKey("rtuPollInterval"))
            config->rtuPollInterval = doc["rtuPollInterval"];
        if (doc.containsKey("modbusTcpEnabled"))
            config->modbusTcpEnabled = doc["modbusTcpEnabled"];
        if (doc.containsKey("mqttEnabled"))
            config->mqttEnabled = doc["mqttEnabled"];
        if (doc.containsKey("mqttBroker"))
            strlcpy(config->mqttBroker, doc["mqttBroker"] | "", sizeof(config->mqttBroker));
        if (doc.containsKey("mqttPort"))
            config->mqttPort = doc["mqttPort"];
        if (doc.containsKey("mqttPubTopic"))
            strlcpy(config->mqttPubTopic, doc["mqttPubTopic"] | config->mqttPubTopic, sizeof(config->mqttPubTopic));

        configManagerRef->save();
        server.send(200, "application/json", "{\"status\":\"OK\",\"messaggio\":\"Configurazione salvata. Riavvia per applicare.\"}");
    }

    // --- POST /api/write ---
    void handleApiWrite() {
        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(doc, server.arg("plain"));
        if (err) {
            server.send(400, "application/json", "{\"errore\":\"JSON non valido\"}");
            return;
        }

        uint16_t addr = doc["address"] | doc["indirizzo"] | 0xFFFF;
        uint16_t value = doc["value"] | doc["valore"] | 0;

        if (addr >= NUM_REGS) {
            server.send(400, "application/json", "{\"errore\":\"Indirizzo non valido\"}");
            return;
        }

        modbus->requestWrite(addr, value);

        StaticJsonDocument<128> resp;
        resp["status"] = "OK";
        resp["address"] = addr;
        resp["value"] = value;
        resp["messaggio"] = "Scrittura accodata";

        String json;
        serializeJson(resp, json);
        server.send(200, "application/json", json);
    }

    // --- POST /api/reset ---
    void handleApiReset() {
        configManagerRef->resetToDefaults();
        server.send(200, "application/json", "{\"status\":\"OK\",\"messaggio\":\"Reset eseguito. Riavvio...\"}");
        delay(100);
        ESP.restart();
    }

    // --- CAPTIVE PORTAL / NOT FOUND ---
    void handleNotFound() {
        if (wifiMgr->isAPMode()) {
            server.sendHeader("Location", "http://192.168.4.1/wifi", true);
            server.send(302, "text/plain", "Redirect");
        } else {
            server.sendHeader("Location", "/", true);
            server.send(302, "text/plain", "Redirect");
        }
    }

    String escHtml(const String& s) {
        String r = s;
        r.replace("&", "&amp;");
        r.replace("\"", "&quot;");
        r.replace("'", "&apos;");
        r.replace("<", "&lt;");
        r.replace(">", "&gt;");
        return r;
    }

public:
    ConfigManager* configManagerRef = nullptr;
};

#endif
