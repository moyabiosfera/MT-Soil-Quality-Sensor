#pragma once

void connectToWiFi(bool stateLED, AXP20X_Class& axp192, const char* ssid, const char* password, const uint8_t ledPin, const uint8_t pmuIRQPin);
void reconnectToWiFi(bool stateLED, const char* ssid, const char* password, uint8_t ledPin, SemaphoreHandle_t serialSemaphore);