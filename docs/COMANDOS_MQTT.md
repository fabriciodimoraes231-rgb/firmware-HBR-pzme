# Comandos MQTT - Vigiatech Monitor Trifasico v2.0

**Prefixo padrao:** `vigiatech/setor/painel/CT`

---

## 1. Configurar tudo (envio completo)

```bash
mosquitto_pub -h 10.170.50.29 -t "vigiatech/setor/painel/CT/config" -m '{
  "wifi_ssid": "MinhaRede",
  "wifi_pass": "senha",
  "mqtt_host": "10.170.50.29",
  "mqtt_port": 1883,
  "mqtt_user": "usuario",
  "mqtt_pass": "senha",
  "mqtt_topic_prefix": "vigiatech/setor/painel/CT",
  "nominal_voltage": 220,
  "send_interval_s": 5,
  "alarm_interval_s": 60,
  "v_max": 242,
  "v_min": 198,
  "a_max": 100,
  "a_min": 0.5,
  "temp_max": 80,
  "v_factor": [1.0, 1.0, 1.0],
  "a_factor": [1.0, 1.0, 1.0],
  "w_factor": [1.0, 1.0, 1.0]
}'
```

---

## 2. Alterar apenas limites de alarme

```bash
mosquitto_pub -h 10.170.50.29 -t "vigiatech/setor/painel/CT/config" -m '{
  "v_max": 250,
  "v_min": 190,
  "a_max": 80,
  "a_min": 0.3,
  "temp_max": 70
}'
```

---

## 3. Alterar prefixo do topico

```bash
mosquitto_pub -h 10.170.50.29 -t "vigiatech/setor/painel/CT/config" -m '{
  "mqtt_topic_prefix": "vigiatech/novo/prefixo"
}'
```

**Exemplo real** (se o ESP esta em `vigiatech/eletrical-room/energy-painel-04/esp01`):

```bash
mosquitto_pub -h 10.170.50.29 -t "vigiatech/eletrical-room/energy-painel-04/esp01/config" -m '{
  "mqtt_topic_prefix": "vigiatech/setor/painel/CT"
}'
```

> **IMPORTANTE:** Envie o comando usando o topico que esta configurado **no momento**.
> O ESP so escuta em `<topico_atual>/config`.
> Apos reiniciar, ele passa a publicar no **novo** prefixo.

---

## 4. Alterar intervalo de envio

```bash
mosquitto_pub -h 10.170.50.29 -t "vigiatech/setor/painel/CT/config" -m '{
  "send_interval_s": 10,
  "alarm_interval_s": 120
}'
```

---

## 5. Calibracao (fator de correcao por fase)

```bash
mosquitto_pub -h 10.170.50.29 -t "vigiatech/setor/painel/CT/config" -m '{
  "v_factor": [0.9906, 1.0, 1.0],
  "a_factor": [1.0, 1.0, 1.0],
  "w_factor": [1.0, 1.0, 1.0]
}'
```

> **Calculo do fator:** `fator = leitura_referencia / leitura_pzem`
> Ex: multimetro marca 126.3V, PZEM le 127.5V -> fator = 126.3/127.5 = **0.9906**

---

## 6. Sincronizar relogio (epoch)

```bash
mosquitto_pub -h 10.170.50.29 -t "vigiatech/setor/painel/CT/time" -m '{
  "epoch": 1750698000
}'
```

Ou via texto:
```bash
mosquitto_pub -h 10.170.50.29 -t "vigiatech/setor/painel/CT/time" -m '"09:28 23/06/2026"'
```

---

## 7. Topico de dados (ESP -> Broker)

Leituras publicadas a cada `send_interval_s`:

```
vigiatech/setor/painel/CT/data
```

```json
{
  "datetime": "09:28 23/06/2026",
  "v_nominal": 220.0,
  "F1": { "v": 220.5, "a": 10.25, "w": 2250.0, "kwh": 1234.56, "hz": 60.0, "pf": 0.95, "valid": true },
  "F2": { "v": 220.1, "a": 11.2, "w": 2400.0, "kwh": 1500.0, "hz": 60.0, "pf": 0.93, "valid": true },
  "F3": { "valid": false }
}
```

---

## 8. Topico de alarmes (ESP -> Broker)

```
vigiatech/setor/painel/CT/alarm
```

Tipos: `ALARM`, `ALARM_REFRESH`, `RESOLVED`

```json
{
  "type": "ALARM",
  "alarm": "OVERVOLTAGE",
  "phase": 1,
  "value": 245.0,
  "threshold": 242.0,
  "datetime": "09:28 23/06/2026",
  "condition_active": true
}
```

`condition_active` indica se a condicao fisica ainda esta fora do limite
neste momento. `RESOLVED` **so** e publicado apos um ack (ver secao 8.1) -
a condicao voltar ao normal sozinha nao fecha o alarme, so muda
`condition_active` para `false` nos proximos `ALARM_REFRESH`. Detalhes do
ciclo de vida completo em `docs/ALARMES.md`.

