/*
 * running
 * 
 * Requirements:
 * - M5Stack lib
 * - PbHub: https://docs.m5stack.com/en/unit/pbhub copy porthub.cpp and porthub.h
 * - FirebaseESP32 lib
 * - running-config.h: create a file with the following configurations
 *   #define WIFI_SSID "<SSID>"
 *   #define WIFI_PASS "<PASSWORD>"
 *   #define FIREBASE_HOST "<FIREBASE HOST NAME>"
 *   #define FIREBASE_AUTH "<FIREBASE AUTH KEY>"
*/
#include <time.h>
#include <M5Stack.h>
#include <Wire.h>
#include <FirebaseESP32.h>
#include "porthub.h"
#include "running_config.h"

#define HR_WINDOW_SIZE 10 // how many heart beats for calculating a heart rate

#define FIREBASE_ROOT "gcp-running"

#define NTP_HOST "ntp.nict.jp" // NTP host for clock correction
#define TIMEZONE 60 * 60 * 9 // Timezone offset (JST = 9) for clock correction

#define REMOTE_SPEED_UP 17   // pin no for increasing the speed
#define REMOTE_SPEED_START 2 // pin no for start/stop
#define REMOTE_SPEED_DOWN 5  // pin no for decreasing the speed

#define PREF_RUNNING "running" // the key for storing Preferenes

FirebaseData fbdo;

TFT_eSprite sprite = TFT_eSprite(&M5.Lcd);

PortHub porthub;

void setup(){

  // init M5  
  M5.begin();
  M5.Power.begin();
  M5.Speaker.begin();
  M5.Speaker.setVolume(2);

  // init pins
  porthub.begin();
  pinMode(REMOTE_SPEED_UP, OUTPUT_OPEN_DRAIN);
  pinMode(REMOTE_SPEED_START, OUTPUT_OPEN_DRAIN);
  pinMode(REMOTE_SPEED_DOWN, OUTPUT_OPEN_DRAIN);
  digitalWrite(REMOTE_SPEED_UP, true);
  digitalWrite(REMOTE_SPEED_START, true);
  digitalWrite(REMOTE_SPEED_DOWN, true);

  // init UI
  sprite.setColorDepth(8);
  sprite.createSprite(M5.Lcd.width(), M5.Lcd.height());
  
  // init WiFi
  M5.Lcd.print("WiFi.");  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    M5.Lcd.print(".");
  }
  M5.Lcd.fillScreen(BLACK);

  // init clock
  configTime(TIMEZONE, 0, NTP_HOST);

  // init Firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(50, 100);

  // init finished
  beep();
}

void beep() {
  M5.Speaker.beep();
  delay(100);
  M5.Speaker.mute();
}

// UI status
#define ST_STOP  "STOP"
#define ST_RUN   "RUN"
#define ST_PAUSE "PAUSE"
String uiStatus = ST_STOP;
String runId = "";
unsigned long runStartedAt = 0;

// the main loop
void loop() {

  // read heart rate
  readHeartRate();
  
  // read treadmill speed 
  readTreadmillSpeed();

  // send the metrics to Firebase
  sendMetrics();

  // read buttons
  M5.update();
  if (M5.BtnA.wasPressed()) {
    if (uiStatus == ST_STOP) {
      uiStatus = ST_RUN;
      runId = getRunId();
      runStartedAt = millis();
      beep();
    }
  }
  if (M5.BtnB.wasPressed()) {
    if (uiStatus == ST_RUN || uiStatus == ST_PAUSE) {
      uiStatus = ST_STOP;
      beep();
    }    
  }
  if (M5.BtnC.wasPressed()) {
    if (uiStatus == ST_RUN) {
      uiStatus = ST_PAUSE;
    } else if (uiStatus == ST_PAUSE) {
      uiStatus = ST_RUN;
    }
    beep();
  }

  // update UI
  updateUI();
//  delay(10);
}

