#include "Arduino.h"
#include "TFT_eSPI.h" /* Please use the TFT library provided in the library. */

#include <SCD30_I2C.h>
#include <RTClib.h>

#include "pin_config.h"
#include "img_logo.h"

#define TFT_CHANNEL 0
#define TFT_FREQUENCY 2000
#define TFT_RESOLUTION 8
uint8_t brightnessTFT = 7;      // max is 8
uint8_t TFT_DUTYCYCLE[] = {0, 1, 3, 7, 15, 31, 63, 127, 255};

TFT_eSPI tft = TFT_eSPI();

TFT_eSprite background = TFT_eSprite(&tft);
TFT_eSprite fps = TFT_eSprite(&tft);
TFT_eSprite rtctime = TFT_eSprite(&tft);
TFT_eSprite temperature = TFT_eSprite(&tft);
TFT_eSprite humidity = TFT_eSprite(&tft);
TFT_eSprite histogram = TFT_eSprite(&tft);
TFT_eSprite co2value = TFT_eSprite(&tft);
TFT_eSprite ppmtext = TFT_eSprite(&tft);

uint8_t co2Histogram[320] = {};

SCD30_I2C sensorCO2;

float co2;  //in ppm
float temp; //in C
float humd; //in RH%
uint8_t co2_readingInterval = 4;

RTC_DS3231 rtc;

DateTime now;
char daysOfTheWeek[7][3] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

// variables for controlling button functions
bool buttonState_A, buttonState_B;       // Pulled-Up: pressing = HIGH, not pressing = LOW
bool previousButton_A, previousButton_B;    // compare with new button state
bool buttonA_pressed, buttonB_pressed;
unsigned long debounceButton_A, debounceButton_B; // debouncing buttons

unsigned long currentMillis = 0, previousMillis = 0; // timer variable
unsigned long timerCO2 = 0, timerRTC = 0;
unsigned long co2_measurement_delay = 5000;

void displaySprite();

void buttonFunctionA();
void buttonFunctionB();

void pressingButtonA(void) {
  if (currentMillis - debounceButton_A >= 135) {
    buttonA_pressed = 1;
    debounceButton_A = currentMillis;
  }
}

void pressingButtonB(void) {
  if (currentMillis - debounceButton_B >= 135) {
    buttonB_pressed = 1;
    debounceButton_B = currentMillis;
  }
}

void setup()
{
    Serial.begin(460800);
    if (!Serial) delay(200);       // give Serial some time

    pinMode(PIN_BUTTON_1, INPUT);
    pinMode(PIN_BUTTON_2, INPUT);

    attachInterrupt(digitalPinToInterrupt(PIN_BUTTON_1), pressingButtonA, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_BUTTON_2), pressingButtonB, CHANGE);

    tft.begin(); delay(200);
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);
    tft.setSwapBytes(true);
    tft.pushImage(0, 0, 320, 170, (uint16_t *)img_logo);
    // tft.setSwapBytes(false);
    delay(1500);

    ledcSetup(TFT_CHANNEL, TFT_FREQUENCY, TFT_RESOLUTION);
    ledcAttachPin(PIN_LCD_BL, TFT_CHANNEL);
    ledcWrite(TFT_CHANNEL, TFT_DUTYCYCLE[7]);

    Wire.setPins(PIN_JST_SDA, PIN_JST_SCL);
    Wire.begin();

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    // tft.setFreeFont();
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.print(__DATE__);
        tft.print(" - ");
            tft.println(__TIME__);
    tft.setCursor(10, tft.getCursorY());
    tft.print("CPU  = ");
        tft.print(getCpuFrequencyMhz());
            tft.println(" MHz");
    tft.setCursor(10, tft.getCursorY());
    tft.print("XTAL = ");
        tft.print(getXtalFrequencyMhz());
            tft.println(" MHz");
    tft.setCursor(10, tft.getCursorY());
    tft.print("APB  = ");
        tft.print(getApbFrequency());
            tft.println(" Hz");
    tft.setCursor(10, tft.getCursorY());
    tft.print("SCD: ");
    delay(1000);
    if (!sensorCO2.begin()) {
        tft.println("ERROR");
    }
    else {
        sensorCO2.setMeasurementInterval(5);
        // sensorCO2.setTemperatureOffset(110);
        // sensorCO2.setAltitude(1043);
        sensorCO2.setAutoCalibration(0);
        // sensorCO2.readFirmwareVersion();
        tft.println("READY");
    }
    tft.setCursor(10, tft.getCursorY());
    tft.print("RTC: ");
    delay(1000);
    if (!rtc.begin()) {
        tft.println("ERROR");
    }
    else {
        // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        rtc.disable32K();
        tft.println("READY");
    }
    delay(2000);

    for (int i = 0; i < 320; i++) {
        co2Histogram[i] = 128;
    }
}

