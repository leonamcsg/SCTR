
# ESP32 Atuador com Comunicação UDP, OTA e Watchdog

Este projeto implementa o controle de um **atuador ESP32** via UDP com suporte a **OTA (Over-the-Air updates)**, **resposta JSON**, e proteção contra travamentos utilizando **watchdog por hardware**. Ele se comunica com um servidor mestre que envia comandos no formato JSON.

## 🛠️ Funcionalidades

- Conexão automática ao Wi-Fi.
- Recebimento de pacotes UDP com comandos.
- Controle de relés nos pinos **4**, **26**, e **33**.
- Envio periódico de mensagens JSON via UDP com o estado atual do atuador.
- Suporte a OTA para atualização remota do firmware.
- Watchdog por timer de hardware, com reinicialização automática em caso de falhas.
- Suporte a dois modos de envio: rápido e lento.

---

## 📦 Estrutura da Comunicação

### ✅ Pacotes Recebidos

O ESP32 ouve na porta UDP **12345** e espera pacotes no formato JSON.

#### **1. Comando Keep-Alive**
```json
{
  "URI": "200/20"
}
```
Reinicia o watchdog para indicar que o ESP ainda está sendo monitorado.

#### **2. Comando para ligar/desligar o relé**
```json
{
  "URI": "100/10",
  "idAtuador": 5,
  "comando": 1
}
```

- `"comando": 1` → Liga o relé.
- `"comando": 0` → Desliga o relé.

#### **3. Comando para ajustar parâmetros**
```json
{
  "URI": "100/11",
  "idAtuador": 5,
  "parametro": "CALMA",
  "valor": 1.0
}
```

Parâmetros aceitos:
- `"CALMA"`: alterna o modo de envio para lento (5s).
- `"WATCHDOG"`: ajusta o tempo do watchdog (em milissegundos).

---

### 📤 Resposta Enviada

A cada intervalo (`1s` ou `5s`), o ESP envia um pacote UDP com as informações:

```json
{
  "URI": "3306/1",
  "idAtuador": 5,
  "numPct": 42,
  "state": true
}
```

- `"state": true` → relé ligado (nível LOW).
- `"state": false` → relé desligado (nível HIGH).

---

## ⚙️ Configurações

Edite o arquivo `.ino` com suas credenciais Wi-Fi:

```cpp
const char* backup_wifi_ssid = "SUA_REDE";
const char* backup_wifi_password = "SUA_SENHA";
```

---

## 🔌 Pinos Utilizados

| Função   | GPIO |
|----------|------|
| Relé 1   | 4    |
| Relé 2   | 26   |
| Relé 3   | 33   |

> Todos os relés iniciam desligados (nível HIGH).

---

## 🔄 OTA - Atualização via Rede

- O ESP anuncia a atualização OTA com o hostname `atuador_esp32`.
- Você pode atualizar via Arduino IDE (menu: Ferramentas > Porta > Porta OTA).
- Recomendado configurar uma senha com `ArduinoOTA.setPassword()`.

---

## ⚠️ Watchdog Timer

- O watchdog reinicia o ESP caso o comando `"200/20"` não seja recebido dentro do intervalo configurado (padrão: 20 segundos).
- Pode ser ajustado com:

```json
{
  "URI": "100/11",
  "idAtuador": 5,
  "parametro": "WATCHDOG",
  "valor": 30000
}
```

---

## 📚 Bibliotecas Necessárias

Certifique-se de instalar as seguintes bibliotecas no Arduino IDE:

- `ArduinoJson`
- `ArduinoOTA`
- `AsyncUDP` (incluso com o ESP32)

---

## 🚀 Compilação

- Placa: `ESP32 Dev Module`
- Velocidade: `115200 baud`

---

## 🧪 Testado em:

- ESP32 DevKit V1
- Relé 5V com lógica inversa
- Monitoramento UDP com script Python ou outro ESP

---

## 📄 Licença

Este projeto é de uso livre para fins educacionais e comerciais, com devida atribuição ao autor original.

---

**vlw tamo junto!**
