# HW364A Gateway PLC

Gateway industriale basato su **ESP8266 (NodeMCU)** con interfaccia **RS485 (MAX3485)** e display **OLED 128x64**.

Legge un **Siemens S7-300** (e qualsiasi dispositivo Modbus RTU) e lo espone su WiFi con:

- **Web Dashboard** — interfaccia live con AJAX
- **Modbus TCP** — bridge RTU → TCP
- **MQTT** — pubblicazione periodica registri
- **API REST** — endpoint JSON per integrazione

## Hardware

| Componente | PIN |
|---|---|
| RS485 (MAX3485) | RX=D7 (RX), TX=D3 (TX), DE/RE=D4 (GPIO2) |
| OLED SSD1306 | SDA=D6 (GPIO14), SCL=D5 (GPIO12) |
| LED | D0 (GPIO16) |

## Dipendenze

- [U8g2](https://github.com/olikraus/u8g2) — display OLED
- [ArduinoJson](https://arduinojson.org/) v6 — parsing/serializzazione JSON
- [ModbusMaster](https://github.com/4-20ma/ModbusMaster) — comunicazione Modbus RTU
- [PubSubClient](https://github.com/knolleary/pubsubclient) — client MQTT
- ESP8266WiFi / ESP8266WebServer — stack WiFi e web

## Configurazione

1. Alla prima accensione il dispositivo crea una rete WiFi `HW364A-AP`.
2. Collegati e vai su `http://192.168.4.1/wifi`, inserisci le credenziali della tua rete.
3. Dopo il riavvio, raggiungi il gateway a `http://hw364a.local` (mDNS).
4. Vai su **Configurazione** per impostare parametri Modbus, MQTT e registri.
5. Clicca **Salva e Riavvia**.

## Protocolli

- **Web Dashboard**: `/` — tabella registri con refresh automatico
- **API JSON**: `/api/data` — dump dei valori correnti
- **API Config**: `/api/config` — GET/POST configurazione
- **API Scrittura**: `/api/write` — POST per scrivere un registro
- **Modbus TCP**: porta `502` (default) — bridge ai registri configurati
- **MQTT**: publish periodico su topic configurabile

## Schematismo

```
┌──────────────┐      RS485       ┌──────────────┐
│  Siemens     │◄────►│  MAX3485  │◄────►│  ESP8266   │
│  S7-300      │      └──────────┘      │  NodeMCU   │
└──────────────┘                        │  (AP+STA)  │
                                        └──────┬──────┘
                                               │ WiFi
                                               ▼
                                        ┌──────────────┐
                                        │  Browser /   │
                                        │  MQTT Broker │
                                        │  Client TCP  │
                                        └──────────────┘
```

## Licenza

MIT
