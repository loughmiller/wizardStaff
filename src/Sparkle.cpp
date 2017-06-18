#include "Sparkle.h"

Sparkle::Sparkle (int columns, int rows, CRGB * leds, CRGB color)
: Visualization(columns, rows, leds)
{
  this->cycle = 0;
  this->length = 0;
  this->color = color;
  this->color.maximizeBrightness();
}

void Sparkle::display (unsigned long currentTime) {
  int i = 0;
  while (i < this->rows*this->columns) {
    this->leds[i] = this->color;
    i = i + random(93);
  }
}
