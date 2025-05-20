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
// Serial Monitor macros -------------------------------------------------------------------------------------------------------------------------------------
#define ENABLE_SERIAL true

#if ENABLE_SERIAL
  #define Debug(x)    Serial.print(x)
  #define Debugln(x)  Serial.println(x)
  #define Debugf(...) Serial.printf(__VA_ARGS__)
#else
  #define Debug(x)
  #define Debugln(x)
  #define Debugf(...)
#endif
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
static const char* ssid = "";
static const char* password = "";
static const char* mqtt_server = "srv-iot.diatel.upm.es";                                                        // Broker MQTT de la UPM
static const int mqtt_port = 8883;                                                                               // Puerto del Broker MQTT
static const char* mqttTopicPub = "v1/devices/me/telemetry";
static const char* mqttTopicSub = "v1/devices/me/attributes";
static const char* access_token = "";                                                                            // Token unico del dispositivo en Thingsboard

static const char* root_ca = R"EOF(-----BEGIN CERTIFICATE-----
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
  #if ENABLE_SERIAL
    Serial.begin(115200);
  #endif

  Debugln(F("Soil Quality Sensor Alpha"));

  // AXP192 setup --------------------------------------------------------------------------------------------------------------------------------------------
  Wire.begin(SDA_PIN, SCL_PIN);                                                                                  // Initialize I2C bus
  
  if(axp.begin(Wire, AXP192_SLAVE_ADDRESS) != 0){                                                                // "AXP192_SLAVE_ADDRESS" should be "0x34"
    Debugln("AXP192 not detected!");
    while(1);
  }else{
    Debugln("AXP192 detected.");
  }

  axp.setPowerOutPut(AXP192_LDO2, AXP202_OFF);                                                                   // Turn off LoRa
  axp.setPowerOutPut(AXP192_LDO3, AXP202_OFF);                                                                   // Disable GPS power
  Debugln(F("GPS and LoRa powered off"));

  axp.adc1Enable(AXP202_BATT_VOL_ADC1, true);                                                                    // Enable ADC for battery voltage

  pinMode(PMU_IRQ_PIN, INPUT);                                                                                   // Set up PEK button IRQ pin

  axp.clearIRQ();                                                                                                // Clear any existing IRQs
  axp.enableIRQ(AXP202_PEK_SHORTPRESS_IRQ | AXP202_PEK_LONGPRESS_IRQ, true);                                     // Enable PEK IRQs
  // AXP192 setup END ----------------------------------------------------------------------------------------------------------------------------------------
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, ledState);

  esp_sleep_enable_ext0_wakeup(BUTTON_PIN, 0);                                                                   // Enable deep sleep interrupt using builtin button

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
      Debugln(dataStr);                                                                                          // Muestra en el serial de Arduino el string
      Debugln(F("Going to sleep until next TX..."));
      bootCount++;

      esp_sleep_enable_timer_wakeup(SLEEP_DURATION_US);                                                          // Temporizar el deep sleep durante el tiempo establecido
      esp_deep_sleep_start();
    }else{
      Debugln(F("Failed to publish data"));
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
    Debug(F("Attempting MQTT connection..."));

    if(mqttClient.connect("soil_quaity_sensor", access_token, NULL)){                                            // Attempt to connect
      Debugln(F("connected"));
    }else{
      Debug(F("failed, rc="));
      Debug(mqttClient.state());
      Debugln(F(" try again in 5 seconds"));

      delay(5000);                                                                                               // Wait 5 seconds before retrying
    }
  }
}
// RECONNECT TO MQTT END -------------------------------------------------------------------------------------------------------------------------------------

// CONNECT TO WIFI -------------------------------------------------------------------------------------------------------------------------------------------
static void connectToWiFi(){
  Debug(F("Connecting to WIFI SSID "));
  Debugln(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid, password);

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Debug(".");
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);

    checkPEKPress();
  }

  Debugln(F(""));

  Debug(F("WiFi connected, IP address: "));
  Debugln(WiFi.localIP());

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
      Debugln(F("\t\t\tLong press detected: Shutting down..."));
      axp.shutdown();
    }

    axp.clearIRQ();
  }
}
// CHECK PEK PRESS END ---------------------------------------------------------------------------------------------------------------------------------------

// SETUP OTA -------------------------------------------------------------------------------------------------------------------------------------------------
static void setupOTA(){
  ArduinoOTA.setHostname("soil-quality-sensor");                                                                 // Set custom OTA hostname
  ArduinoOTA.setPassword("pw0123");                                                                              // No authentication by default
  
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else                                                                                                       // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Debugln(String(F("Start updating ")) + type);
    })
    .onEnd([]() {
      Debugln(F("\nEnd"));
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Debugf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Debugf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Debugln(F("Auth Failed"));
      else if (error == OTA_BEGIN_ERROR) Debugln(F("Begin Failed"));
      else if (error == OTA_CONNECT_ERROR) Debugln(F("Connect Failed"));
      else if (error == OTA_RECEIVE_ERROR) Debugln(F("Receive Failed"));
      else if (error == OTA_END_ERROR) Debugln(F("End Failed"));
    });

  ArduinoOTA.begin();

  Debugln(F("OTA service started!"));
}
// SETUP OTA END ---------------------------------------------------------------------------------------------------------------------------------------------
// AUXILIARY FUNCTIONS END ===================================================================================================================================
