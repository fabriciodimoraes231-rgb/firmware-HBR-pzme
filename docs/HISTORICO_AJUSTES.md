# Histórico de Ajustes do Firmware — PZEM Multi-Fase (HBR-PZME)

Este documento registra, em ordem cronológica, tudo o que foi feito neste firmware
em duas frentes: (1) reorganização estrutural do código e (2) correções de
robustez/comportamento. Serve como referência para quem for revisar, auditar ou
continuar este trabalho.

Nenhuma mudança de estrutura alterou nomes de tópicos MQTT, campos JSON, ou a lib
do PZEM/framework usado. Mudanças de comportamento (robustez) estão marcadas
explicitamente como tal.

---

## Parte 1 — Reorganização estrutural (refactor puro)

### Motivação

O firmware estava todo dentro de `src/`, sem separação de módulos — todos os
`.cpp`/`.h` soltos no mesmo nível. Isso dificultava reuso, teste isolado de cada
parte e não deixava claro os limites de responsabilidade entre elas. O objetivo
foi alinhar com o padrão usado no firmware irmão da VigiaTech em ESP-IDF, onde
cada módulo é um `component/` isolado — em PlatformIO, o equivalente direto é
`lib/`.

### Estrutura antes

```
src/
├── AlarmManager.cpp / .h
├── CaptivePortal.cpp / .h
├── ConfigManager.cpp / .h
├── main.cpp
├── MqttManager.cpp / .h
├── SensorManager.cpp / .h      (misturava PZEM + AHT10 numa unica classe)
└── TimeManager.cpp / .h
```

### Estrutura depois

```
platformio.ini
.clang-format                  (estilo LLVM)
src/
└── main.cpp                   (só orquestração: setup()/loop())
include/
├── app_config.h                (pinos, constantes de tempo, defaults, NTP, MQTT)
├── config.h                    (struct Config — dados puros, sem dependência de hardware)
└── phase_data.h                (struct PhaseData — dados puros, sem dependência de hardware)
lib/
├── wifi_manager/                (novo — extraído do main.cpp)
├── mqtt_manager/
├── pzem_sensor/                 (metade do antigo SensorManager)
├── aht10_sensor/                (metade do antigo SensorManager)
├── alarm_manager/
├── config_manager/
├── time_manager/
└── captive_portal/
test/
└── test_alarm_manager/          (testes Unity, ambiente native)
```

### O que foi feito, módulo por módulo

1. **`include/app_config.h`** — criado para centralizar constantes que estavam
   espalhadas/hardcoded pelo código: pinos (`LED_PIN`, `BOOT_BUTTON_PIN`,
   `PZEM_RX_PIN`/`PZEM_TX_PIN`, `I2C_SDA_PIN`/`I2C_SCL_PIN`), `MAX_PHASES` e
   `PZEM_ADDR[]`, constantes de tempo (`MQTT_RETRY_MS`, `GPIO_HOLD_MS`,
   `WDT_TIMEOUT_S`, timeouts de conexão Wi-Fi), configuração de NTP, SSID do AP
   e o prefixo MQTT default. O `topic_prefix` default estava duplicado como
   literal em `ConfigManager.h` e no HTML do captive portal — os dois passaram
   a referenciar a mesma constante (`DEFAULT_MQTT_TOPIC_PREFIX`).

2. **`config_manager`** — movido de `src/ConfigManager.*` para
   `lib/config_manager/`, sem mudança de lógica.

3. **`time_manager`** — movido de `src/TimeManager.*` para `lib/time_manager/`.

4. **`pzem_sensor` + `aht10_sensor`** — o antigo `SensorManager` (que misturava
   dois sensores fisicamente distintos numa única classe) foi dividido em dois
   módulos: `PzemSensor` (leitura das 3 fases via Modbus) e `Aht10Sensor`
   (temperatura via I2C). Precisou ser feito antes de mover o `alarm_manager`,
   porque `lib/` não enxerga `src/` diretamente — uma dependência (`PhaseData`)
   precisava já existir como módulo de `lib/` antes.

5. **`alarm_manager`** — movido de `src/AlarmManager.*` para `lib/alarm_manager/`.

6. **`mqtt_manager`** — movido de `src/MqttManager.*` para `lib/mqtt_manager/`.

7. **`captive_portal`** — movido de `src/CaptivePortal.*` para `lib/captive_portal/`.

8. **`wifi_manager`** — não existia como classe antes; a lógica de conexão Wi-Fi
   (WiFi.begin, loop de status, fallback pro AP) estava inline dentro de
   `setup()` em `main.cpp`. Foi extraída para um módulo novo, preservando o
   comportamento exatamente como estava nessa etapa (a melhoria de reconexão
   veio depois, na Parte 2).

### Detalhe técnico descoberto durante o processo

