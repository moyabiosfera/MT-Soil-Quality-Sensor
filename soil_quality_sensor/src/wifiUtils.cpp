#include <Arduino.h>
#include <WiFi.h>                                                                                                // Library to connect to Wi-Fi
#include <axp20x.h>
#include "wifiUtils.h"
#include "macros.h"

// Connect to Wi-Fi during setup ---------------------------------------------------------------------------------------------------------------------------
void connectToWiFi(bool stateLED, AXP20X_Class& axp192, const char* ssid, const char* password, const uint8_t ledPin, const uint8_t pmuIRQPin) {
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, stateLED);
  
  Debug(F("Connecting to WIFI SSID "));
  Debugln(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Debug(".");
    stateLED = !stateLED;
    digitalWrite(ledPin, stateLED);

    if (digitalRead(pmuIRQPin) == LOW) {
      axp192.readIRQ();

      if (axp192.isPEKLongtPressIRQ()) {
        Debugln(F("Long press detected: Shutting down..."));
        delay(100);
        axp192.shutdown();
      }

      axp192.clearIRQ();
    }
  }

  Debugln(F(""));
  Debug(F("WiFi connected, IP address: "));
  Debugln(WiFi.localIP());

  if (stateLED) {
    digitalWrite(ledPin, LOW);
  }
}
// Connect to Wi-Fi during setup END -----------------------------------------------------------------------------------------------------------------------

// Connect to Wi-Fi during the execution of the thread ---------------------------------------------------------------------------------------------------
void reconnectToWiFi(bool stateLED, const char* ssid, const char* password, const uint8_t ledPin, SemaphoreHandle_t serialSemaphore){
    if(xSemaphoreTake(serialSemaphore, portMAX_DELAY)){
    Debug(F("Connecting to WIFI SSID "));
    Debugln(ssid);
    xSemaphoreGive(serialSemaphore);
    }

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));
    WiFi.begin(ssid, password);

    while(WiFi.status() != WL_CONNECTED){
    vTaskDelay(pdMS_TO_TICKS(500));
    if(xSemaphoreTake(serialSemaphore, portMAX_DELAY)){
      Debug(".");
      xSemaphoreGive(serialSemaphore);
    }
    stateLED = !stateLED;
    digitalWrite(ledPin, stateLED);
    }

    if(xSemaphoreTake(serialSemaphore, portMAX_DELAY)){
      Debugln(F(""));

      Debug(F("WiFi connected, IP address: "));
      Debugln(WiFi.localIP());
      xSemaphoreGive(serialSemaphore);
    }

    if(stateLED){
      digitalWrite(ledPin, LOW);
    }
}
// Connect to Wi-Fi during the execution of the thread END -----------------------------------------------------------------------------------------------