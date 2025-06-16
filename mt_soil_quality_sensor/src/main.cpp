/* ***********************************************************************************************************************************************************
SOIL QUALITY SENSOR: this file includes the main code for the soil quality sensor used in Daniel Rodriguez Moya's Master Thesis. It sends data to ThingsBoard
via MQTT at a fixed frequency, measuring soil temperature and moisture using a DS18B20 and a FC-38, respectively.
*********************************************************************************************************************************************************** */

// ===========================================================================================================================================================
// LIBRARY INCLUSION
// ===========================================================================================================================================================
#include <Arduino.h>                                                                                             // Library for PlatformIO to use the Arduino environment
// Wi-Fi and MQTT libs ---------------------------------------------------------------------------------------------------------------------------------------
#include <WiFi.h>                                                                                                // Library to connect to Wi-Fi
#include <WiFiClientSecure.h>                                                                                    // Library to add TLS certificates to MQTT connection
#include <PubSubClient.h>                                                                                        // Library to connect to a MQTT broker
// ArduinoOTA libs -------------------------------------------------------------------------------------------------------------------------------------------
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
// I2C libs --------------------------------------------------------------------------------------------------------------------------------------------------
#include <Wire.h>
#include <axp20x.h>                                                                                              // Library for the PMU AXP192
// Config libs -----------------------------------------------------------------------------------------------------------------------------------------------
#include "macros.h"
#include "mqttUtils.h"
#include "otaUtils.h"
#include "wifiUtils.h"
#include "sleepUtils.h"
#include "powerUtils.h"
// Sensors libs ----------------------------------------------------------------------------------------------------------------------------------------------
#include "sensors.h"
// LIBRARIES INCLUSION END ===================================================================================================================================

// ===========================================================================================================================================================
// CONSTRUCTORES DE OBJETOS DE CLASE DE LIBRERIA, VARIABLES GLOBALES, CONSTANTES...
// ===========================================================================================================================================================
static WiFiClientSecure secureClient;                                                                            // Object of the Wi-Fi library
static PubSubClient mqttClient(secureClient);                                                                    // Object of the MQTT library
static AXP20X_Class axp;
// CONSTRUCTORES END =========================================================================================================================================

// ===========================================================================================================================================================
// GLOBAL VARIABLES
// ===========================================================================================================================================================
// Variables -------------------------------------------------------------------------------------------------------------------------------------------------
static bool ledState = LOW;
static volatile bool pekPressed = false;
static RTC_DATA_ATTR uint32_t bootCount = 1;                                                                     // Boot counter must be stored in the RTC memory so it survives deep sleep, but not power-off
// GLOBAL VARIABLES END ======================================================================================================================================

// ===========================================================================================================================================================
// ISR
// ===========================================================================================================================================================
static void IRAM_ATTR handlePMUIRQ() {
  pekPressed = true;
}
// ISR END ===================================================================================================================================================

// ===========================================================================================================================================================
// FREERTOS ELEMENTS
// ===========================================================================================================================================================
// Task handles ----------------------------------------------------------------------------------------------------------------------------------------------
static TaskHandle_t MQTTTaskHandle = NULL, PEKTaskHandle = NULL;
// Semaphore -------------------------------------------------------------------------------------------------------------------------------------------------
static SemaphoreHandle_t semaphoreSerial = NULL;
// Tasks -----------------------------------------------------------------------------------------------------------------------------------------------------
static void MQTTTask(void*);
static void PEKTask(void*);
// FREERTOS ELEMENTS END =====================================================================================================================================

