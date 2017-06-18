#include <Arduino.h>
#include <FastLED.h>
#include "Visualization.h"

class Sparkle : public Visualization {
  private :
    int column;
    int length;
    unsigned long cycle;
    CRGB color;
    CRGB currentColor;

  public :
    Sparkle (int columns, int rows, CRGB * leds, CRGB color);

    void inititalize();

    void display (unsigned long currentTime);
};
