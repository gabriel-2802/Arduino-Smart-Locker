#include <Wire.h>
#include <DHT.h>
#include <RtcDS1302.h>
#include "utils.h"

// Temperature + humidity senzor
DHT dht(DHTPIN, DHTTYPE);

// Clock
ThreeWire myWire(DAT_PIN, CLK_PIN, RST_PIN);
RtcDS1302<ThreeWire> rtc(myWire);

// tilt used for interuptions
volatile bool tiltDetected = false;

// packet to send to the master
SensorPacket packet;

void onRequest() {
  packet.tilt = tiltDetected;
  Wire.write((uint8_t*)&packet, sizeof(packet));

  tiltDetected = false;
}

void onTilt() {
  tiltDetected = true;
}

void setup() {
  dht.begin();

  // Set RTC to a specific date a
  // RtcDateTime compiled = RtcDateTime(2025, 5, 17, 11, 0, 0);  // e.g., 17 May 2025, 10:30:00
  // rtc.SetDateTime(compiled);

  // set tilt senzor pin and create interruption
  pinMode(TILT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TILT_PIN), onTilt, FALLING);

  rtc.Begin();
  rtc.SetIsRunning(true);

  // slave to Master arduino
  Wire.begin(SLAVE_ADDRESS);
  Wire.onRequest(onRequest);
}

void loop() {
  // read clock data
  RtcDateTime now = rtc.GetDateTime();
  packet.hour = now.Hour();
  packet.minute = now.Minute();
  packet.day = now.Day();
  packet.month = now.Month();
  packet.year = now.Year();

  // read temp + humidity
  packet.temperature = dht.readTemperature();
  packet.humidity = dht.readHumidity();

  // red sound senzor - percentage
  int soundRaw = analogRead(SOUND_ANALOG_PIN);
  packet.soundPercent = map(soundRaw, 0, 1023, 0, 100);

  // tilt
  packet.tilt = tiltDetected;
  tiltDetected = false;

  // every 0.25 seconds
  delay(250);
}

