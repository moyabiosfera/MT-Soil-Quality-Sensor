#include <Arduino.h>                                                                                             // Library for PlatformIO to use the Arduino environment
#include <ArduinoOTA.h>
#include "otaUtils.h"
#include "macros.h"

// SETUP OTA -------------------------------------------------------------------------------------------------------------------------------------------------
void setupOTA(){
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