---

## 8.1. Topico de ack de alarme (Backend/Frontend -> ESP)

```
vigiatech/setor/painel/CT/alarm/ack
```

Fecha um alarme aberto (por tipo + fase):

```bash
mosquitto_pub -h 10.170.50.29 -t "vigiatech/setor/painel/CT/alarm/ack" -m '{
  "alarm": "OVERVOLTAGE",
  "phase": 1
}'
```

`phase` pode ser omitido para `HIGH_TEMP` (que nao e por fase, `phase: 0`).

Fecha todos os alarmes abertos de uma vez:

```bash
mosquitto_pub -h 10.170.50.29 -t "vigiatech/setor/painel/CT/alarm/ack" -m '{
  "all": true
}'
```

O ESP responde publicando `"RESOLVED"` no topico `.../alarm` para cada
alarme fechado, imediatamente apos processar o ack. Payload invalido ou
`alarm` com nome desconhecido e ignorado silenciosamente (mesmo padrao do
topico `/config`).

---

## 9. Topico de status (ESP -> Broker, QoS 1, Retain, LWT)

```
vigiatech/setor/painel/CT/status
```

Online:
```json
{ "status": "online", "datetime": "09:21 22/07/2026", "ip": "192.168.1.100" }
```

`datetime` reflete o momento da conexao/reconexao ao broker (nao e
atualizado periodicamente) - util para distinguir um status recente de um
retido antigo, já que o topico é publicado com `retain=true`.

Offline (LWT automatico):
```json
{ "status": "offline" }
```

---

## 10. Reset completo (via botao fisico)

Segure o **GPIO0 (BOOT)** por **>5 segundos** durante operacao normal.
- Limpa todas as configuracoes da NVS
- Reinicia em modo AP (`ESP32_Config`)
- Acesse `http://192.168.4.1` para reconfigurar do zero

---

## 11. Subscrever para monitorar

```bash
mosquitto_sub -h 10.170.50.29 -t "vigiatech/setor/painel/CT/#" -v
```

---

## 12. Troubleshooting MQTT

### Erros comuns com mosquitto_pub/sub

| Erro | Causa | Solucao |
|------|-------|---------|
| `Connection refused` | Broker offline ou porta errada | Verificar se o broker esta rodando na porta 1883 |
| `Error: Address already in use` | Outro subscriber na mesma conexao | Usar `-i` para definir um client ID unico |
| `Payload too large` | JSON maior que 1024 bytes | Reduzir campos ou usar MQTT v5 com payload maior |
| `Invalid topic` | Topic com espacos ou caracteres especiais | Usar apenas `/`, letras, numeros e `_` |
| `Not connected` | Tempo limite de conexao expirado | Verificar firewall e rede |

### Como verificar conectividade

```bash
# Testar se o broker responde
mosquitto_pub -h 10.170.50.29 -t "test/ping" -m "hello" -d

# Verificar se o dispositivo esta online
mosquitto_sub -h 10.170.50.29 -t "vigiatech/setor/painel/CT/status" -v -C 1

# Verificar se o topico existe
mosquitto_sub -h 10.170.50.29 -t "vigiatech/setor/painel/CT/#" -v -W 5
```

### Debug de payload

```bash
# Enviar JSON formatado (com -d para debug)
mosquitto_pub -h 10.170.50.29 -t "vigiatech/setor/painel/CT/config" -m '{"send_interval_s": 10}' -d

# Verificar se o JSON e valido
echo '{"send_interval_s": 10}' | python3 -m json.tool
```

### Erros de interpretacao do JSON

| Payload enviado | Resultado esperado | Erro comum |
|----------------|-------------------|------------|
| `{"send_interval_s": 5}` | Intervalo alterado para 5s | Usar aspas: `"send_interval_s"` |
| `{"v_max": 250, "v_min": 190}` | Ambos limites alterados | Nao esquecer virgula entre campos |
| `{"mqtt_topic_prefix": "novo/topico"}` | Prefixo alterado | Usar `mqtt_topic_prefix` (nao `topic_prefix`) |
| `{"v_factor": [0.99, 1.0, 1.0]}` | Fatores atualizados | Array deve ter 3 valores |

### Verificar status do dispositivo

```bash
# Topico de status (QoS 1, Retain)
mosquitto_sub -h 10.170.50.29 -t "vigiatech/setor/painel/CT/status" -v

# Topico de dados (leituras em tempo real)
mosquitto_sub -h 10.170.50.29 -t "vigiatech/setor/painel/CT/data" -v

# Topico de alarmes
mosquitto_sub -h 10.170.50.29 -t "vigiatech/setor/painel/CT/alarm" -v
```

### Resetar dispositivo via MQTT

Para forcar um reset remoto, envie um payload invalido no topico `/config`:

```bash
mosquitto_pub -h 10.170.50.29 -t "vigiatech/setor/painel/CT/config" -m 'invalid'
```

O dispositivo descarta o payload e continua operando. Para resetar de verdade, use o botao GPIO0 (5s).
