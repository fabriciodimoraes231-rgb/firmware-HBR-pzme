# Referência de Alarmes — PZEM Multi-Fase (HBR-PZME)

Este documento explica, para cada tipo de alarme suportado pelo firmware: para
que serve, qual condição exata dispara ele, quais campos de configuração o
controlam, e como ele deve se comportar em diferentes cenários — incluindo
casos especiais que não são óbvios só de olhar o código.

Toda a lógica descrita aqui vive em `lib/alarm_manager/alarm_manager.cpp`, na
função `AlarmManager::check()`, e é validada automaticamente pelos testes em
`test/test_alarm_manager/test_alarm_manager.cpp` (rode com `pio test -e native`).

---

## Como o sistema de alarmes funciona, em linhas gerais

A cada ciclo do `loop()` (com o dispositivo conectado ao MQTT), o firmware:

1. Lê os sensores (PZEM de cada fase + AHT10).
2. Chama `AlarmManager::check()`, que avalia cada condição de alarme contra os
   limites configurados (`Config`) e monta o estado atual (`AlarmState`).
3. Compara esse estado atual contra o estado do ciclo anterior, publicando:
   - **`"ALARM"`** — quando uma condição que **não** estava ativa passa a estar.
   - **`"RESOLVED"`** — quando uma condição que estava ativa deixa de estar.
   - **`"ALARM_REFRESH"`** — reenvio periódico (a cada `alarm_interval_s`,
     default 60s) de tudo que **continua** ativo, mesmo sem mudança.

Cada mensagem vai para o tópico `{prefixo}/alarm`, no formato:

```json
{
  "type": "ALARM",
  "alarm": "OVERVOLTAGE",
  "phase": 1,
  "value": 250.0,
  "threshold": 242.0,
  "datetime": "18:47 09/07/2026"
}
```

(campos `value`/`threshold` são omitidos para `PHASE_LOSS`, que não tem um
valor numérico associado).

---

## Os 6 tipos de alarme

### 1. `OVERVOLTAGE` (sobretensão)

**Para que serve:** detecta quando a tensão de uma fase está acima do
esperado — risco de dano a equipamentos sensíveis, isolamento, etc.

**Condição de disparo:** `fase.v > v_max`

**Campo de configuração:** `v_max` (default: `242.0` V)

**Comportamento esperado:**
- Dispara por fase, independentemente — se as 3 fases estiverem acima do
  limite ao mesmo tempo, dispara 1 evento por fase (`phase: 1`, `phase: 2`,
  `phase: 3`).
- Só é avaliado se a leitura da fase for válida (`p.valid == true`) — se o
  PZEM não respondeu, o que dispara é `PHASE_LOSS`, não `OVERVOLTAGE`.
- Pode coexistir com `OVERCURRENT`/`UNDERCURRENT` na mesma fase e mesmo ciclo
  (são condições independentes, checadas separadamente).

**Cenário de teste:** baixe `v_max` via `/api/config` ou MQTT config para um
valor abaixo da tensão real da rede (ex.: rede em 220V, configure
`v_max: 200`). Deve aparecer `"ALARM"` quase instantaneamente. Suba o valor de
volta para acima da tensão real → deve aparecer `"RESOLVED"`.

---

### 2. `UNDERVOLTAGE` (subtensão)

**Para que serve:** detecta queda de tensão na fase — pode indicar problema
na rede elétrica, sobrecarga do circuito, ou mau contato.

**Condição de disparo:** `fase.v < v_min`

**Campo de configuração:** `v_min` (default: `198.0` V)

**Comportamento esperado:** mesmas regras de `OVERVOLTAGE` (por fase, só com
leitura válida, pode coexistir com alarmes de corrente).

**Atenção — relação com `PHASE_LOSS`:** `v_min` tem um segundo papel no
firmware: também controla se `PHASE_LOSS` é avaliado (ver abaixo). Não
configure `v_min = 0` só para "desativar" o alarme de subtensão — isso tem o
efeito colateral de desativar `PHASE_LOSS` também.

**Cenário de teste:** suba `v_min` acima da tensão real da rede. Deve disparar
`"ALARM"` quase na hora. Baixe de volta → `"RESOLVED"`.

