#include <Arduino.h>
#include "macros.h"
#include "mqttUtils.h"

// CONNECT TO MQTT -------------------------------------------------------------------------------------------------------------------------------------------
void connectToMQTT(PubSubClient& client, WiFiClientSecure &clientSecure, const char* rootCa, const char* mqttServer, const uint16_t mqttPort) {
  clientSecure.setCACert(rootCa);                                                                               // Initialization of the ciphered connection
  client.setServer(mqttServer, mqttPort);                                                                  // Function of the MQTT library to establish connection with the broker
}
// CONNECT TO MQTT END ---------------------------------------------------------------------------------------------------------------------------------------

// RECONNECT TO MQTT -----------------------------------------------------------------------------------------------------------------------------------------
void reconnectToMQTT(PubSubClient& client, const char* clientId, const char* token, SemaphoreHandle_t serialSemaphore) {
  while(!client.connected()){                                                                                // Loop until we're reconnected
    if(xSemaphoreTake(serialSemaphore, portMAX_DELAY)){
      Debug(F("Attempting MQTT connection..."));
      xSemaphoreGive(serialSemaphore);
    }

    if(client.connect("soil_quaity_sensor", token, NULL)){                                            // Attempt to connect
      if(xSemaphoreTake(serialSemaphore, portMAX_DELAY)){
        Debugln(F("connected"));
        xSemaphoreGive(serialSemaphore);
      }
    }else{
      if(xSemaphoreTake(serialSemaphore, portMAX_DELAY)){
        Debug(F("failed, rc="));
        Debug(client.state());
        Debugln(F(" try again in 5 seconds"));
        xSemaphoreGive(serialSemaphore);
      }

      vTaskDelay(pdMS_TO_TICKS(5000));                                                                           // Wait 5 seconds before retrying
    }
  }
}
// RECONNECT TO MQTT END -------------------------------------------------------------------------------------------------------------------------------------