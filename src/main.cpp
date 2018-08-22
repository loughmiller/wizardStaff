#include <Arduino.h>
#include <FastLED.h>
#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_MMA8451.h>
#include <Visualization.h>
#include <Streak.h>
#include <Sparkle.h>
#include <SoundReaction.h>
#include <Spectrum.h>
#include <TeensyAudioFFT.h>

#define ROWS 164
#define COLUMNS 8
#define NUM_LEDS ROWS*COLUMNS
#define CONTROL_UP 1
#define CONTROL_DOWN 0
#define CONTROL_MODE 23
#define SENSOR_LED_PIN 16
#define DISPLAY_LED_PIN 12
#define BATTERY_PIN A7
#define AUDIO_INPUT_PIN A8        // Input pin for audio data.

#define ANALOG_RATIO 310.3
#define BATTERY_SLOPE 0.0045
#define BATTERY_INTERCEPT -3.14
#define BATTERY_APLPHA 0.2
#define BATTERY_DEAD_READING 690
#define BATTERY_READ_INTERVAL 120000
#define BATTERY_LOAD_OFFSET 1.07

#define MODE_COLOR_STEAL 0
#define MODE_BRIGHTNESS 1
#define MODE_SPECTRUM_COLOR 2
#define MODE_SPECTRUM_TRAVEL 3
#define MODE_SPARKLE_COLOR 4
#define MODE_SPARKLE_AMOUNT 5
#define MODE_STREAK_COLOR 6
#define MODE_STREAK_COUNT 7
#define MODES 2

#define BRIGHTNESS 224
#define SATURATION 244
#define NUM_STREAKS 3

CRGB leds[NUM_LEDS];
CRGB off = 0x000000;

// FUNTION DEFINITIONS
void clear();
void setAll(CRGB color);
uint8_t readHue();
uint8_t calcHue(float r, float g, float b);
void defaultAllHues();
void changeAllHues(uint8_t hue);
void stealColorAnimation(uint8_t hue);
void readAccelerometer();
uint16_t xy2Pos(uint16_t x, uint16_t y);

// ACTIONS
void stealColor();
void clearStolenColor();
void increaseBrightness();
void decreaseBrightness();
void increaseSpectrumHue();
void decreaseSpectrumHue();

// GLOBALS (OMG - WTF?)
uint_fast8_t currentMode = 0;
uint_fast8_t currentBrightness = BRIGHTNESS;
CHSV currentModeColor((256/MODES) * currentMode, SATURATION, 32);
unsigned long buttonTimestamp = 0;

uint16_t batteryReading = 2000;
unsigned long batteryTimestamp = 0;

uint8_t pinkHue = 240;
uint8_t blueHue = 137;
uint8_t greenHue = 55;

uint_fast8_t currentSpectrumHue = blueHue;

CHSV blueBatteryMeterColor(blueHue, SATURATION, 64);
CRGB redBatteryMeeterColor = 0x060000;

Streak * streaks[NUM_STREAKS];
Sparkle * sparkle;
SoundReaction * soundReaction;

Spectrum * spectrumTop;
Spectrum * spectrumBottom;
Spectrum * spectrumTopFull;
Spectrum * spectrumBottomFull;

TeensyAudioFFT * taFFT;

// COLOR SENSOR
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

// ACCELEROMETER
Adafruit_MMA8451 mma = Adafruit_MMA8451();

void setup() {
  delay(2000);

  Serial.begin(38400);
  Serial.println("setup started");

  randomSeed(analogRead(14));

  // SETUP LEDS
  FastLED.addLeds<NEOPIXEL, DISPLAY_LED_PIN>(leds, NUM_LEDS).setCorrection( 0xFFD08C );;
  FastLED.setDither(1);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 2600);
  FastLED.setBrightness(currentBrightness);

  // INDICATE BOOT SEQUENCE
  setAll(0x000200);
  FastLED.show();
  FastLED.delay(1000);

  // COLOR SENSOR
  pinMode(SENSOR_LED_PIN, OUTPUT);
  pinMode(CONTROL_UP, INPUT);
  pinMode(CONTROL_DOWN, INPUT);
  pinMode(CONTROL_MODE, INPUT);

  // COLOR SENSOR SETUP
  if (tcs.begin()) {
    Serial.println("Found color sensor");
    digitalWrite(SENSOR_LED_PIN, LOW);
  } else {
    Serial.println("No color sensor found!");
    setAll(0x040000);
    FastLED.show();
    FastLED.delay(30000);
  }

  // AUDIO setup
  TeensyAudioFFTSetup(AUDIO_INPUT_PIN);
  samplingBegin();
  taFFT = new TeensyAudioFFT();
  Serial.println("Audio Sampling Started");

  // DISPLAY STUFF
  clear();
  FastLED.show();
  Serial.println("cleared");

  for (int i=0;i<NUM_STREAKS;i++) {
    streaks[i] = new Streak(COLUMNS, ROWS, greenHue, SATURATION, leds);
    streaks[i]->setRandomHue(true);
    streaks[i]->setIntervalMinMax(7, 37);
    streaks[i]->setLengthMinMax(13, 37);
    streaks[i]->inititalize(millis());
  }

  Serial.println("Streaks Setup");

  sparkle = new Sparkle(NUM_LEDS, 0, 0, leds, 397);
  Serial.println("Sparkles!");

  spectrumTop = new Spectrum(COLUMNS, ROWS, (ROWS / 2) - 1, ROWS/2,
    currentSpectrumHue, SATURATION, true, 100, leds);
  spectrumBottom = new Spectrum(COLUMNS, ROWS, ROWS / 2, ROWS/2,
    currentSpectrumHue, SATURATION, false, 100, leds);
  spectrumTopFull = new Spectrum(COLUMNS, ROWS, 0, ROWS/2,
    currentSpectrumHue, SATURATION, false, 100, leds);
  spectrumBottomFull = new Spectrum(COLUMNS, ROWS, ROWS-1, ROWS/2,
    currentSpectrumHue, SATURATION, true, 100, leds);

  Serial.println("Spectrum Setup");

  Serial.println("setup complete");
}

