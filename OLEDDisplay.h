/*
 * OLEDDisplay.h - Display OLED SSD1306 128x64 I2C con libreria U8g2
 * Mostra stato gateway e valori registri in 4 pagine cicliche
 *
 * Libreria richiesta: "U8g2" di oliver da Library Manager Arduino
 *
 * Collegamento: SDA=GPIO14(D6), SCL=GPIO12(D5), VCC=3.3V, GND=GND
 * Indirizzo I2C default: 0x3C
 */

#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <Wire.h>
#include <U8g2lib.h>
#include "ConfigManager.h"
#include "ModbusHandler.h"
#include "WiFiManager.h"

#define PIN_SDA 14   // D6
#define PIN_SCL 12   // D5
#define INTERVALLO_SCREEN 4000

// Font: 6x10 pixel -> 21 colonne x 6 righe
#define FW 6
#define FH 10
#define COL(c) ((c) * FW)
#define ROW(r) (((r) + 1) * FH)

static U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0);

class OLEDManager {
public:
    void begin() {
        Wire.begin(PIN_SDA, PIN_SCL);
        u8g2.begin();
        inizializzato = true;
        ultimoCambio = millis();
        schermataCorrente = 0;
        splash();
    }

    void handle(GatewayConfig* cfg, ModbusHandler* mdb, WiFiManagerClass* wifi, bool mqttOk) {
        if (!inizializzato) return;

        if (millis() - ultimoCambio >= INTERVALLO_SCREEN) {
            schermataCorrente = (schermataCorrente + 1) % NUM_SCREEN;
            ultimoCambio = millis();
        }

        u8g2.firstPage();
        do {
            switch (schermataCorrente) {
                case 0: pStato(cfg, mdb, wifi, mqttOk); break;
                case 1: pRegistri(mdb, cfg, 0);         break;
                case 2: pRegistri(mdb, cfg, 5);         break;
                case 3: pExtra(mdb, wifi);               break;
            }
        } while (u8g2.nextPage());
    }

private:
    bool inizializzato = false;
    uint32_t ultimoCambio = 0;
    int schermataCorrente = 0;
    static const int NUM_SCREEN = 4;

    void stampa(uint8_t row, const char* s) {
        u8g2.setCursor(0, ROW(row));
        u8g2.print(s);
    }

    void stampaCol(uint8_t row, uint8_t col, const char* s) {
        u8g2.setCursor(COL(col), ROW(row));
        u8g2.print(s);
    }

    void stampaF(uint8_t row, const char* fmt, ...) {
        char buf[22];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        buf[21] = 0;
        stampa(row, buf);
    }

    void stampaStr(uint8_t row, const String& s) {
        stampa(row, s.c_str());
    }

    void splash() {
        u8g2.firstPage();
        do {
            u8g2.setFont(u8g2_font_6x10_tf);
            u8g2.setCursor(COL(5), ROW(2));
            u8g2.print("HW364A");
            u8g2.setCursor(COL(4), ROW(3));
            u8g2.print("GATEWAY");
            u8g2.setCursor(COL(1), ROW(5));
            u8g2.print("Avvio in corso...");
        } while (u8g2.nextPage());
    }

    void pStato(GatewayConfig* cfg, ModbusHandler* mdb, WiFiManagerClass* wifi, bool mqttOk) {
        u8g2.setFont(u8g2_font_6x10_tf);
        stampa(0, "=== STATO ===");

        String s = wifi->getStato();
        stampaStr(1, String("W:") + s.substring(0, 15));

        String ip = wifi->getIP();
        stampaStr(2, String("IP:") + ip);

        stampaF(3, "MQTT:%s M.TCP:%s",
            mqttOk ? "OK" : cfg->mqttEnabled ? "NO" : "-",
            cfg->modbusTcpEnabled ? "ON" : "OFF");

        stampaF(4, "PLC:%s  Scr:%s",
            mdb->plcData.valid ? "OK" : "WAIT",
            mdb->hasWritePending() ? "Q" : "-");

        unsigned long up = millis() / 1000;
        if (up < 60)       stampaF(5, "Up:%lus", up);
        else if (up < 3600) stampaF(5, "Up:%lum%lus", up / 60, up % 60);
        else                stampaF(5, "Up:%luh%lum", up / 3600, (up % 3600) / 60);
    }

    void pRegistri(ModbusHandler* mdb, GatewayConfig* cfg, int startIdx) {
        u8g2.setFont(u8g2_font_6x10_tf);
        stampa(0, "=== REGISTRI ===");

        for (int i = 0; i < 5 && (startIdx + i) < NUM_REGS; i++) {
            int idx = startIdx + i;
            float val = mdb->plcData.registers[idx] * cfg->regScaling[idx];
            String nome = String(cfg->regNames[idx]).substring(0, 8);
            stampaF(i + 1, "%s:%.1f", nome.c_str(), val);
        }

        if (!mdb->plcData.valid) {
            stampa(5, "Dati non validi");
        }
    }

    void pExtra(ModbusHandler* mdb, WiFiManagerClass* wifi) {
        u8g2.setFont(u8g2_font_6x10_tf);
        stampa(0, "=== EXTRA ===");
        stampaF(1, "RSSI:%d dBm", WiFi.RSSI());
        stampaF(2, "Heap:%u KB", ESP.getFreeHeap() / 1024);

        if (mdb->plcData.valid) {
            unsigned long eta = (millis() - mdb->plcData.lastUpdate) / 1000;
            stampaF(3, "Ult.agg:%lus fa", eta);
        } else {
            stampa(3, "Ult.agg:mai");
        }

        u8g2.drawHLine(0, 45, 128);
        stampaStr(4, String("IP:") + wifi->getIP());
        stampaF(5, "Scritture:%s", mdb->hasWritePending() ? "CODA" : "OK");
    }
};

#endif
