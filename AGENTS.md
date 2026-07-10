# PZEM Multi-Fase Energy Monitor

## Descricao Geral

Firmware para ESP32 que monitora ate 3 fases eletricas usando modulos PZEM-004T V3.0
em barramento Modbus-RTU compartilhado, mais sensor de temperatura AHT10 (I2C).
Publica dados individuais por fase via MQTT para um broker central.
Possui modo de configuracao inicial (Captive Portal) via Access Point.

## Documentacao relacionada

| Documento | Conteudo |
|-----------|----------|
| `docs/HISTORICO_AJUSTES.md` | Changelog detalhado da reorganizacao do firmware em modulos `lib/` e das correcoes de robustez (reconexao Wi-Fi, delay bloqueante, bug de alarme novo/resolvido) |
| `docs/ALARMES.md` | Referencia completa de cada tipo de alarme: condicao de disparo, casos especiais, cenarios de teste |
| `docs/COMANDOS_MQTT.md` | Cookbook de comandos `mosquitto_pub/sub` para configurar e testar o dispositivo em campo |
| `docs/diagramas.md` | Referencia de hardware (pinagem PZEM-004T, conexao com ESP32, LEDs indicadores) |

## Hardware

| Componente | Especificacao |
|-----------|---------------|
| MCU | ESP32 (ESP32-WROOM-32) |
| Sensor Fase 1 | PZEM-004T V3.0 — Modbus 0x01 |
| Sensor Fase 2 | PZEM-004T V3.0 — Modbus 0x02 |
| Sensor Fase 3 | PZEM-004T V3.0 — Modbus 0x03 |
| Temperatura | AHT10 — I2C (SDA=21, SCL=22) |
| Botao Config | GPIO0 (BOOT) — 3s boot = AP, 5s running = reset (deteccao nao-bloqueante) |

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
- Ambiente de teste: `native` (Unity, roda no host sem placa conectada — ver secao de Testes)

### Dependencias (platformio.ini)

```ini
[env:esp32dev]
lib_deps =
    mandulaj/PZEM-004T-v30
    knolleary/PubSubClient
    bblanchon/ArduinoJson
    me-no-dev/ESPAsyncWebServer
    me-no-dev/AsyncTCP
    adafruit/Adafruit AHTX0
```

`build_flags` inclui `-Iinclude` — necessario porque o PlatformIO nao adiciona
`include/` automaticamente ao path de compilacao de bibliotecas privadas em
`lib/` (so o faz para `src/`).

`[platformio] default_envs = esp32dev` garante que `pio run` sem `-e` so
compila o firmware real — o ambiente `native` (usado só para testes) tentaria
compilar `src/main.cpp`, que depende do Arduino/ESP32 e falharia.

## Estrutura de Arquivos

```
firmware-HBR-pzme/
├── platformio.ini
├── .clang-format                   # estilo LLVM
├── AGENTS.md
├── docs/
│   ├── HISTORICO_AJUSTES.md
│   ├── ALARMES.md
│   ├── COMANDOS_MQTT.md
│   └── diagramas.md
├── src/
│   └── main.cpp                    # so orquestracao: setup()/loop()
├── include/
│   ├── app_config.h                # pinos, tempos, defaults, NTP, MQTT
│   ├── config.h                    # struct Config (dado puro, sem hardware)
│   └── phase_data.h                # struct PhaseData (dado puro, sem hardware)
├── lib/
│   ├── wifi_manager/                # conexao STA + reconexao nao-bloqueante
│   ├── mqtt_manager/                # PubSubClient wrapper (5 topicos)
│   ├── pzem_sensor/                 # leitura das 3 fases via Modbus
│   ├── aht10_sensor/                # leitura de temperatura via I2C
│   ├── alarm_manager/               # motor de limites + transicoes
│   ├── config_manager/              # Preferences (NVS) wrapper
│   ├── time_manager/                # NTP + timestamp HH:MM DD/MM/YYYY
│   └── captive_portal/              # AP 192.168.4.1 + form HTML
└── test/
    └── test_alarm_manager/          # Unity, ambiente native (`pio test -e native`)
```

