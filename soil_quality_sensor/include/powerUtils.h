#pragma once

#include <axp20x.h>

void setupPower(AXP20X_Class& axp192, const uint8_t pmuIRQPin, void (*isr)());
void pekThreadRoutine(volatile bool* pekPressedFlag, AXP20X_Class& axp192, SemaphoreHandle_t serialSemaphore);