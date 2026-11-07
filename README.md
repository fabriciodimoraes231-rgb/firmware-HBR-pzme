# Monitor PZEM: medição trifásica com ESP32

[![CI](https://github.com/fabriciodimoraes231-rgb/firmware-HBR-pzme/actions/workflows/ci.yml/badge.svg)](https://github.com/fabriciodimoraes231-rgb/firmware-HBR-pzme/actions/workflows/ci.yml)
![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-orange)
![License](https://img.shields.io/badge/license-Apache%202.0-blue)

Firmware para ESP32 que monitora um painel elétrico de até **3 fases** com medidores PZEM-004T num barramento **Modbus RTU** e publica as leituras por **MQTT**. Foi feito para rodar no campo sem supervisão: aguenta a queda do Wi-Fi sem perder dados e só encerra um alarme quando alguém confirma.

## Destaques

- **3 medidores num barramento só**: PZEM-004T V3.0 nos endereços Modbus `0x01`–`0x03`, mais um AHT10 (I2C) para a temperatura do painel.
- **Sem perda de dados**: buffer em RAM de 360 amostras (cerca de 30 min a 5 s) e 30 eventos de alarme, drenado em lotes quando a conexão volta.
- **Alarmes com ciclo de vida**: `ALARM` abre, `ALARM_REFRESH` reenvia a cada 60 s e `RESOLVED` só sai depois de uma confirmação no tópico `{prefixo}/alarm/ack`. Uma tensão que volta ao normal sozinha não esconde o problema.
- **6 tipos de alarme**: sobretensão, subtensão, sobrecorrente, subcorrente, perda de fase e temperatura alta.
- **Configuração sem recompilar**: captive portal no primeiro boot, dashboard web local e ajustes remotos por MQTT. Tudo fica salvo na NVS.
- **Robusto**: reconexão de Wi-Fi e MQTT sem bloquear o `loop()`, watchdog e nenhum `delay()` no caminho principal.
- **Testado sem placa**: a lógica de alarmes é isolada do hardware, e 19 testes Unity rodam no PC.

## Arquitetura

```
 Painel elétrico                ESP32                              Broker MQTT
┌──────────────┐  Modbus RTU  ┌──────────────────────────┐   Wi-Fi  ┌───────────┐
│ PZEM-004T F1 │──┐  9600 bd  │ pzem_sensor  aht10_sensor│          │ /data     │
│ PZEM-004T F2 │──┼──────────▶│ alarm_manager ──────────▶│─────────▶│ /alarm    │
│ PZEM-004T F3 │──┘ (GPIO16/17)│ offline_buffer           │◀─────────│ /config   │
└──────────────┘              │ mqtt_manager  wifi_manager│          │ /alarm/ack│
  AHT10 ── I2C (21/22) ──────▶│ captive_portal web_dash  │          │ /status   │
                              └──────────────────────────┘          └───────────┘
```

O código fica em 10 módulos dentro de `lib/`, cada um expondo só o seu `.h`. As structs de dados puros (`Config`, `PhaseData`) ficam em `include/`, sem dependência de hardware, e por isso o `alarm_manager` compila e roda no PC.

## Hardware

| Componente | Ligação |
|---|---|
| ESP32 (ESP32-WROOM-32, `esp32dev`) | — |
| PZEM-004T V3.0 ×3 | TX → GPIO16 (RX2), RX → GPIO17 (TX2), 5 V, GND |
| AHT10 | SDA = GPIO21, SCL = GPIO22 |
| Botão de configuração | GPIO0 (BOOT): 3 s no boot = modo AP, 5 s em operação = reset |

Pinagem e fotos dos módulos em [`docs/diagramas.md`](docs/diagramas.md).

## Como usar

Requer o [PlatformIO](https://platformio.org/).

```bash
pio run                      # compila o firmware
pio run --target upload      # grava na placa via USB
pio device monitor -b 115200 # monitor serial
pio test -e native           # roda os testes no PC, sem placa
```

No primeiro boot, o ESP32 abre o Wi-Fi `ESP32_Config`. Acesse `http://192.168.4.1`, preencha o Wi-Fi, o broker MQTT e o número de fases e salve. Depois disso, o dashboard fica em `http://<ip-do-esp32>/`.

## Tópicos MQTT

O prefixo é configurável (`topic_prefix`).

| Tópico | Direção | Conteúdo |
|---|---|---|
| `{prefixo}/data` | ESP32 → broker | Leituras por fase (tensão, corrente, potência, energia, FP, frequência) e temperatura |
| `{prefixo}/alarm` | ESP32 → broker | `ALARM`, `ALARM_REFRESH` e `RESOLVED` |
| `{prefixo}/alarm/ack` | broker → ESP32 | Confirmação que encerra um alarme |
| `{prefixo}/config` | broker → ESP32 | Ajuste de limites e intervalos |
| `{prefixo}/status` | ESP32 → broker | `online`/`offline` (retain + LWT) |

## Documentação

| Documento | Conteúdo |
|---|---|
| [`AGENTS.md`](AGENTS.md) | Referência técnica completa: boot, NVS, limites, métricas de memória, troubleshooting |
| [`docs/ALARMES.md`](docs/ALARMES.md) | Cada alarme: condição de disparo, casos especiais e roteiro de teste |
| [`docs/COMANDOS_MQTT.md`](docs/COMANDOS_MQTT.md) | Comandos `mosquitto_pub/sub` para configurar e testar em campo |
| [`docs/HISTORICO_AJUSTES.md`](docs/HISTORICO_AJUSTES.md) | Histórico das decisões e correções de robustez |

## Licença

[Apache 2.0](LICENSE) © Manuel Moraes
