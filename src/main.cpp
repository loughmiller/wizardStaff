#include <Arduino.h>
#include <FastLED.h>
#include "Visualization.h"
#include "Streak.h"
#include "Ladder.h"

#define NUM_LEDS 360
#define ROWS 60
#define COLUMNS NUM_LEDS/ROWS
#define DATA_PIN 52

CRGB leds[NUM_LEDS];
CRGB off;

void clear();
void setAll();

#define NUM_STREAKS 4

CRGB pink = 0x220102;
//Streak * streaks[NUM_STREAKS];
Ladder * pinkL[NUM_STREAKS];
Ladder * blueL[NUM_STREAKS];
Ladder * purpleL[NUM_STREAKS];

void setup() {
  Serial.begin(9600);
  off = 0x000000;
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  clear();
  FastLED.show();
  delay(2000);

  // for(unsigned int i=0; i<NUM_STREAKS; i++) {
  //   streaks[i] = new Streak(COLUMNS, ROWS, leds, pink);
  // }
  for(unsigned int i=0; i<NUM_STREAKS; i++) {
    pinkL[i] = new Ladder(COLUMNS, ROWS, leds, pink);
    blueL[i] = new Ladder(COLUMNS, ROWS, leds, 0x000022);
    purpleL[i] = new Ladder(COLUMNS, ROWS, leds, 0x080010);
  }
}

void loop() {
    unsigned long currentTime = millis();
    clear();
    // for(unsigned int i=0; i<NUM_STREAKS; i++) {
    //   streaks[i]->display(currentTime);
    // }
    for(unsigned int i=0; i<NUM_STREAKS; i++) {
      pinkL[i]->display(currentTime);
      blueL[i]->display(currentTime);
      purpleL[i]->display(currentTime);
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
