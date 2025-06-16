// ===========================================================================================================================================================
// LIBRARY INCLUSION
// ===========================================================================================================================================================
#include <Arduino.h>                                                                                             // Library for PlatformIO to use the Arduino environment
#include <OneWire.h>
#include <DallasTemperature.h>
#include <QuickMedianLib.h>
#include "sensors.h"
#include "macros.h"
// LIBRARY INCLUSION END =====================================================================================================================================

// ===========================================================================================================================================================
// CONSTRUCTORES DE OBJETOS DE CLASE DE LIBRERIA, VARIABLES GLOBALES, CONSTANTES...
// ===========================================================================================================================================================
static OneWire oneWireBus(ONE_WIRE_PIN);
static DallasTemperature tempSensor(&oneWireBus);
// CONSTRUCTORES END =========================================================================================================================================

// ===========================================================================================================================================================
// GLOBAL VARIABLES
// ===========================================================================================================================================================
static const float humedadAire = 605.0f;
static const float humedadAgua = 500.0f;
// GLOBAL VARIABLES END ======================================================================================================================================

// ===========================================================================================================================================================
// SETUP FUNCTIONS
// ===========================================================================================================================================================
void initSensors() {
  analogSetAttenuation(ADC_11db);                                                                                // Set the attenuation to -11 dB to go from 0V to 3V3 in the range of 0 to 4095
  tempSensor.begin();                                                                                            // Start the OneWire bus for the DS18B20
}
// SETUP FUNCTIONS END =======================================================================================================================================

// ===========================================================================================================================================================
// LOOP FUNCTIONS
// ===========================================================================================================================================================
// SOIL TEMPERATURE FUNCTIONS --------------------------------------------------------------------------------------------------------------------------------
// READ TEMPERATURE FUNCTION
static float readTemperatureC() {
  tempSensor.requestTemperatures();                                                                              // Ask the OneWire bus to be available to read the temperature
  return tempSensor.getTempCByIndex(0);                                                                          // Read temperature from the first (and only) device
}

// GET MEDIAN TEMPERATURE FROM "X" SAMPLES
float getMedianTemperatureC(uint8_t samples) {
  if (samples == 0) return 0.0f;                                                                               // If the function is called like "getMedianTemperature(0)", just return 0

  float measurements[samples];                                                                                 // Create a local array of measurements of size "samples"

  for (uint8_t i = 0; i < samples; i++) {                                                                      // For each loop cycle,
    measurements[i] = readTemperatureC();                                                                    // add each measurement to its corresponding index
    delay(10);                                                                                               // Small delay between samples
  }

  return QuickMedian<float>::GetMedian(measurements, samples);                                                 // Return the median value corresponding to the measurements array
}
// SOIL TEMPERATURE FUNCTIONS END ----------------------------------------------------------------------------------------------------------------------------

// SOIL MOISTURE FUNCTIONS -----------------------------------------------------------------------------------------------------------------------------------
// ADAPTION OF MAP FUNCTION TO WORK WITH FLOATS
static float fmap(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// READ MOISTURE FUNCTION
static float readSoilMoisturePercent() {
  int raw = analogRead(SOIL_MOIST_PIN);
  float percent = fmap(raw, humedadAire, humedadAgua, 0.0f, 100.0f);
  return constrain(percent, 0.0f, 100.0f);
}

// GET MEDIAN MOISTURE FROM "X" SAMPLES
float getMedianSoilMoisture(uint8_t samples) {
  if (samples == 0) return 0.0;

  float values[samples];

  for (uint8_t i = 0; i < samples; i++) {
    values[i] = readSoilMoisturePercent();
    delay(10);
  }

  return QuickMedian<float>::GetMedian(values, samples);
}
// SOIL MOISTURE FUNCTIONS END -------------------------------------------------------------------------------------------------------------------------------
// LOOP FUNCTIONS END ========================================================================================================================================