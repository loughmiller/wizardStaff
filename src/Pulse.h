#include <Arduino.h>
#include <FastLED.h>
#include "Visualization.h"

class Pulse : public Visualization {
  private :
    int column;
    int length;
    int interval;
    CRGB color;

  public :
    Pulse(int columns, int rows, CRGB * leds, CRGB color);

    void inititalize();

    void display(unsigned long currentTime);
};
