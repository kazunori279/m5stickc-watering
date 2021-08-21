/*
    M5StickC watering

    For M5StickC + Watering device: https://docs.m5stack.com/en/unit/watering
    
    Usage: The display shows two bars: moisture levels for every second and every hour.
    Use BtnB and Axp button to set moisture threshold. When the level goes under it
    over 1 hour, it starts watering for 90 sec (about 500 mL).
*/


#include <M5StickC.h>
#include <MedianFilterLib2.h>
#include <Preferences.h>

#define INPUT_PIN 33
#define PUMP_PIN 32
#define ADC_DRY 1900
#define ADC_WET 1600
#define WATERING_TIME 90 // 90 sec
#define WET_VALS_SEC_SIZE 10 // 1 sec
#define WET_VALS_HOUR_SIZE 3600 // 1 hour
#define PREF_WATERING "wtr"
#define PREF_WATERING_THRESHOLD "wtrThr"

MedianFilter2<int> wetValsInSec(WET_VALS_SEC_SIZE);
MedianFilter2<int> wetValsInHour(WET_VALS_HOUR_SIZE);

Preferences pref;
int wateringThreshold;

void setup() { 
  
    // init display
    M5.begin();
    M5.Lcd.setRotation(3);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextDatum(MC_DATUM);

    // read pref
    pref.begin(PREF_WATERING, true);
    wateringThreshold = pref.getInt(PREF_WATERING_THRESHOLD, 10);
    pref.end();

    // init pins
    pinMode(INPUT_PIN, ANALOG);
    pinMode(PUMP_PIN, OUTPUT);
    digitalWrite(PUMP_PIN,false);
    analogSetPinAttenuation(INPUT_PIN, ADC_11db);
}

// loop states
int wetRatioSec = 0;
int wetRatioHourly = 0;
bool watering = false;

// timers
int lastSec = 0;
int lastWateringSec = 0;
int wateringTimer = 0;

// main loop
void loop() { 

  // get wet ratio
  int adcVal = max(0, (ADC_DRY - analogRead(INPUT_PIN)) * 100 / (ADC_DRY - ADC_WET));
  wetRatioSec = wetValsInSec.AddValue(adcVal);

  // for every second
  RTC_TimeTypeDef rtcTime;
  M5.Rtc.GetTime(&rtcTime);
  if (rtcTime.Seconds != lastSec) {

    // update timers
    lastSec = rtcTime.Seconds;
    lastWateringSec++;

    // check for watering (wait for an hour)
    wetRatioHourly = wetValsInHour.AddValue(wetRatioSec);
    if (wetRatioHourly < wateringThreshold && lastWateringSec > 3600) {
      watering = true;
      digitalWrite(PUMP_PIN, watering);
      lastWateringSec = 0;
    }

    // update watering timer
    if (watering) {
      wateringTimer++;
      if (wateringTimer > WATERING_TIME) {
        watering = false;
        wateringTimer = 0;
        digitalWrite(PUMP_PIN, watering);
      }
    }

    // update disp
    updateDisp();    
  }  

  // BtnA: start/stop watering
  M5.update();
  if(M5.BtnA.wasPressed()){
    watering = !watering;
    wateringTimer = 0;
    digitalWrite(PUMP_PIN, watering);
  }

  // BtnB or Axp button: change watering threshold
  if (M5.BtnB.wasPressed()) {
    wateringThreshold = min(wateringThreshold + 5, 90);
    updateDisp();
    saveWateringThreshold();
  }
  if (M5.Axp.GetBtnPress() == 2) {
    wateringThreshold = max(wateringThreshold - 5, 0);
    updateDisp();
    saveWateringThreshold();
  }

  delay(100);
}

// save watering threshold to the NVS
void saveWateringThreshold() {
  pref.begin(PREF_WATERING, false);
  pref.putInt(PREF_WATERING_THRESHOLD, wateringThreshold);
  pref.end();
}

// update display
void updateDisp() {
  M5.Lcd.fillRect(0, 0, 160, 80, DARKGREY);
  int widthSec = 160 * wetRatioSec / 100;
  M5.Lcd.fillRect(0, 0, widthSec, 39, BLUE);
  int widthHour = 160 * wetRatioHourly / 100;
  M5.Lcd.fillRect(0, 40, widthHour, 80, BLUE);
  int posThreshold = 160 * wateringThreshold / 100;
  M5.Lcd.fillRect(posThreshold, 0, 2, 80, RED);
}
