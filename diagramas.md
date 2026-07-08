# Diagramas do Projeto IoT AC Energy Meter com PZEM-004T & ESP32

Fonte: https://how2electronics.com/iot-ac-energy-meter-with-pzem-004t-esp32-webserver/

---

## 1. PZEM-004T V3.0 — Módulo Medidor de Energia AC

**Arquivo:** `PZEM-004T-V3.0.jpg`
**URL:** https://how2electronics.com/wp-content/uploads/2025/02/PZEM-004T-V3.0.jpg

Foto do módulo PZEM-004T V3.0 mostrando o transformador de corrente (CT) encapsulado e os bornes de conexão AC (L e N) e os pinos de comunicação (5V, GND, TX, RX).

---

## 2. Open CT & Close CT — Tipos de Transformadores de Corrente

**Arquivo:** `PZEM-004T-Open-CT-Close-CT.jpg`
**URL:** https://how2electronics.com/wp-content/uploads/2025/02/PZEM-004T-Open-CT-Close-CT.jpg

Comparação entre os dois tipos de CT:
- **Close CT (CT Fechado):** CT integrado não removível, para instalações fixas.
- **Open CT (CT Aberto):** CT tipo garra destacável, para instalação sem desconectar fios.

Ambos suportam até 100A, Classe de precisão 1.5.

---

## 3. Pinout do PZEM-004T V3.0

**Arquivo:** `Pinout-of-PZEM-004T-V3.0.jpg`
**URL:** https://how2electronics.com/wp-content/uploads/2025/02/Pinout-of-PZEM-004T-V3.0.jpg

Diagrama de pinagem do módulo:

### Pinos de Alimentação e Medição:
| Pino | Função |
|------|--------|
| L (Live)   | Conectar à fase da rede AC |
| N (Neutral)| Conectar ao neutro da rede AC |
| CT+ / CT-  | Conectar ao transformador de corrente |

### Pinos de Comunicação (TTL Serial / RS485):
| Pino | Função |
|------|--------|
| 5V   | Alimentação externa 5V |
| GND  | Ground |
| TX   | Transmissão de dados para o microcontrolador |
| RX   | Recepção de dados do microcontrolador |

> **Nota:** A interface TTL é passiva — requer alimentação externa de 5V. Todos os 4 pinos (5V, RX, TX, GND) devem estar conectados.

---

## 4. Indicator Lights (Leds Indicadores) do PZEM-004T V3.0

**Arquivo:** `Indicator-Lights-of-PZEM-004T-V3.0.jpg`
**URL:** https://how2electronics.com/wp-content/uploads/2025/02/Indicator-Lights-of-PZEM-004T-V3.0.jpg

Diagrama mostrando a localização dos 4 LEDs no módulo:

| LED | Função |
|-----|--------|
| **Power** (Alimentação) | Indica que o módulo está ligado |
| **Pulse** (Pulso)   | Pisca conforme o consumo de energia ativa; quanto maior a carga, maior a frequência |
| **TX** (Transmissão) | Pisca quando o módulo transmite dados via RS485 |
| **RX** (Recepção)    | Pisca quando o módulo recebe dados do mestre (Arduino/ESP32) |

---

## 5. Conexão PZEM-004T & ESP32

**Arquivo:** `PZEM-004T-ESP32-Connection.jpg`
**URL:** https://how2electronics.com/wp-content/uploads/2025/03/PZEM-004T-ESP32-Connection.jpg

Diagrama de ligação entre os módulos:

| PZEM-004T | ESP32      |
|-----------|------------|
| 5V        | 5V         |
| GND       | GND        |
| TX        | RX2 (GPIO16) |
| RX        | TX2 (GPIO17) |

---

## 6. Interfacing PZEM-004T com ESP32 — Diagrama no Breadboard

**Arquivo:** `Interfacing-PZEM-004T-with-ESP32-Microcontroller.jpg`
**URL:** https://how2electronics.com/wp-content/uploads/2025/03/Interfacing-PZEM-004T-with-ESP32-Microcontroller.jpg

Foto do circuito montado em protoboard mostrando ESP32 e PZEM-004T conectados por jumpers. Confirma as conexões:
- PZEM TX → ESP32 GPIO16 (RX2)
- PZEM RX → ESP32 GPIO17 (TX2)
- PZEM 5V → ESP32 5V
- PZEM GND → ESP32 GND

---

## 7. Conexão do Transformador de Corrente (CT)

**Arquivo:** `Current-Transformer-Connection.jpg`
**URL:** https://how2electronics.com/wp-content/uploads/2025/02/Current-Transformer-Connection.jpg

