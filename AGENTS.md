# PZEM Multi-Fase Energy Monitor

## Descricao Geral

Firmware para ESP32 que monitora ate 3 fases eletricas usando modulos PZEM-004T V3.0
em barramento Modbus-RTU compartilhado, mais sensor de temperatura AHT10 (I2C).
Publica dados individuais por fase via MQTT para um broker central.
Possui modo de configuracao inicial (Captive Portal) via Access Point.

## Hardware

| Componente | Especificacao |
|-----------|---------------|
| MCU | ESP32 (ESP32-WROOM-32) |
| Sensor Fase 1 | PZEM-004T V3.0 — Modbus 0x01 |
| Sensor Fase 2 | PZEM-004T V3.0 — Modbus 0x02 |
| Sensor Fase 3 | PZEM-004T V3.0 — Modbus 0x03 |
| Temperatura | AHT10 — I2C (SDA=21, SCL=22) |
| Botao Config | GPIO0 (BOOT) — 3s boot = AP, 5s running = reset |

## Conexoes

| PZEM-004T | ESP32 |
|-----------|-------|
| TX -> RX2 | GPIO16 |
| RX -> TX2 | GPIO17 |
| 5V -> 5V | |
| GND -> GND | |

## Plataforma

- Framework: PlatformIO + Arduino
- Board: esp32dev (espressif32)
- Monitor: 115200 baud

### Dependencias (platformio.ini)

```ini
lib_deps =
    mandulaj/PZEM-004T-v30
    knolleary/PubSubClient
    bblanchon/ArduinoJson
    me-no-dev/ESP Async WebServer
    me-no-dev/AsyncTCP
    adafruit/Adafruit AHTX0
```

## Estrutura de Arquivos

```
pzem/
├── platformio.ini
├── AGENTS.md
├── src/
│   ├── main.cpp               # Boot logic + dashboard
│   ├── ConfigManager.h/.cpp   # Preferences (NVS) wrapper
│   ├── CaptivePortal.h/.cpp   # AP 192.168.4.1 + form HTML
│   ├── SensorManager.h/.cpp   # 3x PZEM + AHT10
│   ├── AlarmManager.h/.cpp    # Threshold engine + transicoes
│   ├── TimeManager.h/.cpp     # NTP + timestamp HH:MM DD/MM/YYYY
│   └── MqttManager.h/.cpp     # PubSubClient wrapper (5 topicos)
```

## Fluxo de Boot (main.cpp)

```
setup()
├─ Preferences.begin("pzem", false)
├─ pinMode(GPIO0, INPUT_PULLUP)
├─ incrementBootCount()
├─ Se GPIO0 LOW >3s OU wifi_ssid vazio:
│   └─ CaptivePortal::start()
│       ├─ WiFi.softAP("ESP32_Config")
│       ├─ AsyncWebServer(80)
│       │   GET  /       → HTML (PROGMEM)
│       │   POST /save   → salva Preferences + ESP.restart()
│       │   POST /reset  → limpa Preferences + ESP.restart()
│       └─ while(true) loop eterno
│
└─ Se config OK:
    ├─ WiFi.begin(wifi_ssid, wifi_pass)
    ├─ configTime (NTP)
    ├─ MqttManager::connect()
    │   subscribe({prefix}/config)
    │   subscribe({prefix}/time)
    │   publish({prefix}/status, "online", QoS1, retain)
    │   set LWT: {prefix}/status = {"status":"offline"}
    ├─ SensorManager::init()
    │   Serial2.begin(9600, SERIAL_8N1, 16, 17)
    │   Wire.begin(21, 22)
    │   for i in 0..2 → new PZEM004Tv30(Serial2, 16, 17, PZEM_ADDR[i])
    │   new Adafruit_AHTX0()
    └─ loop()
        ├─ mqtt.loop() + reconnect
        ├─ check GPIO0 → 5s hold → clear + restart AP
        ├─ a cada send_interval_s:
        │   for i in 0..num_phases-1:
        │       v = pzems[i]->voltage() * v_factor[i]
        │       a = pzems[i]->current() * a_factor[i]
        │       w = pzems[i]->power() * w_factor[i]
        │       kwh = pzems[i]->energy()
        │       hz = pzems[i]->frequency()
        │       pf = pzems[i]->pf()
        │   publish({prefix}/data)
        ├─ a cada send_interval_s:
        │   publish({prefix}/data)
        ├─ alarmes:
        │   getNewAlarms() → publish({prefix}/alarm, "ALARM")
        │   getResolvedAlarms() → publish({prefix}/alarm, "RESOLVED")
        │   getActiveAlarms() → publish({prefix}/alarm, "ALARM_REFRESH")
```

