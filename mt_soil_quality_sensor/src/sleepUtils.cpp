#include <Arduino.h>    
#include <esp_sleep.h>

void sleep_interrupt(gpio_num_t gpio, uint8_t mode) {
    esp_sleep_enable_ext0_wakeup(gpio, mode);
}

void sleep_seconds(uint64_t seconds) {
    esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
    esp_deep_sleep_start();
}