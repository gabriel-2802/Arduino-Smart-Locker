#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Servo.h>
#include <Wire.h>
#include "StateSafe.h"
#include "icons.h"
#include "utils.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo lockServo;
SafeState safeState;
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS);

LockedPhase lockedPhase = PHASE_IDLE;
UnlockedPhase unlockedPhase = PHASE_UNLOCK_IDLE;
CodeSubPhase codeSubPhase;

unsigned long phaseStart, lastCheck = 0, lastUserSeen = 0;
bool userPresent = false, wasUserPresent = false, hasInteracted = false;
int failedAttempts = 0;

String inputCode = "", newCode = "", confirmCode = "", notificationMsg = "";

SensorPacket packet;

void myDigitalWrite(uint8_t pin, bool value) {
  uint8_t bit = digitalPinToBitMask(pin);
  uint8_t port = digitalPinToPort(pin);
  volatile uint8_t* out;
  if (port == NOT_A_PIN) return;
  out = portOutputRegister(port);
  if (value) *out |= bit;
  else *out &= ~bit;
}

void requestSensorData() {
  Wire.requestFrom(SLAVE_ADDRESS, sizeof(SensorPacket));
  if (Wire.available() == sizeof(SensorPacket)) {
    Wire.readBytes((char*)&packet, sizeof(SensorPacket));
  }
}

void lock() {
  lockServo.write(SERVO_LOCK_POS);
  safeState.lock();
  myDigitalWrite(LED_PIN, LOW);
}

void unlock() {
  lockServo.write(SERVO_UNLOCK_POS);
  myDigitalWrite(LED_PIN, HIGH);
}

bool isUserClose() {
  myDigitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  myDigitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  myDigitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  return (duration > 0 && (duration * 0.034 / 2) <= PROXIMITY_THRESHOLD_CM);
}

void resetScreenState() {
  lcd.clear();
  lcd.noBacklight();
  hasInteracted = false;
  lockedPhase = PHASE_IDLE;
  unlockedPhase = PHASE_UNLOCK_IDLE;
  inputCode = newCode = confirmCode = "";
}

void showNotification(const String& msg, bool isLockedPhase) {
  lcd.clear();
  lcd.setCursor((16 - msg.length()) / 2, 0);
  lcd.print(msg);
  phaseStart = millis();
  if (isLockedPhase) lockedPhase = PHASE_NOTIFICATION;
  else unlockedPhase = PHASE_UNLOCK_IDLE;
}

void displaySensorSummary(bool locked) {
  lcd.clear();
  init_icons(lcd);

  lcd.setCursor(0, 0);
  lcd.write(locked ? ICON_LOCKED_CHAR : ICON_UNLOCKED_CHAR);
  lcd.print(" ");

  lcd.print((int)packet.temperature);
  lcd.print("C ");

  lcd.write(ICON_WATER_DROP_CHAR);
  lcd.print((int)packet.humidity);
  lcd.print("% ");

  lcd.write(ICON_SOUND_CHAR);
  lcd.print(packet.soundPercent);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print("H:");
  if (packet.hour < 10) lcd.print("0");
  lcd.print(packet.hour);

  lcd.print(" [____] M:");

  if (packet.minute < 10) lcd.print("0");
  lcd.print(packet.minute);
}

void handleLockedIdle() {
  requestSensorData();
  if (packet.tilt) myDigitalWrite(BUZZER_PIN, HIGH);
  displaySensorSummary(true);

  lcd.setCursor(6, 1);
  inputCode = "";
  lockedPhase = PHASE_WAIT_INPUT;
}

void handleLockedWaitInput() {
  char key = keypad.getKey();
  if (key >= '0' && key <= '9' && inputCode.length() < 4) {
    lcd.print('*');
    inputCode += key;
  }
  if (inputCode.length() == 4) {
    if (safeState.unlock(inputCode)) {
      unlock();
      failedAttempts = 0;
      myDigitalWrite(BUZZER_PIN, LOW);
      showNotification("Access Granted", true);
    } else {
      failedAttempts++;
      if (failedAttempts >= 3) myDigitalWrite(BUZZER_PIN, HIGH);
      showNotification("Access Denied", true);
    }
  }
}

