/*
 * running
*/
#include <M5Stack.h>

#define HR_INPUT 26
#define SPEED_INPUT 22
#define HR_SIGNALS_SIZE 10

void setup(){
  M5.begin();
  M5.Power.begin();
  pinMode(HR_INPUT, INPUT);
  pinMode(SPEED_INPUT, INPUT);
  M5.Lcd.setTextSize(3);
}

// HR status
unsigned long hrSignals[HR_SIGNALS_SIZE];
int hrIdx = 0;
boolean lastHrSignal = false;
unsigned long lastHrTime = 0;
float heartRate = 0;

void addBeatTime(unsigned long beatTime) {
  hrSignals[hrIdx++] = beatTime;
  if (hrIdx == HR_SIGNALS_SIZE) {
    hrIdx = 0;
  }
}

float calcHr() {
  unsigned long lastHr = hrIdx == 0 ? hrSignals[HR_SIGNALS_SIZE - 1] : hrSignals[hrIdx - 1];
  unsigned long firstHr = hrSignals[hrIdx];
//  M5.Lcd.setCursor(50, 100);
//  M5.Lcd.print(String(int(firstHr / 1000)) + ", " + String(int(lastHr / 1000)));
  double hrInterval = double(lastHr - firstHr) / double(HR_SIGNALS_SIZE - 1);
  return firstHr == 0 ? -1.0 : float(1000.0 / hrInterval * 60.0);
}

// Speed status
boolean lastSpSignal = false;
unsigned long lastSpSignalTime = 0;
int spSignalInterval = 0;
float speed = 0;

void loop() {

  // read HR signal
  unsigned long beatTime;
  int hrSignal = digitalRead(HR_INPUT) ? 1 : 0;
  if (hrSignal != lastHrSignal) {
    lastHrSignal = hrSignal;
    unsigned long now = millis();
    if (hrSignal == true && (now - lastHrTime) > 200) {
      addBeatTime(now);
      lastHrTime = now;
    }
    heartRate = calcHr();
  }

  // read speed signal
  boolean spSignal = !digitalRead(SPEED_INPUT);
  if (spSignal != lastSpSignal) {
    lastSpSignal = spSignal;
    unsigned long now = millis();
    if (spSignal && (now - lastSpSignalTime) > 500) {
      spSignalInterval = int(now - lastSpSignalTime);
      lastSpSignalTime = now;
      speed = calcSpeed(spSignalInterval);
    }
  }

  // update UI
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(50, 100);
  M5.Lcd.print(String(speed));
  M5.Lcd.setCursor(50, 140);
  M5.Lcd.print(String(hrSignal) + ", " + String(heartRate));
  delay(10);
}

// calculate the speed from the interval by a power series
// the parameters should be measured from the actual treadmill
float calcSpeed(int interval) {
  return 11716 * pow(interval, -1.04); 
}