Cada pasta de `lib/` e um modulo compilado isoladamente, expondo so o que
esta no `.h` — equivalente ao `components/` do ESP-IDF. `PhaseData` e
`Config` vivem em `include/` (nao dentro das libs de hardware) justamente
para poderem ser usados por `alarm_manager` sem puxar `<Preferences.h>`/
`<PZEM004Tv30.h>`, o que permite testar `alarm_manager` no ambiente `native`.

## Fluxo de Boot (main.cpp)

```
setup()
├─ ConfigManager::load() + incrementBootCount()
├─ pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP)
├─ checkGpioBoot(): GPIO0 LOW >3s → shouldRebootAp = true
├─ Se shouldRebootAp OU wifi_ssid vazio:
│   └─ CaptivePortal::start()
│       ├─ WiFi.softAP(AP_SSID)          # sem senha (POC uso interno)
│       ├─ AsyncWebServer(80)
│       │   GET  /       → HTML (PROGMEM)
│       │   POST /save   → salva Preferences + ESP.restart()
│       │   POST /reset  → limpa Preferences + ESP.restart()
│       └─ while(true) loop eterno (bloqueante, é o modo de configuracao)
│
└─ Se config OK:
    ├─ WifiManager::connect() — bloqueante so no boot, ate WIFI_CONNECT_MAX_ATTEMPTS
    ├─ configTime (NTP, via TimeManager::init())
    ├─ MqttManager::connect()
    │   subscribe({prefix}/config)
    │   subscribe({prefix}/time)
    │   publish({prefix}/status, "online", QoS1, retain)
    │   set LWT: {prefix}/status = {"status":"offline"}
    ├─ PzemSensor::init()   → Serial2.begin(9600, SERIAL_8N1, RX=16, TX=17)
    ├─ Aht10Sensor::init()  → Wire.begin(SDA=21, SCL=22)
    └─ loop()
        ├─ wifiManager.maintain()         → reconecta sozinho se cair (nao-bloqueante)
        ├─ mqtt.loop() + reconnect (so tenta se Wi-Fi ja estiver conectado)
        ├─ checkGpioRuntime(): GPIO0 LOW >5s → reset config + AP (nao-bloqueante)
        ├─ a cada send_interval_s:
        │   pzemSensor.read(phases, ...) + aht10Sensor.getTemperature()
        │   mqttManager.publishData({prefix}/data)
        └─ (com MQTT conectado) alarmes:
            alarmManager.check()
            getNewAlarms()      → publish({prefix}/alarm, "ALARM")
            getResolvedAlarms() → publish({prefix}/alarm, "RESOLVED")
            a cada alarm_interval_s:
            getActiveAlarms()   → publish({prefix}/alarm, "ALARM_REFRESH")
```

Nenhuma etapa do `loop()` bloqueia por mais que o tempo de uma leitura de
sensor — ver `docs/HISTORICO_AJUSTES.md` (secao 2.2) para o detalhe da
correcao que eliminou o bloqueio do `checkGpioRuntime()`.

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
| v_min | float | 198.0 | Limite subtensao (tambem liga/desliga PHASE_LOSS — ver docs/ALARMES.md) |
| a_max | float | 100.0 | Limite sobrecorrente |
| a_min | float | 0.5 | Limite subcorrente |
| temp_max | float | 80.0 | Limite temperatura |
| v_factor_0..2 | float | 1.0 | Correcao tensao por fase |
| a_factor_0..2 | float | 1.0 | Correcao corrente por fase |
| w_factor_0..2 | float | 1.0 | Correcao potencia por fase |

Estrutura definida em `include/config.h` (`struct Config`).

## Topicos MQTT

Prefixo padrao: `vigiatech/setor/painel/CT` (configuravel via `topic_prefix`).
Topicos montados como `{prefixo}/sufixo`. Payloads completos e exemplos de
comando estao em `docs/COMANDOS_MQTT.md`.