Diagrama mostrando como conectar o CT ao circuito AC:
- **Open CT (split-core):** Encaixar o garfo ao redor do fio fase (L) — NÃO no neutro.
- **Close CT (solid-core):** Desconectar o circuito, passar o fio fase pelo núcleo do CT e reconectar.

> ⚠ **Cuidado:** Isolar a fiação AC (120V/220V) dos circuitos do Arduino por segurança.

---

## 8. Sistema Alimentado — PZEM-004T & ESP32 Ligados

**Arquivo:** `Energy-Meter-with-PZEM-004T-ESP32.jpg`
**URL:** https://how2electronics.com/wp-content/uploads/2025/03/Energy-Meter-with-PZEM-004T-ESP32.jpg

Foto do sistema montado em protoboard com alimentação ligada. LEDs do PZEM-004T acesos indicando operação normal.

---

## 9. Saída do Serial Monitor — Leitura dos Parâmetros

**Arquivo:** `Serial-Output-PZEM-004T-with-ESP32.jpg`
**URL:** https://how2electronics.com/wp-content/uploads/2025/03/Serial-Output-PZEM-004T-with-ESP32.jpg

Captura de tela do Serial Monitor da Arduino IDE (115200 baud) exibindo:
- Endereço customizado (Custom Address)
- Voltage (V)
- Current (A)
- Power (W)
- Energy (kWh)
- Frequency (Hz)
- Power Factor

Atualização a cada 2 segundos.

---

## 10. Serial Monitor — Endereço IP do WebServer

**Arquivo:** `ESP32-PZEM-004T-WebServer-IP-Address.jpg`
**URL:** https://how2electronics.com/wp-content/uploads/2025/03/ESP32-PZEM-004T-WebServer-IP-Address.jpg

Captura do Serial Monitor mostrando:
- Conexão WiFi bem-sucedida
- Endereço IP atribuído ao ESP32
- Dados dos sensores sendo impressos em tempo real

---

## 11. Dashboard Web — Dados do Energy Meter

**Arquivo:** `Webpage-showing-energy-meter-data.jpg`
**URL:** https://how2electronics.com/wp-content/uploads/2025/03/Webpage-showing-energy-meter-data.jpg

Captura de tela do dashboard web responsivo exibindo 6 cartões com ícones coloridos (Font Awesome):
- **VOLTAGE** (ícone raio amarelo) — ex: 127.5V
- **CURRENT** (ícone setas azul) — ex: 0.50A
- **POWER** (ícone plug vermelho) — ex: 63.8W
- **ENERGY** (ícone gráfico verde) — ex: 0.005kWh
- **FREQUENCY** (ícone onda roxo) — ex: 60.0Hz
- **POWER FACTOR** (ícone porcentagem laranja) — ex: 1.00

Atualização automática via AJAX a cada 2 segundos.

---

## 12. IoT AC Energy Meter — Visão Mobile

**Arquivo:** `IoT-AC-Dimmer-ESP32-PZEM-004T-Webserver.jpg`
**URL:** https://how2electronics.com/wp-content/uploads/2025/03/IoT-AC-Dimmer-ESP32-PZEM-004T-Webserver.jpg

Foto do dashboard sendo acessado de um dispositivo móvel (smartphone), provando a responsividade da página web.

---

## 13. Dashboard AJAX — Parâmetros em Tempo Real

**Arquivo:** `Energy-Meter-Parameters-on-AJAX-Based-Webpage.jpg`
**URL:** https://how2electronics.com/wp-content/uploads/2025/03/Energy-Meter-Parameters-on-AJAX-Based-Webpage.jpg

Close-up dos parâmetros elétricos sendo exibidos no dashboard web com atualização em tempo real sem refresh da página.

---

## Tabela de Registros Modbus-RTU do PZEM-004T V3.0

| Endereço | Descrição               | Resolução |
|----------|-------------------------|-----------|
| 0x0000   | Voltage (Tensão)        | 0.1V      |
| 0x0001-2 | Current (Corrente)      | 0.001A    |
| 0x0003-4 | Power (Potência)        | 0.1W      |
| 0x0005-6 | Energy (Energia)        | 1Wh       |
| 0x0007   | Frequency (Frequência)  | 0.1Hz     |
| 0x0008   | Power Factor            | 0.01      |
| 0x0009   | Alarm Status            | 0/0xFFFF  |

**Protocolo:** Modbus-RTU, 9600 baud, 8N1, half-duplex.