## Modelo de Configuracao (Preferences NVS)

Namespace: `pzem`

| Chave | Tipo | Default | Descricao |
|-------|------|---------|-----------|
| wifi_ssid | string | "" | WiFi SSID |
| wifi_pass | string | "" | WiFi senha |
| mqtt_host | string | "" | Broker host |
| mqtt_port | uint16 | 1883 | Broker porta |
| mqtt_user | string | "" | Broker usuario |
| mqtt_pass | string | "" | Broker senha |
| topic_prefix | string | "vigiatech/setor/painel/CT" | Prefixo topicos |
| num_phases | uint8 | 1 | 1-3 |
| nominal_voltage | float | 127.0 | 127/220/380 |
| send_interval_s | uint16 | 5 | Intervalo envio (1-3600) |
| alarm_interval_s | uint16 | 60 | Intervalo alarme (5-3600) |
| v_max | float | 242.0 | Limite sobretensao |
| v_min | float | 198.0 | Limite subtensao |
| a_max | float | 100.0 | Limite sobrecorrente |
| a_min | float | 0.5 | Limite subcorrente |
| temp_max | float | 80.0 | Limite temperatura |
| v_factor_0 | float | 1.0 | Correcao tensao Fase 1 |
| v_factor_1 | float | 1.0 | Correcao tensao Fase 2 |
| v_factor_2 | float | 1.0 | Correcao tensao Fase 3 |
| a_factor_0 | float | 1.0 | Correcao corrente Fase 1 |
| a_factor_1 | float | 1.0 | Correcao corrente Fase 2 |
| a_factor_2 | float | 1.0 | Correcao corrente Fase 3 |
| w_factor_0 | float | 1.0 | Correcao potencia Fase 1 |
| w_factor_1 | float | 1.0 | Correcao potencia Fase 2 |
| w_factor_2 | float | 1.0 | Correcao potencia Fase 3 |

## Topicos MQTT

Prefixo padrao: `vigiatech/setor/painel/CT`
(Configuravel via `topic_prefix`)

Os topicos sao montados como: `{prefixo}/sufixo`

### 1. `{prefixo}/data` — PUBLICAR (dispositivo -> broker)

Leituras eletricas trifasicas em tempo real.
Intervalo: configuravel (`send_interval_s`, padrao 5s)
QoS: 0 | Retain: nao

```json
{
  "datetime": "09:28 23/06/2026",
  "v_nominal": 220.0,
  "F1": {
    "v": 220.5,
    "a": 10.25,
    "w": 2250.0,
    "kwh": 1234.56,
    "hz": 60.0,
    "pf": 0.95,
    "valid": true
  },
  "F2": {
    "v": 220.1,
    "a": 11.2,
    "w": 2400.0,
    "kwh": 1500.0,
    "hz": 60.0,
    "pf": 0.93,
    "valid": true
  },
  "F3": {
    "v": 0.0,
    "a": 0.0,
    "w": 0.0,
    "kwh": 0.0,
    "hz": 0.0,
    "pf": 0.0,
    "valid": false
  }
}
```

### 2. `{prefixo}/config` — RECEBER (broker -> dispositivo)