| Topico | Direcao | QoS | Retain | LWT |
|--------|---------|-----|--------|-----|
| `/data` | Dispositivo→Broker | 0 | nao | nao |
| `/config` | Broker→Dispositivo | 0 | nao | nao |
| `/time` | Broker→Dispositivo | 0 | nao | nao |
| `/alarm` | Dispositivo→Broker | 0 | nao | nao |
| `/status` | Dispositivo→Broker | 1 | sim | sim |

## Tipos de Alarme

| Alarme | Condicao | Campo usado | Por fase? |
|--------|----------|-------------|-----------|
| OVERVOLTAGE | v > v_max | v_max | Sim |
| UNDERVOLTAGE | v < v_min | v_min | Sim |
| OVERCURRENT | a > a_max | a_max | Sim |
| UNDERCURRENT | a > 0 && a < a_min | a_min | Sim |
| PHASE_LOSS | !valid && v_min > 0 | v_min (liga/desliga) | Sim |
| HIGH_TEMP | !isnan(temp) && temp > temp_max | temp_max | Nao (phase: 0) |

Ver `docs/ALARMES.md` para condicoes completas, casos especiais (ex.: `a == 0`
nao dispara UNDERCURRENT, `v_min == 0` desativa PHASE_LOSS) e o ciclo de vida
`ALARM`/`RESOLVED`/`ALARM_REFRESH`.

## Regras de Implementacao

- Nao adicionar comentarios no codigo (exceto para explicar o "porque" de algo
  nao-obvio, nunca o "o que")
- Pastas de `lib/` e nomes de arquivo em snake_case (`alarm_manager/alarm_manager.h`);
  nomes de classe continuam em PascalCase (`AlarmManager`, `PzemSensor`)
- Usar constexpr para constantes fixas, centralizadas em `include/app_config.h`
- Preferir arrays C-style sobre std::vector em ambiente Arduino
- Evitar alocacao dinamica (new/malloc) no loop()
- Evitar `delay()` bloqueante dentro de `loop()` — usar maquina de estados com `millis()`
- JSON com StaticJsonDocument (tamanho fixo em stack)
- AHT10 lido apenas uma vez por ciclo (um unico sensor compartilhado)
- Todo pzem.read*() retorna NaN em falha — tratar com `std::isnan()`
- Structs de dado puro (`Config`, `PhaseData`) ficam em `include/`, sem incluir
  headers de hardware — permite testar logica de negocio (`alarm_manager`) no
  ambiente `native`

## Testes

`alarm_manager` e testado nativamente (sem placa conectada), porque opera so
sobre dados em memoria:

```bash
pio test -e native
```

13 casos cobrindo cada tipo de alarme e o ciclo `ALARM`/`RESOLVED`/`ALARM_REFRESH`
em `test/test_alarm_manager/test_alarm_manager.cpp`. `config_manager` ainda nao
tem testes automatizados — depende de `Preferences` (NVS do ESP32), precisaria
de uma camada de abstracao antes de rodar nativamente.

## Guia de Deploy

### Requisitos

| Ferramenta | Versao minima | Uso |
|-----------|---------------|-----|
| PlatformIO CLI | 6.1+ | Compilacao e flash |
| esptool.py | 4.4+ | Flash manual (alternativo) |
| Python | 3.8+ | Dependencia do esptool |
| mosquitto-clients | qualquer | Teste MQTT (mosquitto_pub/sub) |

### Flash via PlatformIO

```bash
# Compilar
pio run

# Flash via USB (detecta porta automaticamente)
pio run --target upload

# Monitor serial apos flash
pio device monitor --baud 115200

# Rodar testes automatizados (nao precisa de placa)
pio test -e native
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
2. Conecte-se ao WiFi `ESP32_Config` (sem senha — POC de uso interno, ver
   `docs/HISTORICO_AJUSTES.md` secao "Itens discutidos e adiados")
3. Acesse `http://192.168.4.1` no navegador
4. Preencha: WiFi SSID/senha, MQTT host/porta, numero de fases
5. Clique em Salvar — o ESP32 reinicia e conecta ao WiFi
6. Acesse o dashboard em `http://<IP_do_ESP32>/`

### Atualizacao OTA (futuro)

