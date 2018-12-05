// Touch screen library with X Y and Z (pressure) readings as well
// as oversampling to avoid 'bouncing'
// (c) ladyada / adafruit
// Code under MIT License
#include "driver/gpio.h"
#include "driver/adc.h"

#include "TouchScreen.h"

// increase or decrease the touchscreen oversampling. This is a little different than you make think:
// 1 is no oversampling, whatever data we get is immediately returned
// 2 is double-sampling and we only return valid data if both points are the same
// 3+ uses insert sort to get the median value.
// We found 2 is precise yet not too slow so we suggest sticking with it!

#define NUMSAMPLES 2

TSPoint::TSPoint(void) {
  x = y = 0;
}

TSPoint::TSPoint(int16_t x0, int16_t y0, int16_t z0) {
  x = x0;
  y = y0;
  z = z0;
}

bool TSPoint::operator==(TSPoint p1) {
  return  ((p1.x == x) && (p1.y == y) && (p1.z == z));
}

bool TSPoint::operator!=(TSPoint p1) {
  return  ((p1.x != x) || (p1.y != y) || (p1.z != z));
}

#if (NUMSAMPLES > 2)
static void insert_sort(int array[], uint8_t size) {
  uint8_t j;
  int save;
  
  for (int i = 1; i < size; i++) {
    save = array[i];
    for (j = i; j >= 1 && save < array[j - 1]; j--)
      array[j] = array[j - 1];
    array[j] = save; 
  }
}
#endif


uint16_t analogRead(gpio_num_t g)
{
  adc1_config_width(ADC_WIDTH_BIT_10);
  return(adc1_get_raw(ADC1_CHANNEL_5)); //FIXME

}

TSPoint TouchScreen::getPoint(void) {
  int x, y, z;
  int samples[NUMSAMPLES];
  uint8_t i, valid;

  valid = 1;

  gpio_set_direction(_yp, GPIO_MODE_INPUT);
  gpio_set_direction(_ym, GPIO_MODE_INPUT);
  gpio_set_direction(_xp, GPIO_MODE_OUTPUT);
  gpio_set_direction(_xm, GPIO_MODE_OUTPUT);

  gpio_set_level(_xp, 1); //  gpio_set_level(DS_GPIO,1);
  gpio_set_level(_xm, 0); //  gpio_set_level(DS_GPIO,0);

#ifdef __arm__
  delayMicroseconds(20); // Fast ARM chips need to allow voltages to settle
#endif

   for (i=0; i<NUMSAMPLES; i++) {
     samples[i] = analogRead(_yp);
   }

#if NUMSAMPLES > 2
   insert_sort(samples, NUMSAMPLES);
#endif
#if NUMSAMPLES == 2
   // Allow small amount of measurement noise, because capacitive
   // coupling to a TFT display's signals can induce some noise.
   if (samples[0] - samples[1] < -4 || samples[0] - samples[1] > 4) {
     valid = 0;
   } else {
     samples[1] = (samples[0] + samples[1]) >> 1; // average 2 samples
   }
#endif

   x = (1023-samples[NUMSAMPLES/2]);

   gpio_set_direction(_xp, GPIO_MODE_INPUT);
   gpio_set_direction(_xm, GPIO_MODE_INPUT);
   gpio_set_direction(_yp, GPIO_MODE_OUTPUT);
   gpio_set_direction(_ym, GPIO_MODE_OUTPUT);

#if defined (USE_FAST_PINIO)
   *ym_port &= ~ym_pin;
   *yp_port |= yp_pin;
#else
   gpio_set_level(_ym, 0);
   gpio_set_level(_yp, 1);
#endif

  
#ifdef __arm__
   delayMicroseconds(20); // Fast ARM chips need to allow voltages to settle
#endif

   for (i=0; i<NUMSAMPLES; i++) {
     samples[i] = analogRead(_xm);
   }

#if NUMSAMPLES > 2
   insert_sort(samples, NUMSAMPLES);
#endif
#if NUMSAMPLES == 2
   // Allow small amount of measurement noise, because capacitive
   // coupling to a TFT display's signals can induce some noise.
   if (samples[0] - samples[1] < -4 || samples[0] - samples[1] > 4) {
     valid = 0;
   } else {
     samples[1] = (samples[0] + samples[1]) >> 1; // average 2 samples
   }
#endif

   y = (1023-samples[NUMSAMPLES/2]);

   // Set X+ to ground
   // Set Y- to VCC
   // Hi-Z X- and Y+
   gpio_set_direction(_xp, GPIO_MODE_OUTPUT);
   gpio_set_direction(_yp, GPIO_MODE_INPUT);

   gpio_set_level(_xp, 0);
   gpio_set_level(_ym, 1); 
  
   int z1 = analogRead(_xm); 
   int z2 = analogRead(_yp);

   if (_rxplate != 0) {
     // now read the x 
     float rtouch;
     rtouch = z2;
     rtouch /= z1;
     rtouch -= 1;
     rtouch *= x;
     rtouch *= _rxplate;
     rtouch /= 1024;
     
     z = rtouch;
   } else {
     z = (1023-(z2-z1));
   }

   if (! valid) {
     z = 0;
   }

   return TSPoint(x, y, z);
}

