#include <Arduino.h>
#include <FastLED.h>
#include "Visualization.h"
#include "Streak.h"
#include "Ladder.h"
#include "Sparkle.h"

#define NUM_LEDS 360
#define ROWS 60
#define COLUMNS NUM_LEDS/ROWS
#define DATA_PIN 52

CRGB leds[NUM_LEDS];
CRGB off;

void clear();
void setAll();

#define NUM_STREAKS 4
#define NUM_LADDERS 4

CRGB pink = 0xFF0B20;
CRGB blue = 0x0BFFDD;
CRGB green = 0xB9FF0B;

Streak * pinkS[NUM_STREAKS];
Streak * blueS[NUM_STREAKS];
Streak * greenS[NUM_STREAKS];
Ladder * pinkL[NUM_STREAKS];
Ladder * blueL[NUM_STREAKS];
Ladder * greenL[NUM_STREAKS];

Sparkle * sparkle;

int active = 0;

void setup() {
  FastLED.setBrightness(32);
  Serial.begin(9600);
  off = 0x000000;
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  clear();
  FastLED.show();
  delay(2000);

  for(unsigned int i=0; i<NUM_STREAKS; i++) {
    pinkS[i] = new Streak(COLUMNS, ROWS, leds, pink);
    blueS[i] = new Streak(COLUMNS, ROWS, leds, blue);
    greenS[i] = new Streak(COLUMNS, ROWS, leds, green);
  }

  for(unsigned int i=0; i<NUM_LADDERS; i++) {
    pinkL[i] = new Ladder(COLUMNS, ROWS, leds, pink);
    blueL[i] = new Ladder(COLUMNS, ROWS, leds, blue);
    greenL[i] = new Ladder(COLUMNS, ROWS, leds, green);
  }

  sparkle = new Sparkle(COLUMNS, ROWS, leds, pink);
  // p2 = new Sparkle(COLUMNS, ROWS, leds, blue);
  // p3 = new Sparkle(COLUMNS, ROWS, leds, green);
}

void loop() {
    unsigned long currentTime = millis();
    clear();

    for(unsigned int i=0; i<NUM_STREAKS; i++) {
      pinkS[i]->display(currentTime);
      blueS[i]->display(currentTime);
      greenS[i]->display(currentTime);
    }
    //
    // for(unsigned int i=0; i<NUM_STREAKS; i++) {
    //   pinkL[i]->display(currentTime);
    //   blueL[i]->display(currentTime);
    //   greenL[i]->display(currentTime);
    // }

    sparkle->display(currentTime);

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