Receber configuracao remota via MQTT.
QoS: 0 | Retain: nao

Payload (todos os campos sao OPCIONAIS):

```json
{
  "wifi_ssid": "MinhaRede",
  "wifi_pass": "senha",
  "mqtt_host": "broker.emqx.io",
  "mqtt_port": 1883,
  "mqtt_user": "usuario",
  "mqtt_pass": "senha",
  "mqtt_topic_prefix": "vigiatech/setor/painel/CT",
  "nominal_voltage": 220.0,
  "send_interval_s": 5,
  "alarm_interval_s": 60,
  "v_max": 242.0,
  "v_min": 198.0,
  "a_max": 100.0,
  "a_min": 0.5,
  "temp_max": 80.0,
  "v_factor": [1.0, 1.0, 1.0],
  "a_factor": [1.0, 1.0, 1.0],
  "w_factor": [1.0, 1.0, 1.0]
}
```

Resposta:
```json
{ "status": "ok", "message": "Config saved. Rebooting.", "timestamp": "09:28 23/06/2026" }
```

### 3. `{prefixo}/time` — RECEBER (broker -> dispositivo)

Sincronizar data/hora do dispositivo.
QoS: 0 | Retain: nao

Formatos aceitos:

Formato JSON:
```json
{ "epoch": 1750698000 }
```
ou
```json
{ "ts": 1750698000 }
```

Formato texto:
```
"09:28 23/06/2026"
```

### 4. `{prefixo}/alarm` — PUBLICAR (dispositivo -> broker)

Notificar alarmes (disparo, resolucao e reenvio periodico).
QoS: 0 | Retain: nao

Tipos de alarme:
- `OVERVOLTAGE` — sobretensao
- `UNDERVOLTAGE` — subtensao
- `OVERCURRENT` — sobrecorrente
- `UNDERCURRENT` — subcorrente
- `PHASE_LOSS` — perda de fase
- `HIGH_TEMP` — temperatura alta

#### 4a. Disparo (ALARM)

```json
{
  "type": "ALARM",
  "alarm": "PHASE_LOSS",
  "phase": 2,
  "value": 0.0,
  "threshold": 1.0,
  "datetime": "09:28 23/06/2026"
}
```

#### 4b. Resolucao (RESOLVED)

```json
{
  "type": "RESOLVED",
  "alarm": "PHASE_LOSS",
  "phase": 2,
  "value": 127.5,
  "threshold": 1.0,
  "datetime": "09:35 23/06/2026"
}
```

#### 4c. Reenvio periodico (ALARM_REFRESH)

Reenvia alarmes ainda ativos a cada `alarm_interval_s` (padrao 60s).

```json
{
  "type": "ALARM_REFRESH",
  "alarm": "PHASE_LOSS",
  "phase": 2,
  "datetime": "09:30 23/06/2026"
}
```

### 5. `{prefixo}/status` — PUBLICAR (dispositivo -> broker)

Informar status de conexao do dispositivo.
QoS: 1 | Retain: SIM | LWT: SIM

#### Online (na conexao)

```json
{
  "status": "online",
  "ip": "192.168.1.100"
}
```

#### Offline (LWT — desconexao inesperada)

O broker publica automaticamente:
```json
{
  "status": "offline"
}
```

## Resumo Topicos

| Topico | Direcao | QoS | Retain | LWT |
|--------|---------|-----|--------|-----|
| /data | Dispositivo>Broker | 0 | nao | nao |
| /config | Broker>Dispositivo | 0 | nao | nao |
| /time | Broker>Dispositivo | 0 | nao | nao |
| /alarm | Dispositivo>Broker | 0 | nao | nao |
| /status | Dispositivo>Broker | 1 | sim | sim |

## Modo de Configuracao (Captive Portal)

- WiFi AP: ESP32_Config (senha: vazio)
- IP fixo: 192.168.4.1
- Porta: 80
- Pagina: formulario HTML unico embutido em PROGMEM

