#pragma once

#define DHTPIN 2              // Digital pin for DHT11
#define DHTTYPE DHT11         // DHT 11 senzor
#define SOUND_ANALOG_PIN A1   // Analog input from KY-037
#define TILT_PIN 3            // Digital pin with interrupt from SW-530

// clock digital PINS
#define CLK_PIN 4
#define DAT_PIN 5
#define RST_PIN 6

#define SLAVE_ADDRESS 0x08

struct SensorPacket {
  uint8_t hour;
  uint8_t minute;
  uint8_t day;
  uint8_t month;
  uint16_t year;

  float temperature;
  float humidity;
  uint8_t soundPercent;
  bool tilt;
} __attribute__((packed));