// ===========================================================================================================================================================
// THREADS
// ===========================================================================================================================================================
// MQTT thread -----------------------------------------------------------------------------------------------------------------------------------------------
static void MQTTTask(void *pvParameters){
  while(true) {
    ArduinoOTA.handle();                                                                                           // If a new version is available, download and install it

    if(!mqttClient.connected()){                                                                                   // If no connection
      reconnectToMQTT(mqttClient, MQTT_CLIENT, ACCESS_TOKEN, semaphoreSerial);                                     // Call reconnect function
    }
    mqttClient.loop();                                                                                             // Main MQTT function. It must run at the highest frequency and never be blocked

    if(WiFi.status() != WL_CONNECTED){
      reconnectToWiFi(ledState, WIFI_SSID, WIFI_PASSWORD, LED_PIN, semaphoreSerial);                               // Connect to Wi-Fi during the execution of the thread
    }else{                                                                                                         // Check WiFi connection status
      // MQTT Pub ----------------------------------------------------------------------------------------------------------------------------------------------
      char dataStr[256];                                                                                           // A string is created to save a JSON containing the variables and values to be published with a size of 256 characters
      // Sensor readings ---------------------------------------------------------------------------------------------------------------------------------------
      // float soilTemp = random(1000, 4500) / 100.0f;                                                                // Simulated measurements
      float soilMoist = 94.47;
      float soilTemp = getMedianTemperatureC(TEMPERATURE_SAMPLES);                                                 // Real measurements, iterated 5 times to get the median and so more robust data
      // float soilMoist = getMedianSoilMoisture(MOISTURE_SAMPLES);
      // Sensor readings END -----------------------------------------------------------------------------------------------------------------------------------
      axp.setPowerOutPut(AXP192_DCDC1, AXP202_OFF);                                                                // Turn off the sensors after measurements have been taken

      float batVolt = (axp.getBattVoltage()) / 1000.0f;                                                            // Read battery voltage in mV and convert it to V

      sprintf(dataStr, "{\"treeId\":%u,\"bootCnt\":%lu,\"soilTemperature\":%4.2f,\"soilMoisture\":%5.2f,\"batVoltage\":%4.3f}",
              TREE_ID, (unsigned long)bootCount, soilTemp, soilMoist, batVolt);                                    // 'sprintf' C++ function is used to introduce the values of the sensor variables with the optimal formatting
      
      if(mqttClient.publish(MQTT_TOPIC_PUB, dataStr)){                                                             // The string is published on ThingsBoard topic
        if(xSemaphoreTake(semaphoreSerial, portMAX_DELAY)){
          Debugln(dataStr);                                                                                        // Display the string in the serial monitor
          Debugln(F("Going to sleep until next TX..."));
          xSemaphoreGive(semaphoreSerial);
        }
        bootCount++;

        sleep_seconds(SLEEP_DURATION_S);                                                                           // Schedule deep sleep for the specified duration (30 seconds)
      }else{
        if(xSemaphoreTake(semaphoreSerial, portMAX_DELAY)){
          Debugln(F("Failed to publish data"));
          xSemaphoreGive(semaphoreSerial);
        }
      }
      // MQTT Pub END ----------------------------------------------------------------------------------------------------------------------------------------
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// PEK THREAD ------------------------------------------------------------------------------------------------------------------------------------------------
static void PEKTask(void *pvParameters){
  while(true) {
    pekThreadRoutine(&pekPressed, axp, semaphoreSerial);

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
// THREADS END ===============================================================================================================================================

// ===========================================================================================================================================================
// SETUP FUNCTION
// ===========================================================================================================================================================
void setup() {
  #if ENABLE_SERIAL
    Serial.begin(115200);
  #endif

  Debugln(F("Soil Quality Sensor Beta"));

  // AXP192 setup --------------------------------------------------------------------------------------------------------------------------------------------
  Wire.begin(SDA_PIN, SCL_PIN);                                                                                  // Initialize I2C bus
  
  if(axp.begin(Wire, AXP192_SLAVE_ADDRESS) != 0){                                                                // "AXP192_SLAVE_ADDRESS" should be "0x34"
    Debugln(F("AXP192 not detected!"));
    while(1);
  }else{
    Debugln(F("AXP192 detected"));
  }

  setupPower(axp, PMU_IRQ_PIN, handlePMUIRQ);                                                                                  // AXP192 setup
  initSensors();                                                                                                 // Function from the custom library to setup the sensors
  sleep_interrupt(BUTTON_PIN, 0);                                                                                // Enable deep sleep interrupt using builtin button
  connectToWiFi(ledState, axp, WIFI_SSID, WIFI_PASSWORD, LED_PIN, PMU_IRQ_PIN);                                  // Connect to Wi-Fi during setup
  setupOTA();                                                                                                    // Function that contains all the OTA parameters setup
  connectToMQTT(mqttClient, secureClient, ROOT_CA, MQTT_SERVER, MQTT_PORT);                                      // Connectarse al broker MQTT y establecer TLS

  // FreeRTOS setup ------------------------------------------------------------------------------------------------------------------------------------------
  // Create the semaphore
  semaphoreSerial = xSemaphoreCreateMutex();

  // Initialize Tasks
  xTaskCreatePinnedToCore(
    MQTTTask,                                                                                                    /* Function to implement the task */
    "MQTTTask",                                                                                                  /* Name of the task */
    10000,                                                                                                       /* Stack size in bytes */
    NULL,                                                                                                        /* Task input parameter */
    1,                                                                                                           /* Priority of the task */
    &MQTTTaskHandle,                                                                                             /* Task handle. */
    1                                                                                                            /* Core where the task should run */
  );

  xTaskCreatePinnedToCore(
    PEKTask,                                                                                                     /* Function to implement the task */
    "PEKTask",                                                                                                   /* Name of the task */
    5000,                                                                                                        /* Stack size in bytes */
    NULL,                                                                                                        /* Task input parameter */
    1,                                                                                                           /* Priority of the task */
    &PEKTaskHandle,                                                                                              /* Task handle. */
    0                                                                                                            /* Core where the task should run */
  );
  // FreeRTOS setup END --------------------------------------------------------------------------------------------------------------------------------------
}
// SETUP FUNCTION END ========================================================================================================================================

// ===========================================================================================================================================================
// LOOP FUNCTION
// ===========================================================================================================================================================
void loop() {
  delay(10000);                                                                                                  // Empty loop as FreeRTOS is doing the tasks' job
}
// LOOP FUNCTION END =========================================================================================================================================