// LOOP
void loop() {
  clear();  // this just sets the array, no reason it can't be at the top
  unsigned long currentTime = millis();


  // Serial.println(touchRead(CONTROL_UP));
  // Serial.println(touchRead(CONTROL_DOWN));
  // Serial.println(touchRead(CONTROL_MODE));

  if (currentTime > buttonTimestamp + 1000) {
    buttonTimestamp = currentTime;

    if (touchRead(CONTROL_MODE) > 4000) {
      currentMode = (currentMode + 1) % MODES;
      currentModeColor.hue = (256/MODES) * currentMode;
      Serial.print("Change Mode to ");
      Serial.println(currentMode);
    }

    if (touchRead(CONTROL_UP) > 4000) {
      switch(currentMode) {
        case 0: stealColor();
        case 1: increaseBrightness();
      }
    }

    if (touchRead(CONTROL_DOWN) > 4000) {
      switch(currentMode) {
        case 0: clearStolenColor();
        case 1: decreaseBrightness();
      }
    }
  }

  // BATTERY READ
  if (currentTime > batteryTimestamp + BATTERY_READ_INTERVAL) {
    float currentReading = analogRead(BATTERY_PIN) * BATTERY_LOAD_OFFSET;

    if (batteryReading == 2000) {
      batteryReading = (int)currentReading;
    } else {
      batteryReading = (int)((float)batteryReading * BATTERY_APLPHA + (1 - BATTERY_APLPHA) * (float)currentReading);
    }

    batteryTimestamp = currentTime;
    Serial.print(batteryTimestamp);
    Serial.print("\t");
    Serial.print(currentReading);
    Serial.print("\t");
    Serial.print(batteryReading);

    float dividedVoltage = (float)batteryReading / ANALOG_RATIO;
    Serial.print("\t");
    Serial.print(dividedVoltage);

    float batteryPercentage = ((float)batteryReading * BATTERY_SLOPE) + BATTERY_INTERCEPT;
    Serial.print("\t");
    Serial.println(batteryPercentage);

    if (batteryReading < BATTERY_DEAD_READING) {
      Serial.println("");
      Serial.println("Batteries are dead!");
      clear();
      FastLED.show();
      exit(0);
    }
  }

  // BATTERY GAUGE
  float batteryPercentage = ((float)batteryReading * BATTERY_SLOPE) + BATTERY_INTERCEPT;
  CRGB batteryMeterColor = blueBatteryMeterColor;

  if (batteryPercentage < 0.2) {
    batteryMeterColor = redBatteryMeeterColor;
  }

  for (int i=0;i<min(batteryPercentage*10, 10);i++) {
    leds[xy2Pos(2, 163 - i)] = batteryMeterColor;
  }

  // BUTTON INDICATORS
  // CHANGE MODE
  leds[xy2Pos(1, 13)] = currentModeColor;
  leds[xy2Pos(2, 13)] = currentModeColor;

  // CHANGE BRIGHTNESS
  leds[xy2Pos(1, 21)] = 0x000400;
  leds[xy2Pos(2, 21)] = 0x000400;

  // CHANGE BRIGHTNESS
  leds[xy2Pos(1, 28)] = 0x040000;
  leds[xy2Pos(2, 28)] = 0x040000;

  // MAIN DISPLAY
  // Serial.println();
  for (int i=0;i<NUM_STREAKS;i++) {
    streaks[i]->display(currentTime);
  }

  taFFT->loop();
  taFFT->updateRelativeIntensities(currentTime);
  spectrumTop->display(taFFT->intensities);
  spectrumBottom->display(taFFT->intensities);
  spectrumTopFull->display(taFFT->intensities);
  spectrumBottomFull->display(taFFT->intensities);

  sparkle->display();

  FastLED.show();
}
// /LOOP