### Endpoints do WebServer

| Metodo | Rota | Descricao |
|--------|------|-----------|
| GET | / | Formulario HTML |
| POST | /save | Salva config + reboot |
| POST | /reset | Limpa config + reboot |

### Trigger

- Boot: GPIO0 LOW por >3s → força AP
- Boot: WiFi SSID vazio na NVS → força AP
- Operacao: GPIO0 LOW por >5s → limpa config + reboot AP

## Algoritmo de Calibração

Cada leitura bruta do PZEM e multiplicada pelo fator de correcao por fase:

```cpp
float corrected = raw * correction_factor[phase];
```

O fator e default 1.0 (sem correcao). Deve ser ajustado comparando com um instrumento de referencia (ex: multimetro true RMS) e calculado como:

```
fator = leitura_referencia / leitura_pzem
```

## Tipos de Alarme

| Alarme | Condicao | Campo usado |
|--------|----------|-------------|
| OVERVOLTAGE | v > v_max | v_max |
| UNDERVOLTAGE | v < v_min | v_min |
| OVERCURRENT | a > a_max | a_max |
| UNDERCURRENT | a < a_min | a_min |
| PHASE_LOSS | v = 0 ou isnan(v) | v_min |
| HIGH_TEMP | temp > temp_max | temp_max |

## Regras de Implementacao

- Nao adicionar comentarios no codigo
- Seguir o padrao de nomes dos arquivos existentes (CamelCase, .h/.cpp)
- Usar constexpr para constantes fixas
- Preferir arrays C-style sobre std::vector em ambiente Arduino
- Evitar alocacao dinamica (new/malloc) no loop()
- JSON com StaticJsonDocument (tamanho fixo em stack)
- AHT10 lido apenas uma vez por ciclo (um unico sensor compartilhado)
- Todo pzem.read*() retorna NaN em falha — tratar com isnan()

## Guia de Deploy

### Requisitos

| Ferramenta | Versao minima | Uso |
|-----------|---------------|-----|
| PlatformIO CLI | 6.1+ | Compilacao e flash |
| esptool.py | 4.4+ | Flash manual (alternativo) |
| Python | 3.8+ | Dependencia do esptool |
| mosquitto-clients |qualquer | Teste MQTT (mosquitto_pub/sub) |

### Flash via PlatformIO

```bash
# Compilar
pio run

# Flash via USB (detecta porta automaticamente)
pio run --target upload

# Monitor serial apos flash
pio device monitor --baud 115200
```

### Flash manual (esptool)

```bash
# Apagar flash completa
esptool.py --chip esp32 --port COM3 --baud 460800 erase_flash

# Gravar firmware
esptool.py --chip esp32 --port COM3 --baud 460800 write_flash -z 0x1000 .pio/build/esp32dev/firmware.bin
```

### Primeiros passos apos flash

1. O ESP32 inicia em modo AP automaticamente (NVS vazia)
2. Conecte-se ao WiFi `ESP32_Config` (sem senha)
3. Acesse `http://192.168.4.1` no navegador
4. Preencha: WiFi SSID/senha, MQTT host/porta, numero de fases
5. Clique em Salvar — o ESP32 reinicia e conecta ao WiFi
6. Acesse o dashboard em `http://<IP_do_ESP32>/`

### Atualizacao OTA (futuro)

Atualmente so e possivel atualizar via cabo USB. OTA (Over-The-Air) nao esta implementado.

### Verificacao pos-deploy

```bash
# Verificar se o dispositivo esta online
mosquitto_sub -h <broker> -t "vigiatech/setor/painel/CT/status" -C 1

# Forcar leitura manual
mosquitto_pub -h <broker> -t "vigiatech/setor/painel/CT/config" -m '{"send_interval_s": 1}'
```

## Troubleshooting / FAQ

### Erros de conexao MQTT