---

### 3. `OVERCURRENT` (sobrecorrente)

**Para que serve:** detecta corrente acima do limite de segurança do
circuito/disjuntor — risco de sobreaquecimento de cabos, sobrecarga.

**Condição de disparo:** `fase.a > a_max`

**Campo de configuração:** `a_max` (default: `100.0` A)

**Comportamento esperado:** por fase, só com leitura válida. É o alarme mais
fácil de disparar em bancada sem precisar de carga real perigosa: basta
baixar `a_max` para um valor abaixo da corrente de qualquer carga pequena
já conectada (ex.: uma lâmpada).

**Cenário de teste:** com alguma carga ligada na fase monitorada, baixe
`a_max` para abaixo da corrente atual. `"ALARM"` deve aparecer. Suba de volta
→ `"RESOLVED"`.

---

### 4. `UNDERCURRENT` (subcorrente)

**Para que serve:** detecta quando a fase está energizada mas consumindo
menos corrente que o esperado — pode indicar uma carga que deveria estar
ligada mas não está (ex.: motor parado, equipamento desligado
inesperadamente).

**Condição de disparo:** `fase.a > 0 && fase.a < a_min`

**Campo de configuração:** `a_min` (default: `0.5` A)

**Caso especial importante:** a condição exige `a > 0`. Isso significa que
**corrente exatamente zero não dispara este alarme** — é tratado como "sem
carga conectada" (situação normal), não como falha. Se você quer detectar
"circuito sem nenhuma carga", este alarme **não** serve para isso; ele serve
para detectar uma carga que está *quase* desligada, mas não totalmente
(corrente residual baixa, tipicamente sintoma de problema).

**Cenário de teste:** com uma carga pequena ligada (corrente baixa, mas não
zero), configure `a_min` acima dessa corrente. Deve disparar `"ALARM"`.
Desligue a carga completamente (`a` vai a zero) — o alarme deve ser
**resolvido** (porque `a > 0` deixa de ser verdade), não permanecer ativo.

---

### 5. `PHASE_LOSS` (perda de fase)

**Para que serve:** detecta quando o PZEM daquela fase parou de responder —
sensor desconectado, cabo rompido, falta de alimentação no módulo, ou
problema de comunicação Modbus.

**Condição de disparo:** `!fase.valid && v_min > 0`

**Campo de configuração:** `v_min` — mas aqui funciona como uma **chave
liga/desliga** para este alarme especificamente, não como um limite numérico.
Se `v_min == 0`, este alarme fica **completamente desativado**,
independente de qualquer fase estar de fato desconectada.

**Por que essa dependência existe:** não está documentado no código, mas o
efeito prático é: em instalações de fase única onde `v_min` não faz sentido
configurar (ou foi deixado em 0 por engano), a perda de fase também para de
ser monitorada — o que pode ser surpreendente se não for intencional.

**Comportamento esperado quando ativo:**
- Dispara com `value: 0.0` e `threshold: <v_min configurado>` — mas o JSON
  publicado (`mqtt_manager.cpp`) omite esses dois campos especificamente para
  `PHASE_LOSS` (só manda `type`, `alarm`, `phase`, `datetime`).
- Quando uma fase está com `PHASE_LOSS` ativo, **nenhum outro alarme daquela
  fase é avaliado no mesmo ciclo** (o código usa `continue` assim que detecta
  `!p.valid` — overvoltage/undervoltage/overcurrent/undercurrent são pulados
  para essa fase até ela voltar a responder).

**Cenário de teste:** confirme primeiro que `v_min > 0` na configuração atual
(veja `/api/data` → `limits.v_min`, ou o log de boot no Serial Monitor).
Desconecte fisicamente o cabo de uma fase do PZEM (ou o CT dela) — `"ALARM"`
de `PHASE_LOSS` deve aparecer. Reconecte — `"RESOLVED"`.

---

### 6. `HIGH_TEMP` (temperatura alta)

**Para que serve:** monitora a temperatura ambiente do painel/gabinete via
sensor AHT10 — risco de superaquecimento do próprio equipamento.

**Condição de disparo:** `!isnan(temperatura) && temperatura > temp_max`

**Campo de configuração:** `temp_max` (default: `80.0` °C)

