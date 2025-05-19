/* ***********************************************************************************************************************************************************
SOIL QUALITY SENSOR: this file includes the main code for the soil quality sensor used in Daniel Rodriguez Moya's Master Thesis. It sends data to ThingsBoard
via MQTT at a fixed frequency, measuring soil temperature and moisture using a DS18B20 and a FC-38, respectively.
*********************************************************************************************************************************************************** */

// ===========================================================================================================================================================
// INLCUSION DE LIBRERIAS
// ===========================================================================================================================================================
#include <Arduino.h>                                                                                             // Libreria de Arduino para garantizar que se aplican funciones propias tambien en la ESP32 S3
// Wi-Fi and MQTT libs ---------------------------------------------------------------------------------------------------------------------------------------
#include <WiFi.h>                                                                                                // Liberia para crear un hotspot desde el que configurar la conexion WiFi
#include <WiFiClientSecure.h>                                                                                    // Liberia para añadir certificados de conexion segura
#include <PubSubClient.h>                                                                                        // Libreria para MQTT
// ArduinoOTA libs -------------------------------------------------------------------------------------------------------------------------------------------
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
// I2C libs --------------------------------------------------------------------------------------------------------------------------------------------------
#include <Wire.h>
#include <axp20x.h>                                                                                              // Library for the PMU AXP192
// ESP32 libs ------------------------------------------------------------------------------------------------------------------------------------------------
#include <esp_sleep.h>                                                                                           // Library to send the ESP32 to sleep
#include "rom/rtc.h"                                                                                             // Libreria para usar la memoria RTC del ESP32, donde se pueden guardar variables cuyos valores sobreviven al deep sleep
// LIBRERIAS END =============================================================================================================================================

// ===========================================================================================================================================================
// MACROS (de ser necesarias)
// ===========================================================================================================================================================
// T-Beam macros ---------------------------------------------------------------------------------------------------------------------------------------------
#define LED_PIN 4
#define BUTTON_PIN GPIO_NUM_38                                                                                   // RTC pin to interrupt deep sleep
#define SDA_PIN 21
#define SCL_PIN 22
#define PMU_IRQ_PIN 35                                                                                           // PEK (PWR) button interrupt pin on T-Beam
// MACROS END ================================================================================================================================================

// ===========================================================================================================================================================
// CONSTRUCTORES DE OBJETOS DE CLASE DE LIBRERIA, VARIABLES GLOBALES, CONSTANTES...
// ===========================================================================================================================================================
static WiFiClientSecure secureClient;                                                                            // Objeto de la libreria WiFiManager
static PubSubClient mqttClient(secureClient);                                                                    // Objeto de la libreria MQTT
static AXP20X_Class axp;
// CONSTRUCTORES END =========================================================================================================================================

// ===========================================================================================================================================================
// GLOBAL VARIABLES
// ===========================================================================================================================================================
// Constants -------------------------------------------------------------------------------------------------------------------------------------------------
// const char* ssid = "DIGIFIBRA-24-h3JA";
// const char* password = "edaXG9qJA6";
static const char* ssid = "Pixel_OF13";
static const char* password = "mynameisjeff";
static const char* mqtt_server = "srv-iot.diatel.upm.es";                                                        // Broker MQTT de la UPM
static const int mqtt_port = 8883;                                                                               // Puerto del Broker MQTT
static const char* mqttTopicPub = "v1/devices/me/telemetry";
static const char* mqttTopicSub = "v1/devices/me/attributes";
static const char* access_token = "c0ar6qni65ev6515q845";                                                        // Token unico del dispositivo en Thingsboard

