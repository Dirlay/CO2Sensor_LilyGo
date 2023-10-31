/*
 * CO2_V2_Project
 *
 * Graphical CO2 monitor using the popular LilyGo T-Display S3 development board.
 * 
 * Created by Dirlay, 08.10.2023.
 * Updated by Dirlay, 30.10.2023.
 * Released into the public domain.
*/
#include "header.h"
#include "pin_config.h"

#include <TFT_eSPI.h>       // TFT Screen
#include "img_logo.h"

#define TFT_CHANNEL 0
#define TFT_FREQUENCY 2000
#define TFT_RESOLUTION 8

TFT_eSPI tft = TFT_eSPI();

TFT_eSprite background = TFT_eSprite(&tft);
TFT_eSprite fps = TFT_eSprite(&tft);
TFT_eSprite rtctime = TFT_eSprite(&tft);
TFT_eSprite temperature = TFT_eSprite(&tft);
TFT_eSprite humidity = TFT_eSprite(&tft);
TFT_eSprite histogram = TFT_eSprite(&tft);
TFT_eSprite co2value = TFT_eSprite(&tft);
TFT_eSprite ppmtext = TFT_eSprite(&tft);

uint8_t brightnessTFT = 7;          // max is 8
uint8_t TFT_DUTYCYCLE[] = {0, 1, 3, 7, 15, 31, 63, 127, 255};
char co2Histogram[320] = {};
uint8_t histogramDrawCounter = 1;
const uint8_t histogramDrawInterval = 5;   // when the next point will be drawn.
float averageCO2;

#include <SCD30_I2C.h>      // CO2 Sensor

SCD30_I2C sensorCO2;

float co2, old_co2;  //in ppm
float show_co2;
float temp; //in Celsius
float humd; //in RH%
uint16_t co2color;

#include <RTClib.h>         // Real Time Clock

RTC_DS3231 rtc;

DateTime now;
char timeString[] = "hh:mm:ss";
char daysOfTheWeek[7][3] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

// button control variables
bool buttonA, buttonB;                  // Pulled-Up: pressed = LOW, released = HIGH
bool buttonPreviousA, buttonPreviousB;  // compare with new button state
byte buttonStateA, buttonStateB;        // 0 == stand-by, 1 == released, 2 == pressed
unsigned long buttonDebounceA, buttonDebounceB; // debouncing buttons

// timer variables
uint32_t currentMillis = 0, previousMillis = 0; // timer variable
uint32_t timerCO2 = 0, timerRTC = 0, timerBAT = 0, timerBUTTON = 0, timerDISPLAY = 0;
uint32_t measurementDelayCO2 = 2;  // in seconds
uint32_t showFPS = 0;
uint32_t currentFPS = 0, previousFPS = 0;

// miscellaneous variables
float batteryVoltage;

typedef struct Message
{
    // char co2Histogram[320];
    uint16_t co2color;
    float show_co2;
    float batteryVoltage;
    float temp;
    // char timeString[9];
    uint32_t showFPS;
} Message;

void calculateHistogram();
void displaySprite(char *co2Histogram,
                uint16_t &co2color,
                float &show_co2,
                float &batteryVoltage,
                float &temp,
                char *timeString,
                uint32_t &showFPS);

void buttonFunctionA();
void buttonFunctionB();

// Multicore Tasking
static const int graphics_queue_len = 5;
static QueueHandle_t graphics_queue;
// TaskHandle_t Task1;
// SemaphoreHandle_t Semaphore1;

void graphicsTask(void * parameter)
{
    Message temp_var2;
    while(1) {
        if (xQueueReceive(graphics_queue, (void *)&temp_var2, 0) == pdTRUE) {
            Serial.print(millis());
            Serial.print(" - Executing on core: ");
            Serial.print(xPortGetCoreID());
            Serial.print(" - temp_var: ");
            Serial.print(temp_var2.show_co2);
            Serial.print("\n");
        }
        vTaskDelay(1000);
    }
    vTaskDelete(NULL);
}

// button interrupt functions
void buttonPressA(void) {
  if (buttonStateA == 0 && currentMillis - buttonDebounceA >= 75) {
    buttonStateA = 2;
    buttonDebounceA = currentMillis;
  }
}

