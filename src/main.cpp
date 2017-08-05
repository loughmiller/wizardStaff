#include <Arduino.h>
#include <FastLED.h>
#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <Visualization.h>
#include <Streak.h>
#include <Sparkle.h>
#include <SoundReaction.h>
#include <TeensyAudioFFT.h>

#define NUM_LEDS 358
#define ROWS 45
#define COLUMNS 6
#define SENSOR_ACTIVATE_PIN 0
#define SENSOR_LED_PIN 16
#define DISPLAY_LED_PIN 23

const int AUDIO_INPUT_PIN = 14;         // Input ADC pin for audio data.

CRGB leds[NUM_LEDS];
CRGB off = 0x000000;

#define HEADPIECE_START 270
#define HEADPIECE_END 358


// FUNTION DEFINITIONS
void clear();
void setAll(CRGB color);
CRGB readColor();
void displaySoundReactiveHeadpiece(unsigned long currentTime, CRGB offColor, CRGB onColor);
uint8_t RGBtoHSV(float r, float g, float b);
CRGB maximizeSaturation(float r, float g, float b);
void defaultAllColors();
void changeAllColors(CRGB color);

#define NUM_STREAKS 5
#define NUM_LADDERS 3

#define pinkHEX 0xFF0B20
#define blueHEX 0x0BFFDD
CRGB pink = pinkHEX;
CRGB blue = blueHEX;
CRGB green = 0xB9FF0B;

Streak * pinkS[NUM_STREAKS];
Streak * blueS[NUM_STREAKS];
Streak * greenS[NUM_STREAKS];

Sparkle * s1;
Sparkle * s2;

SoundReaction * soundReaction;

#define STOLEN_COLOR_INTERVAL 20000
unsigned long stolenColorDeadline = 0;

// COLOR SENSOR
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

// our RGB -> eye-recognized gamma color
byte gammatable[256];

void setup() {
  delay(2000);

  Serial.begin(38400);
  Serial.println("setup started");

  randomSeed(analogRead(14));

  // SETUP LEDS
  FastLED.addLeds<NEOPIXEL, DISPLAY_LED_PIN>(leds, NUM_LEDS).setCorrection( 0xFFD08C );;
  FastLED.setBrightness(48);

  // INDICATE BOOT SEQUENCE
  for (int i=HEADPIECE_START; i<HEADPIECE_END; i++) {
    leds[i] = 0x0F0F0F;
  }
  FastLED.show();

  // COLOR SENSOR
  pinMode(SENSOR_LED_PIN, OUTPUT);
  pinMode(SENSOR_ACTIVATE_PIN, INPUT);

  if (tcs.begin()) {
    Serial.println("Found sensor");
    digitalWrite(SENSOR_LED_PIN, LOW);
  } else {
    Serial.println("No TCS34725 found ... check your connections");
    for (int i=HEADPIECE_START; i<HEADPIECE_END; i++) {
      leds[i] = 0x330000;
    }
    FastLED.show();
    while (1); // halt!
  }

  // AUDIO setup
  TeensyAudioFFTSetup(AUDIO_INPUT_PIN);
  samplingBegin();

  for (int i=0; i<256; i++) {
    float x = i;
    x /= 255;
    x = pow(x, 2.5);
    x *= 255;

    gammatable[i] = x;
    // Serial.println(gammatable[i]);
  }

  // DISPLAY STUFF
  clear();
  FastLED.show();

  for(unsigned int i=0; i<NUM_STREAKS; i++) {
    greenS[i] = new Streak(COLUMNS, ROWS, leds, green);
    blueS[i] = new Streak(COLUMNS, ROWS, leds, blue);
    pinkS[i] = new Streak(COLUMNS, ROWS, leds, pink);
  }

  s1 = new Sparkle(1, NUM_LEDS, leds, 0xFFFFFF, 201);

  soundReaction = new SoundReaction(HEADPIECE_START, HEADPIECE_END, leds, pink, blue);

  Serial.println("setup complete");
}

void loop() {
  CRGB color;
  clear();  // this just sets the array, no reason it can't be at the top

  // Serial.println(touchRead(A3));

  if (touchRead(A3) > 1900) {
    Serial.println("Read Color");
    color = readColor();
    setAll(color);
    FastLED.delay(2000);

    stolenColorDeadline = millis() + STOLEN_COLOR_INTERVAL;
    changeAllColors(color);
  }

  unsigned long currentTime = millis();

  if (stolenColorDeadline > 0 && currentTime > stolenColorDeadline) {
    stolenColorDeadline = 0;
    defaultAllColors();
  }

  float intensity = readRelativeIntensity(currentTime, 2, 4);
  soundReaction->display(intensity);

  for(unsigned int i=0; i<NUM_STREAKS; i++) {
    pinkS[i]->display(currentTime);
    blueS[i]->display(currentTime);
    greenS[i]->display(currentTime);
  }

  s1->display();

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

void changeAllColors(CRGB color) {
  for(unsigned int i=0; i<NUM_STREAKS; i++) {
    pinkS[i]->setColor(color);
    blueS[i]->setColor(color);
    greenS[i]->setColor(color);
  }
  soundReaction->setOnColor(color);
  soundReaction->setOffColor(off);
}

void defaultAllColors() {
  for(unsigned int i=0; i<NUM_STREAKS; i++) {
    pinkS[i]->setColor(pink);
    blueS[i]->setColor(blue);
    greenS[i]->setColor(green);
  }
  soundReaction->setOnColor(pink);
  soundReaction->setOffColor(blue);
}

CRGB readColor() {
  uint16_t clear, red, green, blue;
  float r, g, b;
  CRGB c;

  digitalWrite(SENSOR_LED_PIN, HIGH);
  delay(400);
  tcs.getRawData(&red, &green, &blue, &clear);
  delay(400);
  digitalWrite(SENSOR_LED_PIN, LOW);

  r = (float)red / (float)clear;
  g = (float)green / (float)clear;
  b = (float)blue / (float)clear;

  return maximizeSaturation(r, g, b);
}

CRGB maximizeSaturation(float r, float g, float b) {
  CRGB maxColor;
  float min, max, delta, hue, hue255;

  min = min(r, min(g, b));
  max = max(r, max(g, b));
  delta = max - min;

  if(r == max) {
    hue = ( g - b ) / delta;
  } else if (g == max) {
    hue = 2 + (b - r) / delta;
  } else {
    hue = 4 + (r - g) / delta;
  }

  hue *= 60; // degrees
  if( hue < 0 ) {
    hue += 360;
  }

  hue255 = (uint8_t)((hue/360) * 255);

  maxColor = CHSV(hue255, 244, 255);
  return maxColor;
}