static const char* root_ca = R"EOF(-----BEGIN CERTIFICATE-----
MIIF3jCCA8agAwIBAgIQAf1tMPyjylGoG7xkDjUDLTANBgkqhkiG9w0BAQwFADCB
iDELMAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0pl
cnNleSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNV
BAMTJVVTRVJUcnVzdCBSU0EgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAw
MjAxMDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNV
BAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVU
aGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBSU0EgQ2Vy
dGlmaWNhdGlvbiBBdXRob3JpdHkwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIK
AoICAQCAEmUXNg7D2wiz0KxXDXbtzSfTTK1Qg2HiqiBNCS1kCdzOiZ/MPans9s/B
3PHTsdZ7NygRK0faOca8Ohm0X6a9fZ2jY0K2dvKpOyuR+OJv0OwWIJAJPuLodMkY
tJHUYmTbf6MG8YgYapAiPLz+E/CHFHv25B+O1ORRxhFnRghRy4YUVD+8M/5+bJz/
Fp0YvVGONaanZshyZ9shZrHUm3gDwFA66Mzw3LyeTP6vBZY1H1dat//O+T23LLb2
VN3I5xI6Ta5MirdcmrS3ID3KfyI0rn47aGYBROcBTkZTmzNg95S+UzeQc0PzMsNT
79uq/nROacdrjGCT3sTHDN/hMq7MkztReJVni+49Vv4M0GkPGw/zJSZrM233bkf6
c0Plfg6lZrEpfDKEY1WJxA3Bk1QwGROs0303p+tdOmw1XNtB1xLaqUkL39iAigmT
Yo61Zs8liM2EuLE/pDkP2QKe6xJMlXzzawWpXhaDzLhn4ugTncxbgtNMs+1b/97l
c6wjOy0AvzVVdAlJ2ElYGn+SNuZRkg7zJn0cTRe8yexDJtC/QV9AqURE9JnnV4ee
UB9XVKg+/XRjL7FQZQnmWEIuQxpMtPAlR1n6BB6T1CZGSlCBst6+eLf8ZxXhyVeE
Hg9j1uliutZfVS7qXMYoCAQlObgOK6nyTJccBz8NUvXt7y+CDwIDAQABo0IwQDAd
BgNVHQ4EFgQUU3m/WqorSs9UgOHYm8Cd8rIDZsswDgYDVR0PAQH/BAQDAgEGMA8G
A1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEMBQADggIBAFzUfA3P9wF9QZllDHPF
Up/L+M+ZBn8b2kMVn54CVVeWFPFSPCeHlCjtHzoBN6J2/FNQwISbxmtOuowhT6KO
VWKR82kV2LyI48SqC/3vqOlLVSoGIG1VeCkZ7l8wXEskEVX/JJpuXior7gtNn3/3
ATiUFJVDBwn7YKnuHKsSjKCaXqeYalltiz8I+8jRRa8YFWSQEg9zKC7F4iRO/Fjs
8PRF/iKz6y+O0tlFYQXBl2+odnKPi4w2r78NBc5xjeambx9spnFixdjQg3IM8WcR
iQycE0xyNN+81XHfqnHd4blsjDwSXWXavVcStkNr/+XeTWYRUc+ZruwXtuhxkYze
Sf7dNXGiFSeUHM9h4ya7b6NnJSFd5t0dCy5oGzuCr+yDZ4XUmFF0sbmZgIn/f3gZ
XHlKYC6SQK5MNyosycdiyA5d9zZbyuAlJQG03RoHnHcAP9Dc1ew91Pq7P8yF1m9/
qS3fuQL39ZeatTXaw2ewh0qpKJ4jjv9cJ2vhsE/zB+4ALtRZh8tSQZXq9EfX7mRB
VXyNWQKV3WKdwrnuWih0hKWbt5DHDAff9Yk2dDLWKMGwsAvgnEzDHNb842m1R0aB
L6KCq9NjRHDEjf8tM7qtj3u1cIiuPhnPQCjY/MiQu12ZIvVS5ljFH4gxQ+6IHdfG
jjxDah2nGN59PRbxYvnKkKj9
-----END CERTIFICATE-----)EOF";                                                                                  // Certificado para el cifrado TLS de MQTT en Thingsboard

static const uint64_t SLEEP_DURATION_US = 30ULL * 1000000;                                                       // Sleep time between messages

// Variables -------------------------------------------------------------------------------------------------------------------------------------------------
static bool ledState = LOW;
static RTC_DATA_ATTR uint32_t bootCount = 0;
// GLOBAL VARIABLES END ======================================================================================================================================

// ===========================================================================================================================================================
// FUNCTION PROTOTYPES
// ===========================================================================================================================================================
static void connectToWiFi();
static void setupOTA();
static void reconnectToMQTT();
static void checkPEKPress();
// FUNCTION PROTOTYPES END ===================================================================================================================================

// ===========================================================================================================================================================
// SETUP FUNCTION
// ===========================================================================================================================================================
void setup() {
  Serial.begin(115200);
  Serial.println(F("Soil Quality Sensor Alpha"));

  // AXP192 setup --------------------------------------------------------------------------------------------------------------------------------------------
  Wire.begin(SDA_PIN, SCL_PIN);                                                                                  // Initialize I2C bus
  
  if(axp.begin(Wire, AXP192_SLAVE_ADDRESS) != 0){                                                                // "AXP192_SLAVE_ADDRESS" should be "0x34"
    Serial.println("AXP192 not detected!");
    while(1);
  }else{
    Serial.println("AXP192 detected.");
  }

  axp.adc1Enable(AXP202_BATT_VOL_ADC1, true);                                                                    // Enable ADC for battery voltage

  pinMode(PMU_IRQ_PIN, INPUT);                                                                                   // Set up PEK button IRQ pin

  axp.clearIRQ();                                                                                                // Clear any existing IRQs
  axp.enableIRQ(AXP202_PEK_SHORTPRESS_IRQ | AXP202_PEK_LONGPRESS_IRQ, true);                                     // Enable PEK IRQs
  // AXP192 setup END ----------------------------------------------------------------------------------------------------------------------------------------
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, ledState);

  esp_sleep_enable_ext0_wakeup(BUTTON_PIN, 0);

  connectToWiFi();
  setupOTA();

  secureClient.setCACert(root_ca);                                                                               // Inicializacion de la conexion cifrada

  mqttClient.setServer(mqtt_server, mqtt_port);                                                                  // Funcion de la libreria PubSubClient para establecer el servidor MQTT y su puerto
}
// SETUP FUNCTION END ========================================================================================================================================

