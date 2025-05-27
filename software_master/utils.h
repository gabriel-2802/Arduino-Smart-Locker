#ifndef UTILS_H
#define UTILS_H

#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Servo.h>
#include <Wire.h>

#include "StateSafe.h"
#include "icons.h"

// Servo Motor
#define SERVO_PIN 6
#define SERVO_LOCK_POS   180
#define SERVO_UNLOCK_POS 90

// Ultrasonic sensor
#define TRIG_PIN 8
#define ECHO_PIN 9
#define PROXIMITY_THRESHOLD_CM 75

// Buzzer
#define BUZZER_PIN 7

#define CHECK_INTERVAL 300
#define INACTIVITY_TIMEOUT 5000

// LED
#define LED_PIN 12

// Keypad
#define KEYPAD_ROWS 4
#define KEYPAD_COLS 4
byte rowPins[KEYPAD_ROWS] = {5, 4, 3, 2};
byte colPins[KEYPAD_COLS] = {A3, A2, A1, A0};
char keys[KEYPAD_ROWS][KEYPAD_COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

#define SLAVE_ADDRESS 0x08

// States
enum LockedPhase { PHASE_IDLE, PHASE_WAIT_INPUT, PHASE_VERIFY, PHASE_NOTIFICATION };
enum UnlockedPhase { PHASE_UNLOCK_IDLE, PHASE_UNLOCK_OPTIONS, PHASE_NEW_CODE_SETUP };
enum CodeSubPhase { CODE_ENTER, CODE_CONFIRM };


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


#endif