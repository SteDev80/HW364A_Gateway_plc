/*
 * ModbusHandler.h - Gestione Modbus RTU (RS485) e Modbus TCP (Wi-Fi)
 * Legge ciclicamente i registri dal PLC e li espone in rete
 */

#ifndef MODBUS_HANDLER_H
#define MODBUS_HANDLER_H

#include <ModbusMaster.h>
#include "ConfigManager.h"

struct PlcData {
    uint16_t registers[NUM_REGS] = { 0 };
    uint32_t lastUpdate = 0;
    bool valid = false;
};

// Funzioni statiche per controllo RS485 (callback ModbusMaster)
static uint8_t _rs485TxPin = 2;

static void _rs485PreTx() {
    digitalWrite(_rs485TxPin, HIGH);
    delayMicroseconds(100);
}

static void _rs485PostTx() {
    delayMicroseconds(2000);
    digitalWrite(_rs485TxPin, LOW);
}

class ModbusHandler {
public:
    PlcData plcData;

    void begin(GatewayConfig* cfg) {
        config = cfg;

        _rs485TxPin = config->rtuTxEnablePin;
        pinMode(_rs485TxPin, OUTPUT);
        digitalWrite(_rs485TxPin, LOW);

        Serial.begin(config->rtuBaudRate, SERIAL_8E1);
        node.begin(config->rtuSlaveId, Serial);

        node.preTransmission(_rs485PreTx);
        node.postTransmission(_rs485PostTx);

        if (config->modbusTcpEnabled) {
            tcpServer = new WiFiServer(config->tcpPort);
            tcpServer->begin();
        }
    }

    void handle() {
        gestisciPollingRTU();
        gestisciTCPServer();
    }

    void requestWrite(uint16_t address, uint16_t value) {
        pendingWrite.address = address;
        pendingWrite.value = value;
        pendingWrite.pending = true;
    }

    bool hasWritePending() {
        return pendingWrite.pending;
    }

private:
    GatewayConfig* config = nullptr;
    ModbusMaster node;

    enum RtuState { RTU_IDLE, RTU_ATTESA };
    RtuState statoRTU = RTU_IDLE;
    uint32_t ultimoPoll = 0;
    uint32_t inizioRichiesta = 0;
    static const uint32_t TIMEOUT_RISPOSTA = 1000;
    bool writeInCorso = false;

    struct {
        uint16_t address = 0;
        uint16_t value = 0;
        bool pending = false;
    } pendingWrite;

    WiFiServer* tcpServer = nullptr;
    WiFiClient tcpClient;

    void gestisciPollingRTU() {
        switch (statoRTU) {
            case RTU_IDLE: {
                if (pendingWrite.pending) {
                    writeInCorso = true;
                    uint8_t res = node.writeSingleRegister(pendingWrite.address, pendingWrite.value);
                    if (res == node.ku8MBSuccess) {
                        statoRTU = RTU_ATTESA;
                        inizioRichiesta = millis();
                    } else {
                        pendingWrite.pending = false;
                        writeInCorso = false;
                        ultimoPoll = millis();
                    }
                    break;
                }

                if (millis() - ultimoPoll >= config->rtuPollInterval) {
                    uint8_t res = node.readHoldingRegisters(config->rtuStartAddress, NUM_REGS);
                    if (res == node.ku8MBSuccess) {
                        statoRTU = RTU_ATTESA;
                        inizioRichiesta = millis();
                        writeInCorso = false;
                    } else {
                        ultimoPoll = millis();
                    }
                }
                break;
            }

            case RTU_ATTESA: {
                if (node.available() > 0) {
                    if (writeInCorso) {
                        pendingWrite.pending = false;
                    } else {
                        for (int i = 0; i < NUM_REGS; i++) {
                            plcData.registers[i] = node.getResponseBuffer(i);
                        }
                        plcData.lastUpdate = millis();
                        plcData.valid = true;
                    }
                    statoRTU = RTU_IDLE;
                    ultimoPoll = millis();
                } else if (millis() - inizioRichiesta > TIMEOUT_RISPOSTA) {
                    if (writeInCorso) {
                        pendingWrite.pending = false;
                    }
                    statoRTU = RTU_IDLE;
                    ultimoPoll = millis();
                }
                break;
            }
        }
    }

