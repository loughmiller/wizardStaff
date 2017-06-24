#include <Arduino.h>
#include <FastLED.h>
#include "Visualization.h"
#include "Streak.h"
#include "Ladder.h"
#include "Sparkle.h"
#include "Pulse.h"

#define NUM_LEDS 358
#define ROWS 45
#define COLUMNS 6
#define DATA_PIN 52

CRGB leds[NUM_LEDS];
CRGB off;

void clear();
void setAll();

#define NUM_STREAKS 8
#define NUM_LADDERS 3

CRGB pink = 0xFF0B20;
CRGB blue = 0x0BFFDD;
CRGB green = 0xB9FF0B;

Streak * pinkS[NUM_STREAKS];
Streak * blueS[NUM_STREAKS];
Streak * greenS[NUM_STREAKS];
Ladder * pinkL[NUM_STREAKS];
Ladder * blueL[NUM_STREAKS];
Ladder * greenL[NUM_STREAKS];

Sparkle * s1;
Sparkle * s2;

Pulse * pulseHeadPiece;

int active = 0;

void setup() {
  FastLED.setBrightness(64);
  Serial.begin(9600);
  off = 0x000000;
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  clear();
  FastLED.show();
  delay(2000);

  for(unsigned int i=0; i<NUM_STREAKS; i++) {
    greenS[i] = new Streak(COLUMNS, ROWS, leds, green);
    blueS[i] = new Streak(COLUMNS, ROWS, leds, blue);
    pinkS[i] = new Streak(COLUMNS, ROWS, leds, pink);
  }

  for(unsigned int i=0; i<NUM_LADDERS; i++) {
    greenL[i] = new Ladder(COLUMNS, ROWS, leds, green);
    blueL[i] = new Ladder(COLUMNS, ROWS, leds, blue);
    pinkL[i] = new Ladder(COLUMNS, ROWS, leds, pink);
  }

  s1 = new Sparkle(1, NUM_LEDS, leds, blue, 201);
  //s2 = new Sparkle(COLUMNS, ROWS, leds, green, 421);

  pulseHeadPiece = new Pulse(270, 88, leds, pink);
}

void loop() {
    unsigned long currentTime = millis();
    clear();

    pulseHeadPiece->display(currentTime);

    s1->display();

    for(unsigned int i=0; i<NUM_STREAKS; i++) {
      pinkS[i]->display(currentTime);
//      blueS[i]->display(currentTime);
//      greenS[i]->display(currentTime);
    }
    //
    // for(unsigned int i=0; i<NUM_STREAKS; i++) {
    //   pinkL[i]->display(currentTime);
    //   blueL[i]->display(currentTime);
    //   greenL[i]->display(currentTime);
    // }
    //

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