void loop()
{
    previousMillis = currentMillis;
    currentMillis = millis();

    // call button functionality
    buttonState_A = digitalRead(PIN_BUTTON_1);
    buttonState_B = digitalRead(PIN_BUTTON_2);
    if (buttonState_A) buttonFunctionA();    // button A is LOW
    if (buttonState_B) buttonFunctionB();    // button B is LOW

    if (currentMillis > timerCO2 + co2_measurement_delay && sensorCO2.getMeasurementStatus()) {
        sensorCO2.getMeasurement(&co2, &temp, &humd);
        if (co2 < 400) {
            co2Histogram[0] = 0;
        }
        else if (co2 > 2000) {
            co2Histogram[0] = 127;
        }
        else {
            co2Histogram[0] = (co2 - 400) * 0.08;
        }
        if (co2_readingInterval > 15) {
            for (int i = 320 - 1; i > 0; i--) {
                co2Histogram[i] = co2Histogram[i - 1];
            }
            co2_readingInterval = 0;
        }
        co2_readingInterval++;
        // Serial.println(co2);
        timerCO2 = currentMillis;
    }

    Serial.print(digitalRead(PIN_BUTTON_1));
    Serial.print(" ");
    Serial.print(digitalRead(PIN_BUTTON_2));
    Serial.print(" ");
    Serial.println(brightnessTFT);

    if (currentMillis > timerRTC + 1000) {
        now = rtc.now();
        timerRTC = currentMillis;
    }
    displaySprite();
}

void displaySprite()
{
    background.createSprite(320, 170);
    background.setSwapBytes(true);
    background.pushImage(0, 0, 320, 170, (uint16_t *)img_logo);
    background.setSwapBytes(false);

    fps.createSprite(2 * 14, 19);
    fps.fillSprite(TFT_MAGENTA);
    fps.setTextColor(TFT_WHITE, TFT_MAGENTA);
    fps.setTextDatum(TR_DATUM);
    unsigned long showFPS = 1000 / (currentMillis - previousMillis);
    fps.drawNumber(showFPS, 28, 0, 4);
    fps.pushToSprite(&background, 2, 2, TFT_MAGENTA);
    fps.deleteSprite();
    
    rtctime.createSprite(6 * 14 + 2 * 7, 19);
    rtctime.fillSprite(TFT_MAGENTA);
    rtctime.setTextColor(TFT_WHITE, TFT_MAGENTA);
    rtctime.setTextDatum(TL_DATUM);
    char timeString[] = "hh:mm:ss";
    rtctime.drawString(now.toString(timeString), 0, 0, 4);
    rtctime.pushToSprite(&background, 220, 2, TFT_MAGENTA);
    rtctime.deleteSprite();

    temperature.createSprite(3 * 14 + 7, 19);
    temperature.fillSprite(TFT_MAGENTA);
    temperature.setTextColor(TFT_WHITE, TFT_MAGENTA);
    temperature.setTextDatum(TR_DATUM);
    temperature.drawFloat(temp, 1, 49, 0, 4);
    temperature.pushToSprite(&background, 2, 148, TFT_MAGENTA);
    temperature.deleteSprite();

    humidity.createSprite(3 * 14 + 7, 19);
    humidity.fillSprite(TFT_MAGENTA);
    humidity.setTextColor(TFT_WHITE, TFT_MAGENTA);
    humidity.setTextDatum(TR_DATUM);
    humidity.drawFloat(humd, 1, 49, 0, 4);
    humidity.pushToSprite(&background, 270, 148, TFT_MAGENTA);
    humidity.deleteSprite();

    histogram.createSprite(170, 127);
    histogram.fillSprite(TFT_MAGENTA);
    // draw line for CO2 level histogram
    for (int i = 1; i < 320; i++) {
        if (co2Histogram[i] < 128) {
            int j = 0;
            if (co2Histogram[i - 1] != co2Histogram[i]) {
                // return -1 or +1 depending on difference
                j = (co2Histogram[i - 1] - co2Histogram[i]) / std::abs(co2Histogram[i - 1] - co2Histogram[i]);
            }
            histogram.drawLine(i, 127 - co2Histogram[i - 1] + j , i, 127 - co2Histogram[i], TFT_LIGHTGREY);
        }
    }
    histogram.fillCircle(0, 127 - co2Histogram[0], 2, TFT_RED);
    histogram.pushToSprite(&background, 0, 22, TFT_MAGENTA);
    histogram.deleteSprite();

    co2value.createSprite(4 * 55, 76);
    co2value.fillSprite(TFT_MAGENTA);
    co2value.setTextColor(TFT_WHITE, TFT_MAGENTA);
    co2value.setTextDatum(TR_DATUM);
    co2value.drawNumber(co2, 220, 0, 8);
    co2value.pushToSprite(&background, 50, 40, TFT_MAGENTA);
    co2value.deleteSprite();

    ppmtext.createSprite(18, 7);
    ppmtext.fillSprite(TFT_MAGENTA);
    ppmtext.setTextColor(TFT_WHITE, TFT_MAGENTA);
    ppmtext.setTextDatum(TL_DATUM);
    ppmtext.drawString("ppm", 0, 0, 1);
    ppmtext.pushToSprite(&background, 270, 40, TFT_MAGENTA);
    ppmtext.deleteSprite();

    background.pushSprite(0, 0, TFT_MAGENTA);
    background.deleteSprite();
}

void buttonFunctionA() {
  // when button is pressed, do stuff
  if (buttonA_pressed == 1) {
    if (brightnessTFT < 8) {
        brightnessTFT++;
        ledcWrite(TFT_CHANNEL, TFT_DUTYCYCLE[brightnessTFT]);
    }
    buttonA_pressed = 0;    // prevent button spam
  }
}

void buttonFunctionB() {
  // when button is pressed, do stuff
  if (buttonB_pressed == 1) {
    if (brightnessTFT > 0) {
        brightnessTFT--;
        ledcWrite(TFT_CHANNEL, TFT_DUTYCYCLE[brightnessTFT]);
    }
    buttonB_pressed = 0;    // prevent button spam
  }
}