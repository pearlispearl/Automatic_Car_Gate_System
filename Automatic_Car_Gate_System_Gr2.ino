#include <Arduino.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <RTClib.h>
#include <SD.h>
#include <SPI.h>

#define SERVO_PIN   26
#define TRIG_PIN     5
#define ECHO_PIN    25
#define SWITCH_PIN   4    
#define SD_CS_PIN   13    

Servo myServo;
RTC_DS3231 rtc;
File logFile;

volatile bool systemOn = false; 
volatile bool buttonPressed = false;

enum SystemState {
  IDLE,
  ACTIVE
};

SystemState currentState = IDLE;

void IRAM_ATTR switchISR() {
  buttonPressed = true;
}

String printDateTime(const DateTime &dt) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d:%02d",
           dt.day(), dt.month(), dt.year(),
           dt.hour(), dt.minute(), dt.second());
  return String(buf);
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Servo
  myServo.attach(SERVO_PIN);
  myServo.write(0);

  // Ultrasonic
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Switch Button
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SWITCH_PIN), switchISR, FALLING);

  // RTC setup
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting time to compile time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // SD card setup
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD Card initialization failed!");
  } else {
    Serial.println("SD Card ready.");
  }

  Serial.println("Press button to start/stop system.");
}

long readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  return duration * 0.034 / 2; 
}

void loop() {
  if (buttonPressed) {
    buttonPressed = false;
    if (currentState == IDLE) {
      currentState = ACTIVE;
      Serial.println("System ON");
    } else {
      currentState = IDLE;
      Serial.println("System OFF - Entering low power mode.");
      myServo.write(0);
    }
  }

  switch (currentState) {
    case IDLE:
      delay(200);
      break;

    case ACTIVE: {
      int distance = readDistance();
      DateTime now = rtc.now();
      String timeString = printDateTime(now);

      Serial.print(timeString);
      Serial.print(" Distance: ");
      Serial.print(distance);
      Serial.println(" cm");

      // Log to SD card
      logFile = SD.open("/datalog.txt", FILE_APPEND);
      if (logFile) {
        logFile.print(timeString);
        logFile.print(" - Distance: ");
        logFile.print(distance);
        logFile.println(" cm");
        logFile.close();
      } else {
        Serial.println("Failed to open log file");
      }
      
      // Servo control
      if (distance > 0 && distance < 10) {
        myServo.write(150);
        delay(5000);
      } else {
        myServo.write(25);
      }

      delay(500);
      break; 
    }
  } 
} 

