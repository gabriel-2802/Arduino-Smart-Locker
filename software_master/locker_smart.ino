#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Servo.h>
#include <Wire.h>
#include "StateSafe.h"
#include "icons.h"
#include "utils.h"

void myDigitalWrite(uint8_t pin, bool value) {
  uint8_t bit = digitalPinToBitMask(pin);
  uint8_t port = digitalPinToPort(pin);
  volatile uint8_t* out;

  if (port == NOT_A_PIN) return;

  out = portOutputRegister(port);

  if (value) {
  *out |= bit; // Set bit HIGH
  } else {
  *out &= ~bit; // Set bit LOW
  }
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
  myDigitalWrite(LED_PIN, LOW);   // Turn OFF LED
}

void unlock() {
  lockServo.write(SERVO_UNLOCK_POS);
  myDigitalWrite(LED_PIN, HIGH);  // Turn ON LED
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

void handlePhaseIdle() {
  requestSensorData();
  lcd.clear();

  // Handle tilt alert from slave
  if (packet.tilt) {
    failedAttempts = 3;  // Max out attempts to trigger buzzer
    myDigitalWrite(BUZZER_PIN, HIGH);
  }

  init_icons(lcd);
  // Row 1: lock icon, temperature, humidity, sound %
  lcd.setCursor(0, 0);
  lcd.write(ICON_LOCKED_CHAR); //
  lcd.print(" ");
  lcd.print((int)packet.temperature);
  lcd.print("C ");
  lcd.write(ICON_WATER_DROP_CHAR);
  lcd.print((int)packet.humidity);
  lcd.print("% ");
  lcd.write(ICON_SOUND_CHAR);
  lcd.print(packet.soundPercent);
  lcd.print("%");

  // Row 2: H:hh [____] M:mm
  lcd.setCursor(0, 1);
  lcd.print("H:");
  if (packet.hour < 10) lcd.print("0");
  lcd.print(packet.hour);
  lcd.print(" ");
  lcd.print("[____]");
  lcd.setCursor(12, 1);
  lcd.print("M:");
  if (packet.minute < 10) lcd.print("0");
  lcd.print(packet.minute);

  lcd.setCursor(6, 1);
  inputCode = "";
  lockedPhase = PHASE_WAIT_INPUT;
}

void handlePhaseWaitInput() {
  char key = keypad.getKey();
  if (key >= '0' && key <= '9' && inputCode.length() < 4) {
    lcd.print('*');
    inputCode += key;
  }
  if (inputCode.length() == 4) {
    lockedPhase = PHASE_VERIFY;
  }
}

void handlePhaseVerify() {
  if (safeState.unlock(inputCode)) {
    unlock();
    failedAttempts = 0;
    myDigitalWrite(BUZZER_PIN, LOW);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Access Granted");
    lockedPhase = PHASE_GRANTED;
  } else {
    failedAttempts++;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Access Denied");
    if (failedAttempts >= 3) {
      myDigitalWrite(BUZZER_PIN, HIGH);
    }
    lockedPhase = PHASE_DENIED;
  }
  phaseStart = millis();
}

void handlePhaseGrantOrDeny() {
  if (millis() - phaseStart >= 1500) {
    lockedPhase = PHASE_IDLE;
    lcd.clear();
  }
}

void safeLockedLogicStep() {
  switch (lockedPhase) {
    case PHASE_IDLE: handlePhaseIdle(); break;
    case PHASE_WAIT_INPUT: handlePhaseWaitInput(); break;
    case PHASE_VERIFY: handlePhaseVerify(); break;
    case PHASE_GRANTED:
    case PHASE_DENIED: handlePhaseGrantOrDeny(); break;
  }
}

void handleUnlockIdle() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write(ICON_UNLOCKED_CHAR);
  lcd.setCursor(2, 0);
  lcd.print(" # to lock");
  lcd.setCursor(15, 0);
  lcd.write(ICON_UNLOCKED_CHAR);
  if (safeState.hasCode()) {
    lcd.setCursor(0, 1);
    lcd.print("  A = new code");
  }
  unlockedPhase = PHASE_UNLOCK_OPTIONS;
}

void handleUnlockOptions() {
  char key = keypad.getKey();
  if (key == '#') {
    unlockedPhase = PHASE_RELOCK;
  } else if (key == 'A' || !safeState.hasCode()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter new code:");
    lcd.setCursor(5, 1);
    lcd.print("[____]");
    lcd.setCursor(6, 1);
    newCode = "";
    unlockedPhase = PHASE_NEW_CODE_ENTER;
  }
}

void handleNewCodeEnter() {
  char key = keypad.getKey();
  if (key >= '0' && key <= '9' && newCode.length() < 4) {
    lcd.print('*');
    newCode += key;
  }
  if (newCode.length() == 4) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Confirm new code");
    lcd.setCursor(5, 1);
    lcd.print("[____]");
    lcd.setCursor(6, 1);
    confirmCode = "";
    unlockedPhase = PHASE_NEW_CODE_CONFIRM;
  }
}

void handleNewCodeConfirm() {
  char key = keypad.getKey();
  if (key >= '0' && key <= '9' && confirmCode.length() < 4) {
    lcd.print('*');
    confirmCode += key;
  }
  if (confirmCode.length() == 4) {
    if (confirmCode == newCode) {
      safeState.setCode(confirmCode);
      lcd.clear();
      lcd.print("Code set!");
      unlockedPhase = PHASE_CODE_SUCCESS;
    } else {
      lcd.clear();
      lcd.setCursor(1, 0);
      lcd.print("Code mismatch");
      lcd.setCursor(0, 1);
      lcd.print("Safe not locked!");
      unlockedPhase = PHASE_CODE_FAIL;
    }
    unlockPhaseStart = millis();
  }
}

void handleCodeSuccessOrFail() {
  if (millis() - unlockPhaseStart >= 1500) {
    unlockedPhase = PHASE_UNLOCK_IDLE;
  }
}

void handleRelock() {
  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.write(ICON_UNLOCKED_CHAR);
  lcd.print(" ");
  lcd.write(ICON_RIGHT_ARROW);
  lcd.print(" ");
  lcd.write(ICON_LOCKED_CHAR);
  safeState.lock();
  lock();
  unlockPhaseStart = millis();
  unlockedPhase = PHASE_CODE_SUCCESS;
}

void safeUnlockedLogicStep() {
  switch (unlockedPhase) {
    case PHASE_UNLOCK_IDLE: handleUnlockIdle(); break;
    case PHASE_UNLOCK_OPTIONS: handleUnlockOptions(); break;
    case PHASE_NEW_CODE_ENTER: handleNewCodeEnter(); break;
    case PHASE_NEW_CODE_CONFIRM: handleNewCodeConfirm(); break;
    case PHASE_CODE_SUCCESS:
    case PHASE_CODE_FAIL: handleCodeSuccessOrFail(); break;
    case PHASE_RELOCK: handleRelock(); break;
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
  myDigitalWrite(LED_PIN, LOW);  // Start off


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

    if (!userPresent && wasUserPresent) {
      lastUserSeen = currentMillis;
    }

    if (!userPresent && (currentMillis - lastUserSeen >= INACTIVITY_TIMEOUT)) {
      resetScreenState();
    }
  }

  requestSensorData();

  // Trigger the alarm if tilt was detected even with no user
  if (packet.tilt) {
    failedAttempts = 3;
    myDigitalWrite(BUZZER_PIN, HIGH);
  }
  

  if (userPresent) {
    if (safeState.locked()) {
      safeLockedLogicStep();
    } else {
      safeUnlockedLogicStep();
    }
  }
}