Atualmente so e possivel atualizar via cabo USB. OTA (Over-The-Air) nao esta
implementado.

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
| Wi-Fi caiu durante operacao | Router reiniciou/sinal fraco | Deve reconectar sozinho em ate ~5s apos o link voltar (ver `wifi_manager`) — se nao reconectar, checar `docs/HISTORICO_AJUSTES.md` |

### Sensor AHT10

| Sintoma | Causa possivel | Solucao |
|---------|---------------|---------|
| `temperature: 25.0` sempre | AHT10 nao responde | Verificar wiring I2C (SDA=21, SCL=22) |
| `humidity: 0.0` | Sensor com defeito | Substituir modulo AHT10 |
| Leitura instavel | Cabo I2C muito longo | Manter < 20cm, usar pull-ups 4.7k |
| Sensor falhou mas nenhum alarme dispara | Comportamento esperado — `NaN` nao gera `HIGH_TEMP` | Monitorar `temperature: null` no topico `/data` separadamente |

### Alarmes

| Sintoma | Causa possivel | Solucao |
|---------|---------------|---------|
| `PHASE_LOSS` falso/nunca dispara | `v_min` = 0 na config | Definir `v_min` > 0 para fases ativas |
| `UNDERCURRENT` falso | Fase desligada | `UNDERCURRENT` so dispara se `a > 0` |
| Alarme nao dispara | `alarm_interval_s` muito alto | Diminuir intervalo (min: 5s) — isso so afeta o `ALARM_REFRESH`, nao o `ALARM`/`RESOLVED` |
| So aparece `ALARM_REFRESH`, nunca `ALARM`/`RESOLVED` | Firmware rodando e anterior a correcao do bug de deteccao (ver `docs/HISTORICO_AJUSTES.md` 2.3) | Regravar com a versao atual |
| Alarme oscilando rapido entre ALARM/RESOLVED | Condicao fisica instavel (fio solto, tensao no limite) | Comportamento esperado — investigar a instabilidade fisica |

Detalhamento completo em `docs/ALARMES.md`.

### Erros de compilacao

| Erro | Causa | Solucao |
|------|-------|---------|
| `StaticJsonDocument` deprecated | ArduinoJson v7+ | Aviso conhecido, nao impede compilacao |
| `#include` nao encontrado | Library nao instalada | `pio lib install` ou verificar platformio.ini |
| `Serial2` nao declarado | Board nao suporta | Usar esp32dev com 2 UARTs |
| `fatal error: Arduino.h` ao compilar `pio run` sem `-e` | Ambiente `native` tentando compilar `src/main.cpp` | Confirmar `default_envs = esp32dev` no `platformio.ini` |

## Metricas e Limites

### Uso de memoria

| Recurso | Capacidade | Uso atual | Percentual |
|---------|-----------|-----------|-----------|
| RAM | 320 KB | ~48 KB | ~15% |
| Flash | 1.3 MB | ~931 KB | ~71% |

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
| Alarmes por ciclo (`MAX_ALARMS`) | — | 12 | — |

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
| Tempo de reconexao Wi-Fi (se cair) | ate 5s para detectar + tentar de novo |
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

### Captive Portal (AP de configuracao)

O AP `ESP32_Config` sobe **sem senha** — decisao deliberada para esta fase de
POC de uso interno (sem exposicao externa). Se o dispositivo for para
instalacao em campo/ambiente nao controlado, revisitar essa decisao — opcoes
discutidas: senha estatica compartilhada vs. senha por dispositivo derivada
do MAC/eFuse. Ver `docs/HISTORICO_AJUSTES.md`.

### Melhorias de seguranca sugeridas

1. **TLS com ESP-IDF**: Usar mbedTLS para conexao MQTT segura (requer refactor do PubSubClient)
2. **Assinatura HMAC**: Validar payload MQTT com chave compartilhada
3. **OTA com senha**: Proteger atualizacoes over-the-air com autenticacao
4. **Watchdog de hardware**: Configurar WDT para reiniciar em caso de hang
5. **Rate limiting**: Limitar conexoes simultaneas ao Captive Portal (max 1 client)
6. **Senha no AP de configuracao**: ver secao acima

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