TouchScreen::TouchScreen(gpio_num_t xp, gpio_num_t yp, gpio_num_t xm, gpio_num_t ym,
			 uint16_t rxplate=0) {
  _yp = yp;
  _xm = xm;
  _ym = ym;
  _xp = xp;
  _rxplate = rxplate;


  pressureThreshhold = 10;
}

int TouchScreen::readTouchX(void) {
   gpio_set_direction(_yp, GPIO_MODE_INPUT);
   gpio_set_direction(_ym, GPIO_MODE_INPUT);
   gpio_set_level(_yp, 0);
   gpio_set_level(_ym, 0);
   
   gpio_set_direction(_xp, GPIO_MODE_OUTPUT);
   gpio_set_level(_xp, 1);
   gpio_set_direction(_xm, GPIO_MODE_OUTPUT);
   gpio_set_level(_xm, 0);
   
   return (1023-analogRead(_yp));
}


int TouchScreen::readTouchY(void) {
   gpio_set_direction(_xp, GPIO_MODE_INPUT);
   gpio_set_direction(_xm, GPIO_MODE_INPUT);
   gpio_set_level(_xp, 0);
   gpio_set_level(_xm, 0);
   
   gpio_set_direction(_yp, GPIO_MODE_OUTPUT);
   gpio_set_level(_yp, 1);
   gpio_set_direction(_ym, GPIO_MODE_OUTPUT);
   gpio_set_level(_ym, 0);
   
   return (1023-analogRead(_xm));
}


uint16_t TouchScreen::pressure(void) {
  // Set X+ to ground
  gpio_set_direction(_xp, GPIO_MODE_OUTPUT);
  gpio_set_level(_xp, 0);
  
  // Set Y- to VCC
  gpio_set_direction(_ym, GPIO_MODE_OUTPUT);
  gpio_set_level(_ym, 1); 
  
  // Hi-Z X- and Y+
  gpio_set_level(_xm, 0);
  gpio_set_direction(_xm, GPIO_MODE_INPUT);
  gpio_set_level(_yp, 0);
  gpio_set_direction(_yp, GPIO_MODE_INPUT);
  
  int z1 = analogRead(_xm); 
  int z2 = analogRead(_yp);

  if (_rxplate != 0) {
    // now read the x 
    float rtouch;
    rtouch = z2;
    rtouch /= z1;
    rtouch -= 1;
    rtouch *= readTouchX();
    rtouch *= _rxplate;
    rtouch /= 1024;
    
    return rtouch;
  } else {
    return (1023-(z2-z1));
  }
}