void buttonPressB(void) {
  if (buttonStateB == 0 && currentMillis - buttonDebounceB >= 75) {
    buttonStateB = 2;
    buttonDebounceB = currentMillis;
  }
}

// variadic template functions
void tftStringPrint(){}
template <typename T, typename... Types>
void tftStringPrint(T first, Types... other)
{
  tft.print(first);
  tftStringPrint(other...);
}

void setup()
{
    pinMode(PIN_POWER_ON, OUTPUT);      // use rechargeable battery to power device
    digitalWrite(PIN_POWER_ON, HIGH);

    Serial.begin(460800);
    if (!Serial) delay(200);       // give Serial some time

    pinMode(PIN_BUTTON_1, INPUT);
    pinMode(PIN_BUTTON_2, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_BUTTON_1), buttonPressA, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_BUTTON_2), buttonPressB, FALLING);

    tft.begin(); delay(200);
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);
    tft.setSwapBytes(true);
    tft.pushImage(0, 0, 320, 170, (uint16_t *)img_logo);
    // tft.setSwapBytes(false);
    vTaskDelay(1500);

    ledcSetup(TFT_CHANNEL, TFT_FREQUENCY, TFT_RESOLUTION);
    ledcAttachPin(PIN_LCD_BL, TFT_CHANNEL);
    ledcWrite(TFT_CHANNEL, TFT_DUTYCYCLE[7]);

    Wire.setPins(PIN_JST_SDA, PIN_JST_SCL);
    Wire.begin();

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    // tft.setFreeFont();
    tft.setTextSize(2);
    tft.setCursor(0, 0);
    tftStringPrint(" ", __DATE__, " - ", __TIME__, "\n");
    tftStringPrint(" CPU  = ", getCpuFrequencyMhz(), " MHz\n");
    tftStringPrint(" XTAL = ", getXtalFrequencyMhz(), " MHz\n");
    tftStringPrint(" APB  = ", getApbFrequency(), " Hz\n");
    int16_t yCursorPos = tft.getCursorY();
    tft.print(" SCD:\n RTC:\n  SD:");
    
    vTaskDelay(1000);
    tft.setCursor(0, yCursorPos);
    if (!sensorCO2.begin()) {
        tft.print(" SCD: ERROR\n");
    }
    else {
        sensorCO2.setMeasurementInterval(measurementDelayCO2);
        // sensorCO2.setTemperatureOffset(110);
        // sensorCO2.setAltitude(1043);
        sensorCO2.setAutoCalibration(0);
        // sensorCO2.readFirmwareVersion();
        tft.print(" SCD: READY\n");
    }
    vTaskDelay(1000);
    if (!rtc.begin()) {
        tft.print(" RTC: ERROR\n");
    }
    else {
        // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        rtc.disable32K();
        tft.print(" RTC: READY\n");
    }
    vTaskDelay(2000);

    // Message system_msg;
    // system_msg.co2Histogram[320];
    // system_msg.co2color;
    // system_msg.show_co2;
    // system_msg.batteryVoltage;
    // system_msg.temp;
    // system_msg.timeString[9];
    // system_msg.showFPS;


    for (int setOutOfBounds = 0; setOutOfBounds < 320; setOutOfBounds++) {
        co2Histogram[setOutOfBounds] = 128;
    }

    // Semaphore1 = xSemaphoreCreateMutex();

    graphics_queue = xQueueCreate(graphics_queue_len, sizeof(int));
    xTaskCreatePinnedToCore(graphicsTask,   // Function to implement the task
                            "graphics",     // Name of the task
                            1024,           // Stack size in words
                            NULL,           // Task input parameter
                            1,              // Priority of the task
                            NULL,           // Task handle
                            0);             // Core where the task should run
}

