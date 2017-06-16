#include <Arduino.h>
#include "FastLED.h"

class Streak {
  private :
    int columns;
    int rows;
    CRGB * leds;
    int frame;
    unsigned long nextTime;
    int interval;
    int column;
    int length;
    CRGB color;

  public :
    Streak (int columns, int rows, CRGB * leds, CRGB color);

    void inititalize();

    void display (unsigned long currentTime);

    int xy2Pos (int x, int y);
};
