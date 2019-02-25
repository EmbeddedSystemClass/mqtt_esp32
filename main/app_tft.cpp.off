#include "app_tft.h"

#include <TFT_eSPI.h> // Hardware-specific library

TFT_eSPI tft = TFT_eSPI();       // Invoke custom library
boolean SwitchOn = false;
// Comment out to stop drawing black spots
#define BLACK_SPOT 1

// Switch position and size
#define FRAME_X 100
#define FRAME_Y 64
#define FRAME_W 120
#define FRAME_H 50

// Red zone size
#define REDBUTTON_X FRAME_X
#define REDBUTTON_Y FRAME_Y
#define REDBUTTON_W (FRAME_W/2)
#define REDBUTTON_H FRAME_H

// Green zone size
#define GREENBUTTON_X (REDBUTTON_X + REDBUTTON_W)
#define GREENBUTTON_Y FRAME_Y
#define GREENBUTTON_W (FRAME_W/2)
#define GREENBUTTON_H FRAME_H


#include "TouchScreen.h"

#define YP GPIO_NUM_32  // TFT_CS must be an analog pin, use "An" notation! 15
#define XM GPIO_NUM_33  // TFT_DC must be an analog pin, use "An" notation! 4
#define XP TFT_D0   // TFT_D0 can be a digital pin 12
#define YM TFT_D1   // TFT_D1 can be a digital pin 13

const int TS_LEFT=930,TS_RT=170,TS_TOP=100,TS_BOT=930;

// For better pressure precision, we need to know the resistance
// between X+ and X- Use any multimeter to read it
// For the one we're using, its 300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

void tft_handler(void* pvParameters)
{
  static const char *TAG = "app_tft";

  analogReadResolution(10);
 
  tft.init();

  // Set the rotation before we calibrate
  tft.setRotation(0);

  // clear screen
  tft.fillScreen(TFT_BLUE);
  redBtn();

  while(1) {
    uint16_t x, y;
    
    pinMode(TFT_CS, INPUT);
    pinMode(TFT_DC, INPUT);
    TSPoint p = ts.getPoint();
    // ESP_LOGI(TAG, "X = %d\tY=%d\tP=%d\n", p.x, p.y, p.z);
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, LOW);
    pinMode(TFT_DC, OUTPUT);
    digitalWrite(TFT_DC, HIGH);
    
    // See if there's any touch data for us
    if (p.z > ts.pressureThreshhold)
      {
        y = map(p.y, TS_TOP, TS_BOT, 0, tft.height());
        x = map(p.x, TS_RT, TS_LEFT, 0, tft.width());
        
        // Draw a block spot to show where touch was calculated to be
#ifdef BLACK_SPOT
        tft.fillCircle(x, y, 2, TFT_BLACK);
#endif
        
        if (SwitchOn)
          {
            if ((x > REDBUTTON_X) && (x < (REDBUTTON_X + REDBUTTON_W))) {
              if ((y > REDBUTTON_Y) && (y <= (REDBUTTON_Y + REDBUTTON_H))) {
                ESP_LOGI(TAG, "Red btn hit");
                digitalWrite(GPIO_NUM_16, LOW);

                redBtn();
              }
            }
          }
        else //Record is off (SwitchOn == false)
          {
            if ((x > GREENBUTTON_X) && (x < (GREENBUTTON_X + GREENBUTTON_W))) {
              if ((y > GREENBUTTON_Y) && (y <= (GREENBUTTON_Y + GREENBUTTON_H))) {
                ESP_LOGI(TAG, "Green btn hit");
                digitalWrite(GPIO_NUM_16, HIGH);
                greenBtn();
              }
            }
          }
        
        ESP_LOGI(TAG, "%d\n", SwitchOn);
      }
    vTaskDelay(100 / portTICK_PERIOD_MS);

  }
  
}

  void drawFrame()
{
  tft.drawRect(FRAME_X, FRAME_Y, FRAME_W, FRAME_H, TFT_BLACK);
}

// Draw a red button
void redBtn()
{
  tft.fillRect(REDBUTTON_X, REDBUTTON_Y, REDBUTTON_W, REDBUTTON_H, TFT_RED);
  tft.fillRect(GREENBUTTON_X, GREENBUTTON_Y, GREENBUTTON_W, GREENBUTTON_H, TFT_DARKGREY);
  drawFrame();
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("ON", GREENBUTTON_X + (GREENBUTTON_W / 2), GREENBUTTON_Y + (GREENBUTTON_H / 2));
  SwitchOn = false;
}

// Draw a green button
void greenBtn()
{
  tft.fillRect(GREENBUTTON_X, GREENBUTTON_Y, GREENBUTTON_W, GREENBUTTON_H, TFT_GREEN);
  tft.fillRect(REDBUTTON_X, REDBUTTON_Y, REDBUTTON_W, REDBUTTON_H, TFT_DARKGREY);
  drawFrame();
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("OFF", REDBUTTON_X + (REDBUTTON_W / 2) + 1, REDBUTTON_Y + (REDBUTTON_H / 2));
  SwitchOn = true;
}
