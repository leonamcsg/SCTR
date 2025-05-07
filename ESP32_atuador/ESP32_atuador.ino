#include "WiFi.h"
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <AsyncUDP.h>
#include "esp_system.h"
#include "rom/ets_sys.h"
#include <ArduinoJson.h>

// Replace with your network credentials
const char* backup_wifi_ssid = "***********";
const char* backup_wifi_password = "***********";;

#define TEMPO_OTA 1000 // Intervalo para ArduinoOTA.handle()
#define TEMPO_PACOTE_LENTO 5000
#define TEMPO_PACOTE_RAPIDO 1000
const int wdtTimeout = 20000;  //time in ms to trigger the watchdog

hw_timer_t *timer = NULL;

String uri = "3306/1";

const int idAtuador = 5;

// Variável para contar pacotes enviados
int pacoteCount = 0;

void ARDUINO_ISR_ATTR watchdog_estoura(){
  digitalWrite(4, HIGH);
  digitalWrite(26, HIGH);
  digitalWrite(33, HIGH);
  esp_restart();
}

void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(backup_wifi_ssid, backup_wifi_password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void initOTA() {
  ArduinoOTA.setHostname("atuador_esp32");
  ArduinoOTA.onStart([]() { Serial.println("Start"); });
  ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

AsyncUDP udp;

unsigned long previousMillisUDP = 0; // Armazena o último tempo registrado para UDP
unsigned long previousMillisOTA = 0; // Armazena o último tempo registrado para OTA

unsigned long intervalo_pacote = TEMPO_PACOTE_LENTO; // Intervalo padrão de 5 segundos

// Função para montar a response no formato JSON
String montarResponse() {
  StaticJsonDocument<200> doc; // Cria um documento JSON com tamanho máximo de 200 bytes
  doc["URI"] = uri;
  doc["idAtuador"] = idAtuador;
  doc["numPct"] = pacoteCount;
  doc["state"] = digitalRead(4) == LOW; // Estado do relé (true para ligado, false para desligado)

  String response;
  serializeJson(doc, response); // Serializa o JSON para uma string
  return response;
}

void setup() {
  Serial.begin(115200);

  pinMode(33, OUTPUT);
  pinMode(26, OUTPUT);
  pinMode(4, OUTPUT);

  // Garante que iniciem em nível alto (relés desligados)
  digitalWrite(33, HIGH);
  digitalWrite(26, HIGH);
  digitalWrite(4, HIGH);

  initWiFi();
  initOTA();

  timer = timerBegin(1000000);                     //timer 1Mhz resolution
  timerAttachInterrupt(timer, &watchdog_estoura);       //attach callback
  timerAlarm(timer, wdtTimeout * 1000, false, 0);  //set time in us

  if (udp.listen(12345)) {
    Serial.print("WiFi connected. UDP Listening on IP: ");
    Serial.println(WiFi.localIP());
    udp.onPacket([](AsyncUDPPacket packet) {
      String myString = (const char*)packet.data();

      if (myString.startsWith("CALMA")) {
        uri = "3306/1";
        intervalo_pacote = TEMPO_PACOTE_LENTO;
      } else {
        // Tenta analisar a mensagem como JSON
        StaticJsonDocument<50> doc;
        DeserializationError error = deserializeJson(doc, myString);
        if (error) {
          Serial.println("Erro ao analisar JSON recebido.");
          return;
        }
        // Verifica o valor do campo "URI"
        const char* uriReceived = doc["URI"];
        if (strcmp(uriReceived,"200/20")==0) {
          // Reseta o timer ao receber o comando StillAlive
          timerWrite(timer, 0);  //reset timer (feed watchdog)
        } else if (strcmp(uriReceived,"100/10")==0) {
          if (idAtuador == doc["idAtuador"]) {
            int comando = doc["comando"];
            String response;
            switch (comando) {
            case 1:
              Serial.println("Turning on relay");
              digitalWrite(26, LOW);
              digitalWrite(33, LOW);
              digitalWrite(4, LOW);
              uri = "3306/2";
              response = montarResponse();
              udp.broadcastTo(response.c_str(), 12345);
              pacoteCount++;
              intervalo_pacote = TEMPO_PACOTE_RAPIDO;
              break;
            case 0:
              Serial.println("Turning off relay");
              digitalWrite(4, HIGH);
              digitalWrite(26, HIGH);
              digitalWrite(33, HIGH);
              uri = "3306/2";
              response = montarResponse();
              udp.broadcastTo(response.c_str(), 12345);
              pacoteCount++;
              intervalo_pacote = TEMPO_PACOTE_RAPIDO;
              break;
            default:
              break;
            }
          }
        } else if (strcmp(uriReceived,"100/11")==0) {
          if (idAtuador == doc["idAtuador"]) {
            const char* param = doc["parametro"];
            if (strcmp(param, "CALMA") == 0) {
              float valor = doc["valor"];
              if (valor > 0.0) {
                intervalo_pacote = TEMPO_PACOTE_LENTO;
                uri = "3306/1";
              }
            } else if (strcmp(param, "WATCHDOG") == 0) {
              float valor = doc["valor"];
              if (valor > 5000.0) {
                Serial.printf("Atualizando o tempo do timer watchdog para %.2f ms.\n", valor);
                timerAlarm(timer, valor * 1000, false, 0);  // Atualiza o tempo do alarme em microsegundos
                timerWrite(timer, 0);  // Reinicia o timer
              }
            }
          }
        }
      }
    });
  }
}

void loop() {
  unsigned long currentMillis = millis();
  // Verifica se o intervalo para ArduinoOTA.handle() foi atingido
  if (currentMillis - previousMillisOTA >= TEMPO_OTA) {
    previousMillisOTA = currentMillis; // Atualiza o último tempo registrado para OTA
    ArduinoOTA.handle();
  }

  // Verifica se o intervalo para envio de mensagens UDP foi atingido
  if (currentMillis - previousMillisUDP >= intervalo_pacote) {
    previousMillisUDP = currentMillis; // Atualiza o último tempo registrado para UDP
    String response = montarResponse();
    udp.broadcastTo(response.c_str(), 12345);
    pacoteCount++;
  }
}