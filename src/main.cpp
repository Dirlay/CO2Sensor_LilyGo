#include "Arduino.h"
#include "TFT_eSPI.h" /* Please use the TFT library provided in the library. */

#include <SCD30_I2C.h>
#include <RTClib.h>

#include "pin_config.h"
#include "img_logo.h"

#include "AnimatedGIF.h"
#include "breath.gif"

#define TFT_CHANNEL 0
#define TFT_FREQUENCY 2000
#define TFT_RESOLUTION 8
uint8_t TFT_DUTYCYCLE = 127;

TFT_eSPI tft = TFT_eSPI();

TFT_eSprite background = TFT_eSprite(&tft);
TFT_eSprite fps = TFT_eSprite(&tft);
TFT_eSprite rtctime = TFT_eSprite(&tft);
TFT_eSprite temperature = TFT_eSprite(&tft);
TFT_eSprite humidity = TFT_eSprite(&tft);
TFT_eSprite co2value = TFT_eSprite(&tft);
TFT_eSprite ppmtext = TFT_eSprite(&tft);

uint8_t textWidth = 5, textHeight = 7, textColon = 2;
int showNumbers;
int breathFrame = 0;

SCD30_I2C sensorCO2;

float co2;  //in ppm
float temp; //in C
float humd; //in RH%

RTC_DS3231 rtc;

DateTime now;
char daysOfTheWeek[7][3] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

AnimatedGIF gif;

unsigned long currentMillis = 0, previousMillis = 0; // timer variable
unsigned long timerCO2 = 0, timerRTC = 0;

void displaySprite();
void GIFDraw();

void setup()
{
    Serial.begin(115200);
    // delay(500);

    tft.begin();
    tft.setRotation(3);
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_BLACK);
    tft.pushImage(0, 0, 320, 170, (uint16_t *)img_logo);
    // delay(2000);

    ledcSetup(TFT_CHANNEL, TFT_FREQUENCY, TFT_RESOLUTION);
    ledcAttachPin(PIN_LCD_BL, TFT_CHANNEL);
    ledcWrite(TFT_CHANNEL, TFT_DUTYCYCLE);

    
    Wire.setPins(PIN_JST_SDA, PIN_JST_SCL);
    Wire.begin();

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    // tft.setFreeFont();
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("SCD: ");
    tft.setCursor(10, tft.getCursorY());
    tft.print("RTC: ");
    // delay(1000);

    int infoCursorPos = tft.getCursorX();
    tft.setCursor(infoCursorPos, 10);
    if (!sensorCO2.begin()) {
        tft.println("ERROR");
    }
    else {
        // sensorCO2.setMeasurementInterval(5);
        // sensorCO2.setTemperatureOffset(110);
        // sensorCO2.setAltitude(1043);
        sensorCO2.setAutoCalibration(0);
        sensorCO2.readFirmwareVersion();
        tft.println("READY");
    }
    // delay(1000);

    tft.setCursor(infoCursorPos, tft.getCursorY());
    if (!rtc.begin()) {
        tft.println("ERROR");
    }
    else {
        // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        rtc.disable32K();
        tft.println("READY");
    }
    delay(1000);
    
    gif.begin(GIF_PALETTE_RGB565_BE); // big endian pixels
}

void loop()
{
    previousMillis = currentMillis;
    currentMillis = millis();
    
    showNumbers++;
    if (showNumbers > 999) showNumbers = 0;

    if (currentMillis > timerCO2 + 5000 && sensorCO2.getMeasurementStatus()) {
        sensorCO2.getMeasurement(&co2, &temp, &humd);
        // Serial.println(co2);
        timerCO2 = currentMillis;
    }

    if (currentMillis > timerRTC + 1000 ) {
        now = rtc.now();
        timerRTC = currentMillis;
    }

    displaySprite();
}