// ===========================================================================================================================================================
// LOOP FUNCTION
// ===========================================================================================================================================================
void loop() {
  ArduinoOTA.handle();                                                                                           // If a new version is available, download and install it

  if(!mqttClient.connected()){                                                                                   // Si no hay conexion
    reconnectToMQTT();                                                                                           // Entra la funcion de reconexion
  }
  mqttClient.loop();                                                                                             // Funcion principal, tiene que estar corriendo a la mayor frecuencia posible y nunca ser interrumpida

  checkPEKPress();

  // MQTT PUBLISH --------------------------------------------------------------------------------------------------------------------------------------------
  if(WiFi.status() != WL_CONNECTED){
    connectToWiFi();
  }else{                                                                                                         // Check WiFi connection status
    char dataStr[256];                                                                                           // Se crea un string de caracteres para guardar ambas medidas, se reservan 60 espacios
    float soilTemp = random(1000, 4500) / 100.0f;
    float soilMoist = random(0, 10000) / 100.0f;
    float batVolt = (axp.getBattVoltage()) / 1000.0f;

    sprintf(dataStr, "{\"bootCnt\":%lu,\"soilTemperature\":%4.2f,\"soilMoisture\":%5.2f,\"batVoltage\":%4.3f}",
            (unsigned long)bootCount, soilTemp, soilMoist, batVolt);                                             // La funcion 'sprintf' de C++ se usa para poder introducir las medidas y elegir el ancho de numero y su precision. Las medidas se separan con una coma ','
    
    if(mqttClient.publish(mqttTopicPub, dataStr)){                                                               // Se publica el string con los datos de los sensores en el topico 'moya/sensores'
      Serial.println(dataStr);                                                                                   // Muestra en el serial de Arduino el string
      Serial.println(F("Going to sleep until next TX..."));
      bootCount++;

      esp_sleep_enable_timer_wakeup(SLEEP_DURATION_US);                                                          // Temporizar el deep sleep durante el tiempo establecido
      esp_deep_sleep_start();
    }else{
      Serial.println(F("Failed to publish data"));
    }
    // MQTT PUBLISH END --------------------------------------------------------------------------------------------------------------------------------------
  }

  delay(100);
}
// LOOP FUNCTION END =========================================================================================================================================

// ===========================================================================================================================================================
// AUXILIARY FUNCTIONS
// ===========================================================================================================================================================
// RECONNECT TO MQTT -----------------------------------------------------------------------------------------------------------------------------------------
static void reconnectToMQTT() {
  while(!mqttClient.connected()){                                                                                // Loop until we're reconnected
    checkPEKPress();
    Serial.print(F("Attempting MQTT connection..."));

    if(mqttClient.connect("soil_quaity_sensor", access_token, NULL)){                                            // Attempt to connect
      Serial.println(F("connected"));
    }else{
      Serial.print(F("failed, rc="));
      Serial.print(mqttClient.state());
      Serial.println(F(" try again in 5 seconds"));

      delay(5000);                                                                                               // Wait 5 seconds before retrying
    }
  }
}
// RECONNECT TO MQTT END -------------------------------------------------------------------------------------------------------------------------------------

// CONNECT TO WIFI -------------------------------------------------------------------------------------------------------------------------------------------
static void connectToWiFi(){
  Serial.print("Connecting to WIFI SSID ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);

    checkPEKPress();
  }

  Serial.println("");

  Serial.print(F("WiFi connected, IP address: "));
  Serial.println(WiFi.localIP());

  if(ledState){
    digitalWrite(LED_PIN, LOW);
  }
}
// CONNECT TO WIFI END ---------------------------------------------------------------------------------------------------------------------------------------

// CHECK PEK PRESS -------------------------------------------------------------------------------------------------------------------------------------------
static void checkPEKPress(){
  if(digitalRead(PMU_IRQ_PIN) == LOW){                                                                           // Check for PEK button press
    axp.readIRQ();

    if(axp.isPEKLongtPressIRQ()){
      Serial.println("\t\t\tLong press detected: Shutting down...");
      axp.shutdown();
    }

    axp.clearIRQ();
  }
}
// CHECK PEK PRESS END ---------------------------------------------------------------------------------------------------------------------------------------

// SETUP OTA -------------------------------------------------------------------------------------------------------------------------------------------------
static void setupOTA(){
  ArduinoOTA.setHostname("esp32-ota-test");                                                                      // Set custom OTA hostname
  ArduinoOTA.setPassword("pw0123");                                                                              // No authentication by default
  
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else                                                                                                       // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  Serial.println("OTA service started!");
}
// SETUP OTA END ---------------------------------------------------------------------------------------------------------------------------------------------
// AUXILIARY FUNCTIONS END ===================================================================================================================================