| Sintoma | Causa possivel | Solucao |
|---------|---------------|---------|
| `MQTT falhou (rc=-2)` | Broker offline ou porta errada | Verificar host/porta no `/config` |
| `MQTT falhou (rc=-1)` | WiFi nao conectado | Verificar SSID/senha, checar RSSI |
| `MQTT falhou (rc=5)` | Credenciais invalidas | Verificar mqtt_user/mqtt_pass |
| `MQTT falhou (rc=4)` | Protocolo incompativel | Usar broker MQTT v3.1.1 |
| Reconexao infinita | Broker rejeita clientId | Resetar via GPIO0 (5s) e reconfigurar |

### Falhas de leitura PZEM

| Sintoma | Causa possivel | Solucao |
|---------|---------------|---------|
| `v: 0, valid: false` | PZEM sem alimentacao 5V | Verificar conexao 5V/GND |
| `v: 0, valid: false` | CT desconectado | Verificar CT+ / CT- no PZEM |
| `v: NaN` | Wiring TX/RX invertido | Cruzar TX-RX entre ESP32 e PZEM |
| `v: 0, valid: false` | Endereco Modbus errado | Verificar enderecos 0x01-0x03 |
| Todos os F1/F2/F3 invalidos | Serial2 configurada errado | Verificar GPIO16/17, baud 9600 |

### WiFi e Captive Portal

| Sintoma | Causa possivel | Solucao |
|---------|---------------|---------|
| Nao entra no AP | GPIO0 nao em LOW no boot | Segurar GPIO0 antes de ligar |
| AP aparece mas 192.168.4.1 nao responde | Cliente conectou em modo ESTA | Desconectar e reconectar ao AP |
| Config salva mas nao conecta | Senha WiFi incorreta | Resetar (GPIO0 5s) e reconfigurar |
| IP diferente na rede | DHCP atribuiu outro IP | Verificar no Serial Monitor |

### Sensor AHT10

| Sintoma | Causa possivel | Sintoma |
|---------|---------------|---------|
| `temperature: 25.0` sempre | AHT10 nao responde | Verificar wiring I2C (SDA=21, SCL=22) |
| `humidity: 0.0` | Sensor com defeito | Substituir modulo AHT10 |
| Leitura instavel | Cabo I2C muito longo | Manter < 20cm, usar pull-ups 4.7k |

### Alarmes

| Sintoma | Causa possivel | Solucao |
|---------|---------------|---------|
| `PHASE_LOSS` falso | `v_min` = 0 na config | Definir `v_min` > 0 para fases ativas |
| `UNDERCURRENT` falso | Fase desligada | `UNDERCURRENT` so dispara se `a > 0` |
| Alarme nao dispara | `alarm_interval_s` muito alto | Diminuir intervalo (min: 5s) |
| Alarme reenvia sempre | Alarme ativo nao resolvido | Verificar se condicao voltou ao normal |

### Erros de compilacao

| Erro | Causa | Solucao |
|------|-------|---------|
| `StaticJsonDocument` deprecated | ArduinoJson v7+ | Atualizar para `JsonDocument` (ArduinoJson v7) |
| `#include` nao encontrado | Library nao instalada | `pio lib install` ou verificar platformio.ini |
| `Serial2` nao declarado | Board nao suporta | Usar esp32dev com 2 UARTs |

## Metricas e Limites

### Uso de memoria

| Recurso | Capacidade | Uso atual | Percentual |
|---------|-----------|-----------|-----------|
| RAM | 320 KB | ~48 KB | ~15% |
| Flash | 1.3 MB | ~928 KB | ~71% |

### Buffer MQTT

| Topico | Tamanho buffer | Tamanho estimado JSON | Margem |
|--------|---------------|----------------------|--------|
| `/data` | 1536 bytes | ~600 bytes (3 fases) | ~60% livre |
| `/alarm` | 256 bytes | ~120 bytes | ~53% livre |
| `/status` | 512 bytes | ~200 bytes | ~61% livre |
| `/config` (resposta) | 192 bytes | ~80 bytes | ~58% livre |