    void gestisciTCPServer() {
        if (!config->modbusTcpEnabled || !tcpServer) return;

        if (!tcpClient.connected()) {
            tcpClient = tcpServer->available();
            if (tcpClient) tcpClient.setTimeout(1000);
            return;
        }

        if (tcpClient.available() < 8) return;

        uint8_t mbap[7];
        tcpClient.readBytes(mbap, 7);
        uint16_t transactionId = (mbap[0] << 8) | mbap[1];
        uint16_t length = (mbap[4] << 8) | mbap[5];
        uint8_t unitId = mbap[6];

        if (length < 2) {
            tcpClient.stop();
            return;
        }

        uint8_t fc = tcpClient.read();

        switch (fc) {
            case 0x03: {
                uint8_t buf[4];
                if (tcpClient.readBytes(buf, 4) < 4) { tcpClient.stop(); return; }
                uint16_t startAddr = (buf[0] << 8) | buf[1];
                uint16_t quantity = (buf[2] << 8) | buf[3];
                if (quantity > 125 || quantity == 0) { inviaErroreTCP(transactionId, unitId, fc, 0x03); break; }
                if (startAddr + quantity > NUM_REGS) { inviaErroreTCP(transactionId, unitId, fc, 0x02); break; }

                uint8_t pdu[2 + quantity * 2];
                pdu[0] = fc;
                pdu[1] = quantity * 2;
                for (uint16_t i = 0; i < quantity; i++) {
                    pdu[2 + i * 2] = (plcData.registers[startAddr + i] >> 8) & 0xFF;
                    pdu[2 + i * 2 + 1] = plcData.registers[startAddr + i] & 0xFF;
                }

                inviaTCP(transactionId, unitId, pdu, 2 + quantity * 2);
                break;
            }

            case 0x06: {
                uint8_t buf[4];
                if (tcpClient.readBytes(buf, 4) < 4) { tcpClient.stop(); return; }
                uint16_t addr = (buf[0] << 8) | buf[1];
                uint16_t value = (buf[2] << 8) | buf[3];
                if (addr >= NUM_REGS) { inviaErroreTCP(transactionId, unitId, fc, 0x02); break; }

                requestWrite(addr, value);

                uint8_t pdu[5];
                pdu[0] = fc;
                pdu[1] = buf[0]; pdu[2] = buf[1];
                pdu[3] = buf[2]; pdu[4] = buf[3];
                inviaTCP(transactionId, unitId, pdu, 5);
                break;
            }

            case 0x10: {
                uint8_t buf[5];
                if (tcpClient.readBytes(buf, 5) < 5) { tcpClient.stop(); return; }
                uint16_t addr = (buf[0] << 8) | buf[1];
                uint16_t quantity = (buf[2] << 8) | buf[3];
                uint8_t byteCount = buf[4];
                if (addr + quantity > NUM_REGS) { inviaErroreTCP(transactionId, unitId, fc, 0x02); break; }

                for (uint16_t i = 0; i < quantity && i < byteCount / 2; i++) {
                    int b = tcpClient.read();
                    if (b < 0) break;
                    int b2 = tcpClient.read();
                    if (b2 < 0) break;
                    uint16_t val = (b << 8) | b2;
                    requestWrite(addr + i, val);
                }

                uint8_t pdu[5];
                pdu[0] = fc;
                pdu[1] = buf[0]; pdu[2] = buf[1];
                pdu[3] = buf[2]; pdu[4] = buf[3];
                inviaTCP(transactionId, unitId, pdu, 5);
                break;
            }

            default:
                inviaErroreTCP(transactionId, unitId, fc, 0x01);
                break;
        }

        tcpClient.stop();
    }

    void inviaTCP(uint16_t transactionId, uint8_t unitId, uint8_t* pdu, uint16_t pduLen) {
        uint16_t lunghezza = 1 + pduLen; // Unit ID + PDU

        uint8_t resp[7 + pduLen];
        resp[0] = (transactionId >> 8) & 0xFF;
        resp[1] = transactionId & 0xFF;
        resp[2] = 0; resp[3] = 0;
        resp[4] = (lunghezza >> 8) & 0xFF;
        resp[5] = lunghezza & 0xFF;
        resp[6] = unitId;
        memcpy(resp + 7, pdu, pduLen);

        tcpClient.write(resp, 7 + pduLen);
        tcpClient.flush();
    }

    void inviaErroreTCP(uint16_t transactionId, uint8_t unitId, uint8_t fc, uint8_t exceptionCode) {
        uint8_t pdu[2];
        pdu[0] = fc | 0x80;
        pdu[1] = exceptionCode;
        inviaTCP(transactionId, unitId, pdu, 2);
    }
};

#endif