O PlatformIO **não adiciona `include/` automaticamente ao path de compilação
de bibliotecas privadas em `lib/`** — só o faz para `src/`. Isso quebrava a
compilação assim que o primeiro módulo foi movido. Corrigido adicionando
`-Iinclude` em `build_flags` no `platformio.ini`.

### Validação

Cada módulo foi movido individualmente e validado com `pio run` antes de passar
para o próximo (build incremental, nunca vários módulos de uma vez). RAM/Flash
permaneceram praticamente idênticos do início ao fim desta parte (~47.9 KB RAM,
~930 KB Flash), confirmando que nenhuma lógica mudou — só a organização dos
arquivos.

---

## Parte 2 — Correções de robustez (mudança de comportamento real)

Diferente da Parte 1, os itens abaixo alteram comportamento do firmware, não
só a estrutura. Cada um foi discutido e autorizado individualmente antes da
implementação.

### 2.1 — Reconexão de Wi-Fi

**Problema:** o Wi-Fi só era conectado uma vez, em `setup()`. Se a rede caísse
durante a operação, nada no código percebia isso — o firmware só monitorava o
MQTT, tentando reconectar a cada 5s independente do estado do Wi-Fi. Se o
roteador caísse, o dispositivo ficava offline para sempre (só um reboot manual
resolvia).

**Solução:** `WifiManager` ganhou `isConnected()` e `maintain(ssid, pass)`,
chamado a cada volta do `loop()`. Se desconectado e já tiver passado
`WIFI_RECONNECT_RETRY_MS` (5s) desde a última tentativa, dispara `WiFi.begin()`
de novo — de forma assíncrona, sem travar o `loop()`. O retry do MQTT passou a
só disparar se o Wi-Fi já estiver de volta. O LED (`LED_PIN`) passou a refletir
o estado da conexão a cada iteração, em vez de acender uma vez e nunca mais
mudar.

**Arquivos:** `lib/wifi_manager/wifi_manager.{h,cpp}`, `src/main.cpp`,
`include/app_config.h` (nova constante `WIFI_RECONNECT_RETRY_MS`).

### 2.2 — Eliminação de delay bloqueante no `loop()`

**Problema:** `checkGpioRuntime()` (verifica se o botão GPIO0 foi segurado por
5s para resetar a configuração de fábrica) usava um `while` com `delay(50)`
que travava o `loop()` inteiro por até 6 segundos sempre que o botão estava
pressionado. Durante esse período, MQTT, leitura de sensores e a reconexão de
Wi-Fi paravam de rodar.

**Solução:** reescrito como máquina de estados baseada em `millis()`, usando
duas variáveis `static` (`holding`, `holdStart`) que persistem entre chamadas
do `loop()`. A cada volta, só faz um `digitalRead()` e uma comparação de
tempo — nunca bloqueia. O comportamento externo (segurar 5s = reset de
fábrica) é idêntico ao anterior.

**Arquivo:** `src/main.cpp` (`checkGpioRuntime()`).

### 2.3 — Bug na detecção de alarme novo/resolvido (encontrado durante os testes)

**Como foi descoberto:** ao escrever os testes automatizados do
`alarm_manager` (ver Parte 3), dois casos de teste falharam de forma
consistente, revelando um bug pré-existente no código original (não
introduzido por nenhuma das mudanças estruturais).

**O bug:** `AlarmManager::check()` sobrescrevia `m_previous` com o estado
recém-calculado na própria chamada, **antes** de `getNewAlarms()` e
`getResolvedAlarms()` (chamados logo em seguida, no `main.cpp`) terem chance
de comparar contra o estado anterior de verdade. Na prática, a comparação
sempre encontrava "estado atual == estado anterior", porque os dois já eram o
mesmo objeto no momento da comparação.

**Impacto real em produção (antes da correção):** as mensagens MQTT do tipo
`"ALARM"` (alarme novo) e `"RESOLVED"` (alarme resolvido) nunca eram
publicadas de fato. Só o `"ALARM_REFRESH"` (reenvio periódico, a cada
`alarm_interval_s`, default 60s) continuava funcionando, porque não depende
dessa comparação. Ou seja: um alarme só era percebido no MQTT até 60s depois
de já ter começado, nunca em tempo real.

**Correção:** adicionado um novo membro interno `m_lastComputed` em
`AlarmManager`. Agora `m_previous` é atualizado a partir de `m_lastComputed`
no **início** de cada `check()` (antes de recalcular o ciclo atual), e
`m_lastComputed` só é atualizado com o resultado novo no **final** — depois
que o ciclo anterior já foi comparado. Isso também tornou a classe mais
robusta: ela deixou de depender do parâmetro de saída do chamador já vir
zerado/preenchido de uma chamada anterior.

