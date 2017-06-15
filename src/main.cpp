#include <Arduino.h>
#include "FastLED.h"

#define NUM_LEDS 360
#define ROWS 60
#define COLUMS NUM_LEDS/ROWS
#define DATA_PIN 52

CRGB leds[NUM_LEDS];
CRGB off;

void clear();
void setAll();
int xy2Pos(int x, int y);


#define NUM_STREAKS 12

class Streak {
  private :
    int id;
    int frame;
    unsigned long nextTime;
    int interval;
    int column;
    int length;
    CRGB color;

  public :
    Streak (int id, CRGB color) {
      this->id = id;
      this->frame = 0;
      this->length = 0;
      this->color = color;
      this->color.maximizeBrightness();
    }

    void inititalize() {
      this->frame = 0;
      this->nextTime = 0;
      this->interval = 20 + random8(60);
      this->column = random8(6);
      this->length = random8(8, 16);
    }

    void display (unsigned long currentTime) {
      int currentFrame = this->frame % (ROWS + this->length);

      if (currentFrame == 0) {
        this->inititalize();
      }

      if (currentTime > this->nextTime) {
        // Serial.print(this->id);
        // Serial.print(": ");
        // Serial.println(currentTime);
        this->frame++;
        this->nextTime = currentTime + this->interval;
      }

      int y = currentFrame;
      int pos;
      for (int i=0; i<this->length; i++) {
        if ((y - i >= 0) && (y - i < ROWS)) {
          pos = xy2Pos(this->column, y - i);
          leds[pos] = this->color;
          leds[pos].fadeToBlackBy((256 / this->length) * i);
        }
      }
    }
};

CRGB pink = 0xFF0813;
Streak * streaks[NUM_STREAKS];

void setup() {
  Serial.begin(9600);
  off = 0x000000;
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  clear();
  FastLED.show();
  delay(2000);

  for(unsigned int i=0; i<NUM_STREAKS; i++) {
    streaks[i] = new Streak(i, pink);
  }
}

void loop() {
    unsigned long currentTime = millis();
    clear();
    for(unsigned int i=0; i<NUM_STREAKS; i++) {
      streaks[i]->display(currentTime);
    }
    FastLED.show();
}

void setAll(CRGB color) {
  for (int i=0; i<NUM_LEDS; i++) {
    leds[i] = color;
  }
}

void clear() {
  setAll(off);
}

int xy2Pos(int x, int y) {
  int pos = x * ROWS;
  if (x % 2 == 0) {
    pos = pos + y;
  } else {
    pos = pos + ((ROWS - 1) - y);
  }

  return pos;
}
