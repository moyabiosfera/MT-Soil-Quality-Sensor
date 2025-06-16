#pragma once

#include <PubSubClient.h>
#include <WiFiClientSecure.h>

void connectToMQTT(PubSubClient& client, WiFiClientSecure &clientSecure, const char* rootCa, const char* mqttServer, const uint16_t mqttPort);
void reconnectToMQTT(PubSubClient& client, const char* clientId, const char* token, SemaphoreHandle_t serialSemaphore);