String getRunId() {
  char buf[20];
  struct tm ti;
  getLocalTime(&ti);
  sprintf(buf, "%04d%02d%02d-%02d%02d%02d", 
    ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday, ti.tm_hour, ti.tm_min, ti.tm_sec); 
  return "R" + (String)buf;
}

// Heart rate pubic status
boolean hrSignal = false;
float heartRate = 0;

// Heart rate private status
boolean lastHrSignal = false;
unsigned long lastHrSignalAt = 0;
float heartRates[HR_WINDOW_SIZE];
int hrCount = 0;

void readHeartRate() {

  // read HR signal
  unsigned long now = millis();
  hrSignal = (boolean) porthub.hub_d_read_value_A(HUB1_ADDR);
  if (hrSignal != lastHrSignal) {
    lastHrSignal = hrSignal;

    // update HR, removing signals shorter than 400 ms (150/min)
    if (hrSignal == true && (now - lastHrSignalAt) > 400) { 
      float hr = 60.0 * 1000.0 / float(now - lastHrSignalAt);
      Serial.println(hr);
      heartRate = addAndCalcHeartRate(hr);
      lastHrSignalAt = now;
    }

  }  

  // set HR to 0 when hrSignal is not coming for 1.5 sec
  if (lastHrSignalAt == 0 || (now - lastHrSignalAt) > 1500) {
    heartRate = 0;
    hrCount = 0;
  }
}

float addAndCalcHeartRate(float hr) {
  heartRates[hrCount++ % HR_WINDOW_SIZE] = hr;
  if (hrCount < HR_WINDOW_SIZE + 5) {
    return 0; // return 0 if it's too few
  }
  float sum = 0;
  for (int i = 0; i < HR_WINDOW_SIZE; i++) {
    sum += heartRates[i];
  }
  return sum / float(HR_WINDOW_SIZE);
}

// Treadmill speed public status
boolean spSignal = false;
float treadmillSpeed = 0;

// Treadmill speed private status
boolean lastSpSignal = false;
unsigned long lastSpSignalAt = 0;
float spSignalInterval = 0;

void readTreadmillSpeed() {
  
  unsigned long now = millis();
  spSignal = !(boolean)porthub.hub_d_read_value_A(HUB2_ADDR);
  if (spSignal != lastSpSignal) {
    lastSpSignal = spSignal;

    // update treadmill speed, removing signals shorter than 500 ms
    if (spSignal && (now - lastSpSignalAt) > 500) {
      spSignalInterval = float(now - lastSpSignalAt);
      treadmillSpeed = calcSpeed(spSignalInterval);
      lastSpSignalAt = now;
    }
  }

  // set speed to 0 when the signal is not coming for 10 sec
  if (lastSpSignalAt == 0 || (now - lastSpSignalAt) > 10000) {
    treadmillSpeed = 0;
  }
}

// calculate the speed from the interval by a power series function
// the parameters should be measured from the actual treadmill
float calcSpeed(float interval) {
  return 11716.0 * pow(interval, -1.04); 
}

unsigned long lastMetricsSentAt = 0;

void sendMetrics() {
  unsigned long now = millis();
  if (uiStatus == ST_RUN && now - lastMetricsSentAt > 1000) {
    lastMetricsSentAt = now;
    String elapsedTime = String(int(now - runStartedAt) / 1000);
    String path = "/" + String(FIREBASE_ROOT) + "/" + runId + "/" + elapsedTime + "/";
    Firebase.setFloatAsync(fbdo, path + "hr", heartRate);
    Firebase.setFloatAsync(fbdo, path + "speed", treadmillSpeed);
    Serial.println(path);
  }
}

void updateUI() {
  sprite.setTextSize(3);
  sprite.fillScreen(BLACK);
  sprite.setCursor(50, 50);
  sprite.print("Status: " + uiStatus);  
  sprite.setCursor(50, 100);
  sprite.print("HR: " + String(hrSignal) + ", " + String(heartRate));  
  sprite.setCursor(50, 140);
  sprite.print("TM: " + String(spSignal) + ", " + String(treadmillSpeed));
  sprite.pushSprite(0, 0);
}
