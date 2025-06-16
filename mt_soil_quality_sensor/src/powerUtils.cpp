#include <Arduino.h>
#include "powerUtils.h"
#include "macros.h"

void setupPower(AXP20X_Class& axp192, const uint8_t pmuIRQPin, void (*isr)()){
    axp192.setPowerOutPut(AXP192_DCDC1, AXP202_ON);                                                                   // Turn on the 3V3 pin corresponding to DCDC1 on the AXP192 - Power on sensors

    axp192.setPowerOutPut(AXP192_LDO2, AXP202_OFF);                                                                   // Turn off LoRa
    axp192.setPowerOutPut(AXP192_LDO3, AXP202_OFF);                                                                   // Disable GPS power
    Debugln(F("GPS and LoRa powered off"));

    axp192.adc1Enable(AXP202_BATT_VOL_ADC1, true);                                                                    // Enable ADC for battery voltage

    pinMode(pmuIRQPin, INPUT);                                                                                   // Set up PEK button IRQ pin

    axp192.clearIRQ();                                                                                                // Clear any existing IRQs
    axp192.enableIRQ(AXP202_PEK_LONGPRESS_IRQ, true);                                                                 // Enable PEK IRQ for long press
    attachInterrupt(digitalPinToInterrupt(PMU_IRQ_PIN), isr, FALLING);                                    // Enable the interruption to notify the ESP32 to give access to execute the code to power off the device
}

void pekThreadRoutine(volatile bool* pekPressedFlag, AXP20X_Class& axp192, SemaphoreHandle_t serialSemaphore){
    if(*pekPressedFlag){                                                                                                // Check for PEK press ISR flag
        *pekPressedFlag = false;
        axp192.readIRQ();                                                                                               // The task checks the type of IRQ

        if(axp192.isPEKLongtPressIRQ()){                                                                                // If the IRQ is long-press type, the device is switched off
            if(xSemaphoreTake(serialSemaphore, portMAX_DELAY)){
                Debugln(F("Long press detected: Shutting down..."));
                xSemaphoreGive(serialSemaphore);
            }
            vTaskDelay(pdMS_TO_TICKS(100));                                                                            // Delay to get to see the print
            axp192.shutdown();
        }

        axp192.clearIRQ();
    }
}