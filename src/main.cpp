#include "Arduino.h"
#include "TFT_eSPI.h" /* Please use the TFT library provided in the library. */

#include "pin_config.h"
#include "img_logo.h"

#define TFT_CHANNEL 0
#define TFT_FREQUENCY 2000
#define TFT_RESOLUTION 8

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite frame = TFT_eSprite(&tft);

unsigned long currentMillis = 0, previousMillis = 0; // timer variable
int showNumbers;


void setup()
{
    Serial.begin(115200);
    Serial.println("Hello T-Display-S3");

    tft.begin();
    tft.setRotation(3);
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_BLACK);
    tft.pushImage(0, 0, 320, 170, (uint16_t *)img_logo);
    delay(2000);
    tft.fillScreen(TFT_BLACK);

    ledcSetup(TFT_CHANNEL, TFT_FREQUENCY, TFT_RESOLUTION);
    ledcAttachPin(PIN_LCD_BL, TFT_CHANNEL);
    ledcWrite(TFT_CHANNEL, 127);
}

void loop()
{
    frame.createSprite(320, 170);
    frame.fillSprite(TFT_BLACK);
    
    previousMillis = currentMillis;
    currentMillis = millis();
    unsigned long showFPS = 1000 / (currentMillis - previousMillis);
    frame.setTextColor(TFT_WHITE, TFT_NAVY);
    frame.setTextDatum(TL_DATUM);
    frame.drawString("FPS:", 230, 150, 4);
    frame.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    frame.setTextDatum(TR_DATUM);
    frame.drawNumber(showFPS, 320, 150, 4);

    showNumbers++;
    if (showNumbers > 999) showNumbers = 0;
    frame.setTextColor(TFT_WHITE, TFT_NAVY);
    frame.setTextDatum(TL_DATUM);
    frame.drawString("N:", 0, 0, 4);
    frame.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    frame.setTextDatum(TR_DATUM);
    frame.drawNumber(showNumbers, 70, 0, 4);

    frame.pushSprite(0, 0);
    frame.deleteSprite();
}