void handleLockedNotification() {
  if (millis() - phaseStart >= 1500) {
    lockedPhase = PHASE_IDLE;
    lcd.clear();
  }
}

void safeLockedLogicStep() {
  switch (lockedPhase) {
    case PHASE_IDLE: handleLockedIdle(); break;
    case PHASE_WAIT_INPUT: handleLockedWaitInput(); break;
    case PHASE_NOTIFICATION: handleLockedNotification(); break;
  }
}

void handleUnlockIdle() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write(ICON_UNLOCKED_CHAR);

  lcd.print(" B to lock");
  if (safeState.hasCode()) {
    lcd.setCursor(0, 1);
    lcd.print("  A = new code");
  }
  unlockedPhase = PHASE_UNLOCK_OPTIONS;
}

void handleUnlockOptions() {
  char key = keypad.getKey();
  if (key == 'B') {
    lock();
    showNotification("Locked", false);
  } else if (key == 'A' || !safeState.hasCode()) {
    lcd.clear();
    lcd.print("Enter new code:");
    lcd.setCursor(6, 1);
    newCode = "";
    codeSubPhase = CODE_ENTER;
    unlockedPhase = PHASE_NEW_CODE_SETUP;
  }
}

void handleNewCodeSetup() {
  char key = keypad.getKey();
  if (key >= '0' && key <= '9') {
    if (codeSubPhase == CODE_ENTER && newCode.length() < 4) {
      lcd.print('*');
      newCode += key;
    } else if (codeSubPhase == CODE_CONFIRM && confirmCode.length() < 4) {
      lcd.print('*');
      confirmCode += key;
    }
  }
  if (newCode.length() == 4 && codeSubPhase == CODE_ENTER) {
    lcd.clear();
    lcd.print("Confirm code:");
    lcd.setCursor(6, 1);
    confirmCode = "";
    codeSubPhase = CODE_CONFIRM;
  }
  if (confirmCode.length() == 4 && codeSubPhase == CODE_CONFIRM) {
    if (confirmCode == newCode) {
      safeState.setCode(confirmCode);
      showNotification("Code set!", false);
    } else {
      showNotification("Code mismatch", false);
    }
  }
}

void safeUnlockedLogicStep() {
  switch (unlockedPhase) {
    case PHASE_UNLOCK_IDLE: handleUnlockIdle(); break;
    case PHASE_UNLOCK_OPTIONS: handleUnlockOptions(); break;
    case PHASE_NEW_CODE_SETUP: handleNewCodeSetup(); break;
  }
}

void setup() {
  lcd.init();
  lcd.backlight();
  init_icons(lcd);

  lockServo.attach(SERVO_PIN);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  myDigitalWrite(BUZZER_PIN, LOW);

  Wire.begin();
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  myDigitalWrite(LED_PIN, LOW);

  safeState.locked() ? lock() : unlock();
  
  lcd.setCursor(4, 0);
  lcd.print("Welcome!");
  delay(500);
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastCheck >= CHECK_INTERVAL) {
    lastCheck = currentMillis;
    wasUserPresent = userPresent;
    userPresent = isUserClose();

    if (userPresent && !wasUserPresent) {
      lcd.backlight();
      hasInteracted = false;
    }
    if (!userPresent && wasUserPresent) lastUserSeen = currentMillis;
    if (!userPresent && (currentMillis - lastUserSeen >= INACTIVITY_TIMEOUT)) resetScreenState();
  }

  requestSensorData();
  // activate buzzer if tilt detected => theft or tampering
  if (packet.tilt) myDigitalWrite(BUZZER_PIN, HIGH);

  // continue the dfa with the current phase
  if (userPresent) {
    if (safeState.locked()) safeLockedLogicStep();
    else safeUnlockedLogicStep();
  }
}