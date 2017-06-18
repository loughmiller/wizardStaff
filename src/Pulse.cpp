#include "Pulse.h"

Pulse::Pulse (int columns, int rows, CRGB * leds, CRGB color)
: Visualization(columns, rows, leds)
{
  this->color = color;
  this->color.maximizeBrightness();
  this->interval = 4;
  this->nextTime = 0;
}

void Pulse::display (unsigned long currentTime) {
  int currentFrame = this->frame % 144;

  if (currentTime > this->nextTime) {
    // Serial.print(this->id);
    // Serial.print(": ");
    // Serial.println(abs(currentFrame - 32)+1);
    this->frame++;
    this->nextTime = currentTime + this->interval;
    FastLED.setBrightness(abs(currentFrame - 72)+8);
  }

  for (int i=currentFrame%4; i<this->rows*this->columns; i=i+4) {
    leds[i] = this->color;
  }
}