// ACTIONS

void stealColor() {
  uint8_t hue = readHue();
  Serial.println(hue);
  stealColorAnimation(hue);
  changeAllHues(hue);
}

void clearStolenColor() {
  Serial.println("Clear Color");
  defaultAllHues();
}

void increaseBrightness() {
  if (currentBrightness != 240) {
    currentBrightness = (currentBrightness + 16) % 256;
    FastLED.setBrightness(currentBrightness);
  }
}

void decreaseBrightness() {
  if (currentBrightness != 0) {
    currentBrightness = (currentBrightness - 16) % 256;
    FastLED.setBrightness(currentBrightness);
  }
}

void increaseSpectrumHue() {
  if (currentSpectrumHue != 255) {
    currentSpectrumHue = (currentSpectrumHue + 5) % 256;
    spectrumTop->setHue(currentSpectrumHue);
    spectrumBottom->setHue(currentSpectrumHue);
    spectrumTopFull->setHue(currentSpectrumHue);
    spectrumBottomFull->setHue(currentSpectrumHue);
  }
}

void decreaseSpectrumHue() {
  if (currentSpectrumHue != 0) {
    currentSpectrumHue = (currentSpectrumHue - 5) % 256;
    FastLED.setBrightness(currentBrightness);
  }
}

void setAll(CRGB color) {
  for (int i=0; i<NUM_LEDS; i++) {
    leds[i] = color;
  }
}

void clear() {
  setAll(off);
}

void changeAllHues(uint8_t hue) {
  for (int i=0;i<NUM_STREAKS;i++) {
    streaks[i]->setRandomHue(false);
    streaks[i]->setHue(hue);
  }

  spectrumTop->setHue(hue);
  spectrumTop->setTravel(0);
  spectrumBottom->setHue(hue);
  spectrumBottom->setTravel(0);
  spectrumTopFull->setHue(hue);
  spectrumTopFull->setTravel(0);
  spectrumBottomFull->setHue(hue);
  spectrumBottomFull->setTravel(0);
}

void defaultAllHues() {
  for (int i=0;i<NUM_STREAKS;i++) {
    streaks[i]->setRandomHue(true);
  }

  spectrumTop->setHue(currentSpectrumHue);
  spectrumTop->setTravel(100);
  spectrumBottom->setHue(currentSpectrumHue);
  spectrumBottom->setTravel(100);
  spectrumTopFull->setHue(currentSpectrumHue);
  spectrumTopFull->setTravel(100);
  spectrumBottomFull->setHue(currentSpectrumHue);
  spectrumBottomFull->setTravel(100);
}

uint8_t readHue() {
  uint16_t clear, red, green, blue;
  float r, g, b;
  CRGB c;

  digitalWrite(SENSOR_LED_PIN, HIGH);
  delay(200);
  tcs.getRawData(&red, &green, &blue, &clear);
  digitalWrite(SENSOR_LED_PIN, LOW);

  r = (float)red / (float)clear;
  g = (float)green / (float)clear;
  b = (float)blue / (float)clear;

  return calcHue(r, g, b);
}

uint8_t calcHue(float r, float g, float b) {
  CRGB maxColor;
  float minC, maxC, delta, hue, hue255;

  minC = min(r, min(g, b));
  maxC = max(r, max(g, b));
  delta = maxC - minC;

  if(r == maxC) {
    hue = ( g - b ) / delta;
  } else if (g == maxC) {
    hue = 2 + (b - r) / delta;
  } else {
    hue = 4 + (r - g) / delta;
  }

  hue *= 60; // degrees
  if( hue < 0 ) {
    hue += 360;
  }

  return (uint8_t)((hue/360) * 255);
}

void stealColorAnimation(uint8_t hue) {
  CRGB color = CHSV(hue, SATURATION, 255);
  setAll(off);

  FastLED.setBrightness(64);

  float d = 125;
  for (uint16_t y=0; y<ROWS; y++) {
    for (uint8_t x=0; x<COLUMNS; x++) {
      leds[xy2Pos(x, y)] = color;
    }
    FastLED.show();
    // FastLED.delay((int)(d *= 0.8));
  }

  // Serial.println("StealColorAnimation Complete.");
  FastLED.setBrightness(currentBrightness);
}


uint16_t xy2Pos(uint16_t x, uint16_t y) {
  uint16_t pos = x * ROWS;
  if (x % 2 == 0) {
    pos = pos + y;
  } else {
    pos = pos + ((ROWS - 1) - y);
  }

  return pos;
}