void loop()
{
    previousMillis = currentMillis;
    currentMillis = millis();

    // call button functionality
    if (currentMillis > timerBUTTON + 100) {
        buttonA = digitalRead(PIN_BUTTON_1);  // button A pressed == LOW
        buttonB = digitalRead(PIN_BUTTON_2);  // button B pressed == LOW
        buttonFunctionA();
        buttonFunctionB();
        timerBUTTON = currentMillis;
    }

    // read real time clock data
    if (currentMillis > timerRTC + 1000) {
        now = rtc.now();
        strncpy(timeString, "hh:mm:ss", sizeof(timeString) - 1);
        now.toString(timeString);
        timerRTC = currentMillis;
    }

    // read co2 sensor data
    if (currentMillis > timerCO2 + measurementDelayCO2 * 1000 && sensorCO2.getMeasurementStatus()) {
        old_co2 = co2;
        sensorCO2.getMeasurement(&co2, &temp, &humd);   // read co2 sensor
        calculateHistogram();
        timerCO2 = currentMillis;
    }
    else {
        show_co2 = old_co2 + ((co2 - old_co2) * (((float)currentMillis - (float)timerCO2) / ((float)measurementDelayCO2 * 1000)));
        // Serial.println(system_msg.show_co2);
    }
    

    // read battery voltage
    if (currentMillis > timerBAT + 1000) {
        batteryVoltage = (analogRead(PIN_BAT_VOLT) * 2 * 3.3 * 1000) / 4096000;
        timerBAT = currentMillis;
    }
    
    // draw on display (max 30 frames per second)
    if (currentMillis > timerDISPLAY + 33) {
        showFPS = 1000 / (currentMillis - previousMillis);
        // create instance and copy variables into queue
        Message temp_var;
        // strcpy(temp_var.co2Histogram, co2Histogram);
        temp_var.co2color = co2color;
        temp_var.show_co2 = show_co2;
        temp_var.batteryVoltage = batteryVoltage;
        temp_var.temp = temp;
        // strcpy(temp_var.timeString, timeString);
        temp_var.showFPS = showFPS;
        if (xQueueSend(graphics_queue, (void *)&temp_var, 10) != pdTRUE) {
            // Serial.println("ERROR: Could not put item on delay queue.");
          }
        displaySprite(co2Histogram,
                    co2color,
                    show_co2,
                    batteryVoltage,
                    temp,
                    timeString,
                    showFPS);
        timerDISPLAY = currentMillis;
    }
}

void calculateHistogram()
{
    // get color for co2 number
    uint8_t co2red, co2green, co2blue = 0;
    if (co2 > 1200) {
        co2red = 31;
        co2green = 0;
        if (co2 <= 2000) {
            co2green = ((800 - (co2 - 1200)) / 12.5) - 1;
        }
    }
    else if (co2 <= 1200) {
        co2red = 0;
        co2green = 63;
        if (co2 >= 400) {
            co2red = (((co2 - 400)) / 25) - 1;
        }
    }
    co2color = (co2red << 11) | (co2green << 5) | co2blue;

    // get co2 position for histogram.
    if (co2 < 400) {
        co2Histogram[0] = 0;
        averageCO2 += 400;
    }
    else if (co2 > 2000) {
        co2Histogram[0] = 127;
        averageCO2 += 2000;
    }
    else {
        co2Histogram[0] = (co2 - 400) * 0.08;
        averageCO2 += co2;
    }
    histogramDrawCounter++;
    // move the whole histogram one pixel to the right and
    // set the next point to the left side of it
    if (histogramDrawCounter >= histogramDrawInterval) {
        co2Histogram[1] = ((averageCO2 / histogramDrawCounter) - 400) * 0.079375;
        for (int i = 320 - 1; i > 1; i--) {
            co2Histogram[i] = co2Histogram[i - 1];
        }
        averageCO2 = 0;
        histogramDrawCounter = 0;
    }
}