**Validado:**
- 13 casos de teste automatizado (`pio test -e native`) — ver Parte 3.
- Teste em bancada real: hardware confirmado publicando `"ALARM"` e
  `"RESOLVED"` em tempo real no tópico `.../alarm`, alternando corretamente
  conforme a condição física mudava.

**Arquivos:** `lib/alarm_manager/alarm_manager.{h,cpp}`.

---

## Parte 3 — Testabilidade (ambiente de testes nativo)

### Objetivo

`alarm_manager` opera só sobre dados em memória (structs `PhaseData`/`Config`,
floats) — não toca hardware. Isso o torna candidato ideal para testes
automatizados que rodam no PC (sem precisar de placa conectada), usando o
framework Unity já empacotado com o PlatformIO.

### Obstáculo encontrado

`alarm_manager.h` incluía `config_manager.h` e `pzem_sensor.h` inteiros só para
pegar os structs `Config`/`PhaseData` — mas esses headers também trazem
`<Preferences.h>` e `<PZEM004Tv30.h>` (bibliotecas exclusivas do ESP32/Arduino),
o que impede compilação num ambiente `native`.

### Solução

Os structs `Config` e `PhaseData` foram extraídos para headers próprios,
sem nenhuma dependência de hardware:

- `include/config.h` — só o `struct Config` (mesmos campos, mesmos defaults).
- `include/phase_data.h` — só o `struct PhaseData` (idêntico).

Esses arquivos precisaram ficar em `include/` (não dentro de
`lib/config_manager/` ou `lib/pzem_sensor/`), porque o Library Dependency
Finder do PlatformIO resolve dependências por **pasta inteira** — se ficassem
dentro da pasta da lib "impura", ele tentaria compilar o `.cpp` inteiro
(incluindo o driver de hardware) junto.

`app_config.h` também teve `#include <Arduino.h>` trocado por
`#include <cstdint>` (só usa `uint8_t`/`unsigned long`, tipos padrão do C++,
não exclusividade do Arduino).

`config_manager.h` e `pzem_sensor.h` continuam existindo normalmente,
incluindo esses headers puros e mantendo as classes `ConfigManager`/
`PzemSensor` (que continuam dependentes de hardware, isso não muda).

### Ambiente de teste

Adicionado ao `platformio.ini`:

```ini
[platformio]
default_envs = esp32dev   ; garante que `pio run` sem -e só compila o firmware real

[env:native]
platform = native
build_flags =
    -std=c++17
    -Iinclude
test_framework = unity
```

### Suíte de testes

`test/test_alarm_manager/test_alarm_manager.cpp` — 13 casos, cobrindo cada
tipo de alarme e o ciclo de vida (novo/resolvido/ativo). Ver `ALARMES.md`
para o detalhe de cada regra testada.

### Como rodar

```bash
pio test -e native
```

Roda em menos de 1 segundo, sem placa conectada.

---

## Parte 4 — Itens discutidos e adiados (não implementados)

| Item | Status | Motivo |
|------|--------|--------|
| Senha no AP do captive portal (`ESP32_Config`) | Adiado | POC de uso interno; sem exposição externa por enquanto. Retomar se for para campo/instalação em ambiente não controlado. |
| Driver AHT10 duplicado (entre este firmware e o firmware irmão em ESP-IDF) | Pendente | Ainda não esclarecido — falta entender exatamente qual é a duplicação mencionada. |
| Extrair handlers HTTP (`/api/data`, `/api/config`, `/api/mqtt-config`) do `main.cpp` para um módulo próprio | Não solicitado | Não estava na árvore de módulos alvo original; `main.cpp` ainda tem ~200 linhas de lógica de dashboard/API, o que tecnicamente contraria a regra "só orquestração", mas foi conscientemente deixado de fora do escopo desta rodada. |
| Testes automatizados para `config_manager` | Não feito | Depende de `Preferences` (NVS do ESP32); precisaria de uma camada de abstração/mock antes de poder rodar nativamente. |

---

## Referência rápida — build e teste

```bash
# Compilar o firmware real (ESP32)
pio run

# Gravar na placa
pio run --target upload

# Rodar os testes automatizados (não precisa de placa)
pio test -e native

# Monitor serial
pio device monitor --baud 115200
```

## Estado final de RAM/Flash

| Momento | RAM | Flash |
|---------|-----|-------|
| Antes de qualquer mudança (baseline) | 47.920 bytes (14,6%) | 930.301 bytes (71,0%) |
| Depois de toda a reorganização estrutural | 47.920 bytes (14,6%) | 930.781 bytes (71,0%) |
| Depois das correções de robustez (Wi-Fi, GPIO, bug do alarme) | 48.088 bytes (14,7%) | 931.025 bytes (71,0%) |

A variação de Flash/RAM entre as duas últimas linhas é inteiramente explicada
pelas correções de comportamento (novas variáveis de estado) — não pela
reorganização de arquivos, que por si só não altera o binário gerado.