**Diferença importante em relação aos outros:** este é o único alarme que
**não é por fase** — é um único sensor compartilhado para o painel inteiro.
No JSON publicado, `phase` sempre vem como `0`.

**Caso especial:** se o sensor AHT10 falhar na leitura (retorna `NaN`), o
alarme **não dispara** — o firmware trata "não sei a temperatura" como
diferente de "temperatura alta". Isso significa que uma falha física do
sensor de temperatura é **silenciosa** do ponto de vista de alarmes (não gera
nenhum evento MQTT indicando que o sensor parou de funcionar). Vale ficar de
olho nisso separadamente (ex.: verificando se `temperature` vem `null` no
tópico `/data` por muito tempo).

**Cenário de teste:** aproxime uma fonte de calor controlada do AHT10 (fone
de calor, secador em distância segura) até a leitura passar de `temp_max`, ou
simplesmente baixe `temp_max` via config para abaixo da temperatura ambiente
atual. `"ALARM"` deve aparecer. Afaste a fonte de calor / suba `temp_max` de
volta → `"RESOLVED"`.

---

## Tabela-resumo

| Alarme | Condição | Config | Por fase? | Observação especial |
|--------|----------|--------|-----------|---------------------|
| `OVERVOLTAGE` | `v > v_max` | `v_max` | Sim | — |
| `UNDERVOLTAGE` | `v < v_min` | `v_min` | Sim | `v_min` também controla `PHASE_LOSS` |
| `OVERCURRENT` | `a > a_max` | `a_max` | Sim | — |
| `UNDERCURRENT` | `a > 0 && a < a_min` | `a_min` | Sim | `a == 0` NÃO dispara |
| `PHASE_LOSS` | `!valid && v_min > 0` | `v_min` (liga/desliga) | Sim | Desativado se `v_min == 0`; suprime outros alarmes da fase no mesmo ciclo |
| `HIGH_TEMP` | `!isnan(temp) && temp > temp_max` | `temp_max` | Não (`phase: 0`) | Falha do sensor (`NaN`) não gera alarme |

---

## Ciclo de vida: `ALARM` / `RESOLVED` / `ALARM_REFRESH`

| Tipo | Quando dispara | Frequência |
|------|-----------------|-----------|
| `ALARM` | No exato ciclo em que a condição passa de inativa para ativa | Uma vez, no momento da transição |
| `RESOLVED` | No exato ciclo em que a condição passa de ativa para inativa | Uma vez, no momento da transição |
| `ALARM_REFRESH` | A cada `alarm_interval_s` (default 60s), para tudo que **continua** ativo | Periódico, enquanto a condição não for resolvida |

**Nota histórica:** até a correção documentada em `HISTORICO_AJUSTES.md`
(seção 2.3), `ALARM` e `RESOLVED` nunca disparavam de fato devido a um bug na
comparação de estados — só o `ALARM_REFRESH` funcionava. Isso já foi
corrigido e validado (13 testes automatizados + confirmação em bancada com
hardware real). Se ao testar você só ver `ALARM_REFRESH` e nunca `ALARM`/
`RESOLVED`, é sinal de que a versão rodando na placa é anterior à correção —
regrave o firmware.

**Sobre oscilação:** se uma condição física estiver instável (ex.: fio com
mau contato, tensão oscilando bem em cima do limite configurado), é esperado
ver `ALARM`/`RESOLVED` alternando repetidamente em pouco tempo — isso é o
sistema funcionando corretamente, refletindo uma condição real instável, não
um bug do firmware.

---

## Limite de capacidade (nota técnica, baixo risco na prática)

`AlarmState` guarda no máximo `MAX_ALARMS = 12` eventos por ciclo. Com 3 fases
e até 2 alarmes simultâneos por fase na prática (tensão OU corrente
raramente disparam ambos ao mesmo tempo de forma anômala, mas é fisicamente
possível: overvoltage + overcurrent juntos), mais 1 de temperatura, o uso
real fica bem abaixo do limite. Se ele for atingido, alarmes além do 12º são
silenciosamente descartados naquele ciclo (sem erro ou log) — cenário
extremamente improvável em operação normal, mas vale saber que existe.