void displaySprite(char *co2Histogram,
    uint16_t &co2color,
    float &show_co2,
    float &batteryVoltage,
    float &temp,
    char *timeString,
    uint32_t &showFPS)
{
    // background image
    background.createSprite(320, 170);
    // background.setSwapBytes(true);
    // background.pushImage(0, 0, 320, 170, (uint16_t *)img_logo);
    background.setSwapBytes(false);

    // frames-per-second counter
    fps.createSprite(2 * 14, 19);
    fps.setTextColor(TFT_WHITE);
    fps.setTextDatum(TR_DATUM);
    fps.drawNumber(showFPS, 28, 0, 4);
    fps.pushToSprite(&background, 2, 2, TFT_BLACK);
    fps.deleteSprite();
    
    // real time clock
    rtctime.createSprite(6 * 14 + 2 * 7, 19);
    rtctime.setTextColor(TFT_WHITE);
    rtctime.setTextDatum(TL_DATUM);
    rtctime.drawString(timeString, 0, 0, 4);
    rtctime.pushToSprite(&background, 220, 2, TFT_BLACK);
    rtctime.deleteSprite();

    // temperature indicator in celsius
    temperature.createSprite(3 * 14 + 7, 19);
    temperature.setTextColor(TFT_WHITE);
    temperature.setTextDatum(TR_DATUM);
    temperature.drawFloat(temp, 1, 49, 0, 4);
    temperature.pushToSprite(&background, 2, 148, TFT_BLACK);
    temperature.deleteSprite();

    // humidity indicator
    humidity.createSprite(3 * 14 + 7, 19);
    humidity.setTextColor(TFT_WHITE);
    humidity.setTextDatum(TR_DATUM);
    // humidity.drawFloat(humd, 1, 49, 0, 4);
    humidity.drawFloat(batteryVoltage, 2, 49, 0, 4);    // temporary, remove later
    humidity.pushToSprite(&background, 270, 148, TFT_BLACK);
    humidity.deleteSprite();

    // histogram showing co2 consistency over time (currently over 8 hours)
    histogram.createSprite(320, 127);
    // draw line for CO2 level histogram
    for (int i = 1; i < 320; i++) {
        if (co2Histogram[i] < 128) {
            int j = 0;
            if (co2Histogram[i - 1] != co2Histogram[i]) {
                // return +1 or -1 if next pixel is above or below previous
                j = (co2Histogram[i - 1] - co2Histogram[i]) / std::abs(co2Histogram[i - 1] - co2Histogram[i]);
            }
            histogram.drawLine(i, 127 - co2Histogram[i - 1] + j , i, 127 - co2Histogram[i], TFT_LIGHTGREY);
        }
    }
    histogram.fillCircle(0, 127 - co2Histogram[0], 2, TFT_RED);
    histogram.pushToSprite(&background, 0, 22, TFT_BLACK);
    histogram.deleteSprite();

    // carbondioxyd indicator
    co2value.createSprite(4 * 55, 75);
    co2value.setTextColor(co2color);
    co2value.setTextDatum(TR_DATUM);
    co2value.drawNumber(show_co2, 220, 0, 8);
    co2value.pushToSprite(&background, 50, 40, TFT_BLACK);
    co2value.deleteSprite();

    // write parts per million abbreviation next to co2 indicator
    ppmtext.createSprite(18, 7);
    ppmtext.setTextColor(TFT_WHITE);
    ppmtext.setTextDatum(TL_DATUM);
    ppmtext.drawString("ppm", 0, 0, 1);
    ppmtext.pushToSprite(&background, 270, 40, TFT_BLACK);
    ppmtext.deleteSprite();

    background.pushSprite(0, 0);
    background.deleteSprite();
}

void buttonFunctionA() {
  // when button is pressed, do stuff once
  if (buttonStateA == 2) {
    tftStringPrint("ButtonA: ", buttonStateA, "\n");
    // increase TFT screen brightness
    if (brightnessTFT < 8) {
        brightnessTFT++;
        ledcWrite(TFT_CHANNEL, TFT_DUTYCYCLE[brightnessTFT]);
    }
    buttonStateA = 1;    // prevent button spam
  }
  // when button is released, do stuff once
  else if (buttonA == HIGH && buttonStateA == 1) {
    tftStringPrint("ButtonA: ", buttonStateA, "\n");
    buttonStateA = 0;    // prevent button spam
  }
}

void buttonFunctionB() {
  // when button is pressed, do stuff once
  if (buttonStateB == 2) {
    tftStringPrint("ButtonB: ", buttonStateB, "\n");
    // decrease TFT screen brightness
    if (brightnessTFT > 0) {
        brightnessTFT--;
        ledcWrite(TFT_CHANNEL, TFT_DUTYCYCLE[brightnessTFT]);
    }
    buttonStateB = 1;    // prevent button spam
  }
  // when button is released, do stuff once
  else if (buttonB == HIGH && buttonStateB == 1) {
    tftStringPrint("ButtonB: ", buttonStateB, "\n");
    buttonStateB = 0;    // prevent button spam
  }
}