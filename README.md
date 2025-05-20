# Soil Quality Sensor

## LilyGO T-Beam v1.1 based agrosensor

___

This project represents an IoT end node that connects to the Internet via Wi-Fi and MQTT over TLS to publish soil moisture and temperature captured by FC-38 and DS18B20 sensors, respectively. It does also take into account the capabilities of the AXP192 PMU to power down the GPS and LoRa chips to save battery. In addition, the device goes to deep sleep after each TX frame for a configurable amount of time.
