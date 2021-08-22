/*
 * running
*/
#include <M5Stack.h>

#define HR_SIGNAL_INPUT 26 // pin no of the heart rate sensor input
#define TM_SPEED_INPUT 22  // pin no of the treadmill line sensor input
#define HR_SIGNALS_SIZE 10 // how many heart beats for calculating a heart rate

void setup(){

  // init M5  
  M5.begin();
  M5.Power.begin();

  // init pins
  pinMode(HR_SIGNAL_INPUT, INPUT);
  pinMode(TM_SPEED_INPUT, INPUT);

  // init UI
  M5.Lcd.setTextSize(3);
}

// the main loop
void loop() {

  // read heart rate
  readHeartRate();
  
  // read treadmill speed 
  readTreadmillSpeed();

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
  hrSignal = (boolean) digitalRead(HR_SIGNAL_INPUT);
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
  spSignal = !digitalRead(TM_SPEED_INPUT);
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

