/*
 * running
 * 
 * Prerequisite:
 * - M5Stack
 * - PbHub: https://docs.m5stack.com/en/unit/pbhub copy porthub.cpp and porthub.h to the sketch directory
*/
#include <M5Stack.h>
#include <Wire.h>
#include "porthub.h"

//#define HR_SIGNAL_INPUT 26 // pin no of the heart rate sensor input
//#define TM_SPEED_INPUT 22  // pin no of the treadmill line sensor input
#define HR_SIGNALS_SIZE 10 // how many heart beats for calculating a heart rate

#define REMOTE_SPEED_UP 17 // pin no for increasing the speed
#define REMOTE_SPEED_START 2 // pin no for start/stop
#define REMOTE_SPEED_DOWN 5 // pin no for decreasing the speed

PortHub porthub;

void setup(){

  // init M5  
  M5.begin();
  M5.Power.begin();

  // init pins
  porthub.begin();
//  pinMode(HR_SIGNAL_INPUT, INPUT);
//  pinMode(TM_SPEED_INPUT, INPUT);
  pinMode(REMOTE_SPEED_UP, OUTPUT_OPEN_DRAIN);
  pinMode(REMOTE_SPEED_START, OUTPUT_OPEN_DRAIN);
  pinMode(REMOTE_SPEED_DOWN, OUTPUT_OPEN_DRAIN);
  digitalWrite(REMOTE_SPEED_UP, true);
  digitalWrite(REMOTE_SPEED_START, true);
  digitalWrite(REMOTE_SPEED_DOWN, true);

  // init UI
  M5.Lcd.setTextSize(3);
}

// the main loop
void loop() {

  // read heart rate
  readHeartRate();
  
  // read treadmill speed 
  readTreadmillSpeed();

  M5.update();
  if (M5.BtnA.wasPressed()) {
    digitalWrite(REMOTE_SPEED_UP, false);
    delay(500);
    digitalWrite(REMOTE_SPEED_UP, true);
  }

  if (M5.BtnB.wasPressed()) {
    digitalWrite(REMOTE_SPEED_START, false);
    delay(500);
    digitalWrite(REMOTE_SPEED_START, true);
  }

  if (M5.BtnC.wasPressed()) {
    digitalWrite(REMOTE_SPEED_DOWN, false);
    delay(500);
    digitalWrite(REMOTE_SPEED_DOWN, true);
  }

  // update UI
  updateUI();
  delay(10);
}

// Heart rate pubic status
boolean hrSignal = false;
float heartRate = 0;

// Heart rate private status
int hrIdx = 0;
boolean lastHrSignal = false;
unsigned long lastHrTime = 0;
unsigned long hrSignals[HR_SIGNALS_SIZE];

void readHeartRate() {
  unsigned long beatTime;
  hrSignal = (boolean) porthub.hub_d_read_value_A(HUB1_ADDR);
//  hrSignal = (boolean) digitalRead(HR_SIGNAL_INPUT);
  if (hrSignal != lastHrSignal) {
    lastHrSignal = hrSignal;
    unsigned long now = millis();
    if (hrSignal == true && (now - lastHrTime) > 200) { // remove signals shorter than 200 ms
      addBeatTime(now);
      lastHrTime = now;
    }
    heartRate = calcHr();
  }  
}

void addBeatTime(unsigned long beatTime) {
  hrSignals[hrIdx++] = beatTime;
  if (hrIdx == HR_SIGNALS_SIZE) {
    hrIdx = 0;
  }
}

float calcHr() {
  unsigned long lastHr = hrIdx == 0 ? hrSignals[HR_SIGNALS_SIZE - 1] : hrSignals[hrIdx - 1];
  unsigned long firstHr = hrSignals[hrIdx];
  float hrInterval = float(lastHr - firstHr) / float(HR_SIGNALS_SIZE - 1);
  return firstHr == 0 ? -1.0 : float(1000.0 / hrInterval * 60.0);
}

// Treadmill speed public status
boolean spSignal = false;
float treadmillSpeed = 0;

// Treadmill speed private status
boolean lastSpSignal = false;
unsigned long lastSpSignalTime = 0;
float spSignalInterval = 0;

void readTreadmillSpeed() {
//  spSignal = !digitalRead(TM_SPEED_INPUT);
  spSignal = !(boolean)porthub.hub_d_read_value_A(HUB2_ADDR);
  if (spSignal != lastSpSignal) {
    lastSpSignal = spSignal;
    unsigned long now = millis();
    if (spSignal && (now - lastSpSignalTime) > 500) { // remove signals shorter than 500 ms
      spSignalInterval = float(now - lastSpSignalTime);
      treadmillSpeed = calcSpeed(spSignalInterval);
      lastSpSignalTime = now;
    }
  }  
}

// calculate the speed from the interval by a power series function
// the parameters should be measured from the actual treadmill
float calcSpeed(float interval) {
  return 11716.0 * pow(interval, -1.04); 
}

void updateUI() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(50, 100);
  M5.Lcd.print("HR: " + String(hrSignal) + ", " + String(heartRate));  
  M5.Lcd.setCursor(50, 140);
  M5.Lcd.print("TM: " + String(spSignal) + ", " + String(treadmillSpeed));
}