### Limites operacionais

| Parametro | Minimo | Maximo | Default |
|-----------|--------|--------|---------|
| `send_interval_s` | 1 | 3600 | 5 |
| `alarm_interval_s` | 5 | 3600 | 60 |
| `num_phases` | 1 | 3 | 1 |
| Tensao medida | 0 V | 300 V | — |
| Corrente medida | 0 A | 100 A | — |
| Temperatura AHT10 | -40 C | +85 C | — |
| Fator de correcao | 0.1 | 10.0 | 1.0 |

### Limites do hardware

| Componente | Limite | Observacao |
|-----------|--------|-----------|
| ESP32 Serial2 | 9600 baud (PZEM) | PZEM so suporta 9600 |
| PZEM-004T | 100A max | CT integrado ou externo |
| PZEM-004T | 250V max | Tensao AC de rede |
| I2C (AHT10) | 400 kHz | Velocidade max do bus |
| WiFi | -30 dBm (ideal) | Minimo para estabilidade: -70 dBm |
| NVS | 50 chaves max | Namespace `pzem` |

### Performance do firmware

| Metrica | Valor |
|---------|-------|
| Tempo de boot | ~3-5 segundos |
| Tempo de conexao WiFi | ~2-10 segundos |
| Tempo de conexao MQTT | ~1-3 segundos |
| Intervalo de leitura PZEM | ~100 ms por fase |
| Total leitura 3 fases | ~300 ms |
| Dashboard refresh | 3 segundos (AJAX) |

## Seguranca

### Recomendacoes de rede

- **VLAN dedicada**: Colocar o ESP32 e o broker MQTT em uma VLAN separada da rede corporativa
- **Firewall**: Restringir acesso ao broker MQTT apenas dos IPs autorizados
- **SSID oculto**: Usar SSID oculto para a rede IoT (nao transmite beacon)
- **WPA3**: Usar WPA3 quando disponivel no roteador

### Autenticacao MQTT

| Parametro | Status | Observacao |
|-----------|--------|-----------|
| Username/Password | Suportado | Configuravel via NVS ou MQTT config |
| TLS/SSL | Nao suportado | PubSubClient nao inclui TLS nativo |
| Client ID | Automatico | `esp32_<MAC>` (unico por dispositivo) |
| LWT (Last Will) | Implementado | Publica `offline` no `/status` ao desconectar |

### Melhorias de seguranca sugeridas

1. **TLS com ESP-IDF**: Usar mbedTLS para conexao MQTT segura (requer refactor do PubSubClient)
2. **Assinatura HMAC**: Validar payload MQTT com chave compartilhada
3. **OTA com senha**: Proteger atualizacoes over-the-air com autenticacao
4. **Watchdog de hardware**: Configurar WDT para reiniciar em caso de hang
5. **Rate limiting**: Limitar conexoes simultaneas ao Captive Portal (max 1 client)

### Isolamento eletrico

- O PZEM-004T ja fornece isolamento galvanico via CT
- Manter fiação AC (120V/220V) fisicamente separada do ESP32
- Usar ecrã (shielding) no cabo Serial2 se houver interferencia
- Nao tocar nos bornes AC com o dispositivo ligado

### Logs e auditoria

- `boot_count` na NVS: rastreia quantas vezes o dispositivo reiniciou
- Serial Monitor: exibe erros em tempo real (115200 baud)
- MQTT `/status`: indica online/offline com IP
- MQTT `/alarm`: registra todos os alarmes com timestamp

### Backup de configuracao

Antes de resetar o dispositivo, anote a configuracao atual via MQTT:

```bash
mosquitto_sub -h <broker> -t "vigiatech/setor/painel/CT/status" -v -C 1
```

O payload `online` inclui `v_nominal` e `limits` atuais.