void displaySprite()
{
    background.createSprite(320, 170);
    // background.setSwapBytes(true);
    breathFrame < 150 ? breathFrame++ : breathFrame = 0;
    if (gif.open("/breath.gif", GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
        tft.startWrite();
        gif.playFrame(false, NULL);
        gif.close();
        tft.endWrite();
    }
    else [
        Serial.println("GIF ERROR!");
    ]
    // background.setSwapBytes(false);

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

void GIFDraw(GIFDRAW *pDraw)
{
    uint8_t *s;
    uint16_t *d, *usPalette, usTemp[320];
    int x, iWidth;
    int startx, starty;
    int bRotate;
    PRIVATE *pPriv = (PRIVATE *)pDraw->pUser;

    switch (pPriv->angle) {
      case 0:
        bRotate = 0;
        startx = pPriv->xoff + pDraw->iX;
        starty = pPriv->yoff + pDraw->iY + pDraw->y;
      break;
      case 1: // 90
        bRotate = 1;
        startx = pPriv->xoff + pDraw->iX + pDraw->y;
        starty = pPriv->yoff + pDraw->iY;
      break;
      case 2: // 180
        bRotate = 0;
        startx = pPriv->xoff + pDraw->iX;
        starty = pPriv->yoff + pDraw->iHeight - pDraw->y;
      break;
      case 3: // 270
        bRotate = 1;
        startx = pPriv->xoff + pDraw->iX + (pDraw->iHeight - pDraw->y);
        starty = pPriv->yoff + pDraw->iY;
      break;
    }
    usPalette = pDraw->pPalette;
    iWidth = pDraw->iWidth;
    if (iWidth > DISPLAY_WIDTH)
       iWidth = DISPLAY_WIDTH;
    s = pDraw->pPixels;
    if (pDraw->ucDisposalMethod == 2) // restore to background color
    {
      for (x=0; x<iWidth; x++)
      {
        if (s[x] == pDraw->ucTransparent)
           s[x] = pDraw->ucBackground;
      }
      pDraw->ucHasTransparency = 0;
    }
    // Apply the new pixels to the main image
    if (pDraw->ucHasTransparency) // if transparency used
    {
      uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
      int x, iCount;
      pEnd = s + iWidth;
      x = 0;
      iCount = 0; // count non-transparent pixels
      while(x < iWidth)
      {
        c = ucTransparent-1;
        d = usTemp;
        while (c != ucTransparent && s < pEnd)
        {
          c = *s++;
          if (c == ucTransparent) // done, stop
          {
            s--; // back up to treat it like transparent
          }
          else // opaque
          {
             *d++ = usPalette[c];
             iCount++;
          }
        } // while looking for opaque pixels
        if (iCount) // any opaque pixels?
        {
          if (bRotate)
            spilcdSetPosition(&lcd, startx, starty + x, 1, iCount, DRAW_TO_LCD);
          else
            spilcdSetPosition(&lcd, startx+x, starty, iCount, 1, DRAW_TO_LCD);
          spilcdWriteDataBlock(&lcd, (uint8_t *)usTemp, iCount*2, DRAW_TO_LCD);
          x += iCount;
          iCount = 0;
        }
        // no, look for a run of transparent pixels
        c = ucTransparent;
        while (c == ucTransparent && s < pEnd)
        {
          c = *s++;
          if (c == ucTransparent)
             iCount++;
          else
             s--;
        }
        if (iCount)
        {
          x += iCount; // skip these
          iCount = 0;
        }
      }
    }
    else
    {
      s = pDraw->pPixels;
      // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
      for (x=0; x<iWidth; x++)
        usTemp[x] = usPalette[*s++];
      if (bRotate)
        spilcdSetPosition(&lcd, startx, starty, 1, iWidth, DRAW_TO_LCD);
      else
        spilcdSetPosition(&lcd, startx, starty, iWidth, 1, DRAW_TO_LCD);
      spilcdWriteDataBlock(&lcd, (uint8_t *)usTemp, iWidth*2, DRAW_TO_LCD);
    }
} /* GIFDraw() */