#include <Arduino.h>
#include <FastLED.h>
#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <Adafruit_Sensor.h>
#define ARM_MATH_CM4
#include <arm_math.h>
#include <algorithm>    // std::sort

#include <Visualization.h>
#include <Streak.h>
#include <Sparkle.h>
#include <Spectrum2.h>

using namespace std;

#define ROWS 164
#define COLUMNS 8
#define NUM_LEDS (ROWS*COLUMNS)
#define CONTROL_UP 1
#define CONTROL_DOWN 0
#define CONTROL_MODE 23
#define SENSOR_LED_PIN 16
// #define DISPLAY_LED_PIN 32  Teensy 3.6 layout
#define DISPLAY_LED_PIN 12
#define BATTERY_PIN A7
#define AUDIO_INPUT_PIN A8        // Input pin for audio data.

#define ANALOG_RATIO 310.3
#define BATTERY_SLOPE 0.0043
#define BATTERY_INTERCEPT -3.1616
#define BATTERY_APLPHA 0.2
#define BATTERY_DEAD_READING 675
#define BATTERY_READ_INTERVAL 120000
#define BATTERY_LOAD_OFFSET 1.07

#define MODES 3
#define STEAL_COLOR 0
#define CHANGE_BRIGHTNESS 1
#define CHANGE_DENSITY 2

#define BRIGHTNESS 208
#define SATURATION 244
#define NUM_STREAKS 3

#define BUTTON_VALUE 128

uint8_t pinkHue = 240;
uint8_t blueHue = 137;
uint8_t greenHue = 55;


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
uint16_t xy2Pos(uint16_t x, uint16_t y);
void displayGauge(uint_fast16_t x, uint_fast16_t yTop, uint_fast16_t length, CHSV color, float value);

// ACTIONS
void stealColor();
void clearStolenColor();
void increaseBrightness();
void decreaseBrightness();

// GLOBALS (OMG - WTF?)
uint_fast8_t currentMode = 0;
uint_fast8_t currentBrightness = BRIGHTNESS;
CHSV currentModeColor(((256/MODES) * currentMode) + pinkHue, SATURATION, BUTTON_VALUE);
unsigned long buttonTimestamp = 0;

uint16_t batteryReading = 2000;
unsigned long batteryTimestamp = 0;

CHSV blueBatteryMeterColor(blueHue, SATURATION, 64);
CHSV redBatteryMeeterColor(0, 255, 64);

CHSV pinkMeterColor(pinkHue, SATURATION, 255);

Streak * streaks[NUM_STREAKS];
Sparkle * sparkle;

// COLOR SENSOR
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);


////////////////////////////////////////////////////////////////////////////////////////////////
// NOTE DETECTION
////////////////////////////////////////////////////////////////////////////////////////////////

// NOTE DETECTION CONSTANTS
const uint_fast16_t fftSize{256};               // Size of the FFT.  Realistically can only be at most 256
const uint_fast16_t fftBinSize{8};              // Hz per FFT bin  -  sample rate is fftSize * fftBinSize
const uint_fast16_t sampleCount{fftSize * 2};   // Complex FFT functions require a coefficient for the imaginary part of the
                                                // input.  This makes the sample array 2x the fftSize
const float middleA{440.0};                     // frequency of middle A.  Needed for freqeuncy to note conversion
const uint_fast16_t sampleIntervalMs{1000000 / (fftSize * fftBinSize)};  // how often to get a sample, needed for IntervalTimer

// FREQUENCY TO NOTE CONSTANTS - CALCULATE HERE: https://docs.google.com/spreadsheets/d/1CPcxGFB7Lm6xJ8CePfCF0qXQEZuhQ-nI1TC4PAiAd80/edit?usp=sharing
const uint_fast16_t noteCount{40};              // how many notes are we trying to detect
const uint_fast16_t notesBelowMiddleA{30};      

// NOTE DETECTION GLOBALS
float samples[sampleCount*2];
uint_fast16_t sampleCounter = 0;
float sampleBuffer[sampleCount];
float magnitudes[fftSize];
float noteMagnatudes[noteCount];
arm_cfft_radix4_instance_f32 fft_inst;
IntervalTimer samplingTimer;

// NOTE DETECTION FUNCTIONS
void noteDetectionSetup();        // run this once during setup
void noteDetectionLoop();         // run this once per loop
void samplingCallback();

////////////////////////////////////////////////////////////////////////////////////////////////
// \ NOTE DETECTION
////////////////////////////////////////////////////////////////////////////////////////////////

Spectrum2 * spectrum1;
Spectrum2 * spectrum2;
Spectrum2 * spectrum3;
Spectrum2 * spectrum4;

void setup() {
  delay(2000);

  Serial.begin(38400);
  Serial.println("setup started");

  randomSeed(analogRead(14));

  noteDetectionSetup();

  // SETUP LEDS
  FastLED.addLeds<NEOPIXEL, DISPLAY_LED_PIN>(leds, NUM_LEDS).setCorrection( 0xFFD08C );;
  FastLED.setDither(1);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 3000);
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

  // DISPLAY STUFF
  clear();
  FastLED.show();
  Serial.println("cleared");

  for (uint_fast16_t i=0;i<NUM_STREAKS;i++) {
    streaks[i] = new Streak(COLUMNS, ROWS, greenHue, SATURATION, leds);
    streaks[i]->setRandomHue(true);
    streaks[i]->setIntervalMinMax(7, 37);
    streaks[i]->setLengthMinMax(13, 37);
    streaks[i]->inititalize(millis());
  }

  Serial.println("Streaks Setup");

  sparkle = new Sparkle(NUM_LEDS, 0, 0, leds, 247);
  Serial.println("Sparkles!");

  spectrum1 = new Spectrum2(COLUMNS, ROWS, (ROWS / 4) - 1, noteCount,
    pinkHue, SATURATION, true, 100, leds);
  // spectrum2 = new Spectrum2(COLUMNS, ROWS, (ROWS / 4), noteCount,
  //   pinkHue, SATURATION, false, 100, leds);
  spectrum2 = new Spectrum2(COLUMNS, ROWS, (ROWS / 2) - 1, noteCount,
    pinkHue, SATURATION, true, 100, leds);
  spectrum3 = new Spectrum2(COLUMNS, ROWS, ((ROWS / 4) * 3) - 1 , noteCount,
    pinkHue, SATURATION, true, 100, leds);
  // spectrum4 = new Spectrum2(COLUMNS, ROWS, (ROWS / 4) * 3, noteCount,
  //   pinkHue, SATURATION, false, 100, leds);
  spectrum4 = new Spectrum2(COLUMNS, ROWS, ROWS - 1, noteCount,
    pinkHue, SATURATION, true, 100, leds);

  defaultAllHues();
  Serial.println("setup complete");
}

// LOOP
void loop() {
  clear();  // this just sets the array, no reason it can't be at the top
  unsigned long currentTime = millis();

  if (currentTime > buttonTimestamp + 500) {
    // Serial.println(touchRead(CONTROL_UP));
    // Serial.println(touchRead(CONTROL_DOWN));
    // Serial.println(touchRead(CONTROL_MODE));

    buttonTimestamp = currentTime;

    if (touchRead(CONTROL_MODE) > 4000) {
      currentMode = (currentMode + 1) % MODES;
      currentModeColor.hue = (256/MODES) * currentMode;
      Serial.print("Change Mode to ");
      Serial.println(currentMode);
    }

    if (touchRead(CONTROL_UP) > 4000) {
      switch(currentMode) {
        case STEAL_COLOR: stealColor();
          break;
        case CHANGE_BRIGHTNESS: increaseBrightness();
          break;
        // case 2: increaseSpectrumThreshold();
      }
    }

    if (touchRead(CONTROL_DOWN) > 4000) {
      switch(currentMode) {
        case STEAL_COLOR: clearStolenColor();
          break;
        case CHANGE_BRIGHTNESS: decreaseBrightness();
          break;
        // case 2: decreaseSpectrumThreshold();
      }
    }
  }

  // BATTERY READ
  if (currentTime > batteryTimestamp + BATTERY_READ_INTERVAL) {
    float currentReading = analogRead(BATTERY_PIN);

    if (batteryReading == 2000) {
      batteryReading = (uint_fast16_t)currentReading;
    } else {
      batteryReading = (uint_fast16_t)((float)batteryReading * BATTERY_APLPHA + (1 - BATTERY_APLPHA) * (float)currentReading);
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
  CHSV batteryMeterColor = blueBatteryMeterColor;

  if (batteryPercentage < 0.2) {
    batteryMeterColor = redBatteryMeeterColor;
  }

  displayGauge(2, 154, 10, batteryMeterColor, batteryPercentage);


  // BUTTON INDICATORS
  // CHANGE MODE
  leds[xy2Pos(1, 13)] = currentModeColor;
  leds[xy2Pos(2, 13)] = currentModeColor;

  // UP
  leds[xy2Pos(1, 21)] = CHSV(pinkHue, SATURATION, BUTTON_VALUE);
  leds[xy2Pos(2, 21)] = CHSV(pinkHue, SATURATION, BUTTON_VALUE);

  // DOWN
  leds[xy2Pos(1, 28)] = CHSV(blueHue, SATURATION, BUTTON_VALUE);
  leds[xy2Pos(2, 28)] = CHSV(blueHue, SATURATION, BUTTON_VALUE);

  // MAIN DISPLAY

  ////////////////////////////////////////////////////////////////////////////////////////////////
  // NOTE DETECTION
  ////////////////////////////////////////////////////////////////////////////////////////////////
  noteDetectionLoop();

  spectrum1->display(noteMagnatudes);
  spectrum2->display(noteMagnatudes);
  spectrum3->display(noteMagnatudes);
  spectrum4->display(noteMagnatudes);

  ////////////////////////////////////////////////////////////////////////////////////////////////
  // \ NOTE DETECTION
  ////////////////////////////////////////////////////////////////////////////////////////////////

  for (uint_fast16_t i=0;i<NUM_STREAKS;i++) {
    streaks[i]->display(currentTime);
  }

  sparkle->display();

  // BRIGHTNESS GAUGE
  if (currentMode == CHANGE_BRIGHTNESS) {
    displayGauge(3, 0, 15, pinkMeterColor, ((float)currentBrightness)/240.0);
  }

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
  currentBrightness = min((currentBrightness + 16), (uint_fast8_t) 240);
  FastLED.setBrightness(currentBrightness);
}

void decreaseBrightness() {
  currentBrightness = max((currentBrightness - 16), (uint_fast8_t)16);
  FastLED.setBrightness(currentBrightness);
}

void setAll(CRGB color) {
  for (uint_fast16_t i=0; i<NUM_LEDS; i++) {
    leds[i] = color;
  }
}

void clear() {
  setAll(off);
}

void changeAllHues(uint8_t hue) {
  for (uint_fast16_t i=0;i<NUM_STREAKS;i++) {
    streaks[i]->setRandomHue(false);
    streaks[i]->setHue(hue);
  }

  spectrum1->setHue(hue);
  spectrum2->setHue(hue);
  spectrum3->setHue(hue);
  spectrum4->setHue(hue);

  spectrum1->setDrift(0);
  spectrum2->setDrift(0);
  spectrum3->setDrift(0);
  spectrum4->setDrift(0);
}

void defaultAllHues() {
  for (uint_fast16_t i=0;i<NUM_STREAKS;i++) {
    streaks[i]->setRandomHue(true);
  }

  spectrum1->setDrift(5);
  spectrum2->setDrift(-3);
  spectrum3->setDrift(3);
  spectrum4->setDrift(-5);
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
  float minC, maxC, delta, hue;

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
  float z = 0;
  CRGB color = CHSV(hue, SATURATION, 255);
  setAll(off);

  FastLED.setBrightness(64);

  for (uint16_t y=1; y<ROWS; y++) {
    for (uint8_t x=0; x<COLUMNS; x++) {
      leds[xy2Pos(x, y)] = color;
    }
    if (y > z) {
      FastLED.show();
      z = y * 1.07;
    }
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

void displayGauge(uint_fast16_t x, uint_fast16_t yTop, uint_fast16_t length, CHSV color, float value) {
  for (uint_fast16_t i = 0; i < length; i++) {
    if (value >= (float)(length - i) / (float)length) {
      leds[xy2Pos(x, yTop + i)] = color;
    }
  }
}


////////////////////////////////////////////////////////////////////////////////////////////////
// NOTE DETECTION
////////////////////////////////////////////////////////////////////////////////////////////////

void noteDetectionSetup() {
  pinMode(AUDIO_INPUT_PIN, INPUT);
  analogReadResolution(10);
  analogReadAveraging(16);
  arm_cfft_radix4_init_f32(&fft_inst, fftSize, 0, 1);
  samplingTimer.begin(samplingCallback, sampleIntervalMs);
}

void noteDetectionLoop() {
  // copy the last N samples into a buffer
  memcpy(sampleBuffer, samples + (sampleCounter + 1), sizeof(float) * sampleCount);

  // FFT magic
  arm_cfft_radix4_f32(&fft_inst, sampleBuffer);
  arm_cmplx_mag_f32(sampleBuffer, magnitudes, fftSize);

  for (uint_fast16_t i=0; i<noteCount; i++) {
    noteMagnatudes[i] = 0;
  }

  for (uint_fast16_t i=1; i<fftSize/2; i++) {  // ignore top half of the FFT results
    float frequency = i * (fftBinSize);
    uint_fast16_t note = roundf(12 * (log(frequency / middleA) / log(2))) + notesBelowMiddleA;

    if (note < 0) {
      continue;
    }

    note = note % noteCount;
    noteMagnatudes[note] = max(noteMagnatudes[note], magnitudes[i]);
  }  
}

void samplingCallback() {
  // Read from the ADC and store the sample data
  float sampleData = (float)analogRead(AUDIO_INPUT_PIN);

  // storing the data twice in the ring buffer array allows us to do a single memcopy
  // to get an ordered buffer of the last N samples
  uint_fast16_t sampleIndex = (sampleCounter) * 2;
  uint_fast16_t sampleIndex2 = sampleIndex + sampleCount;
  samples[sampleIndex] = sampleData;
  samples[sampleIndex2] = sampleData;

  // Complex FFT functions require a coefficient for the imaginary part of the
  // input.  Since we only have real data, set this coefficient to zero.
  samples[sampleIndex+1] = 0.0;
  samples[sampleIndex2+1] = 0.0;

  sampleCounter++;
  sampleCounter = sampleCounter % fftSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////
// \ NOTE DETECTION
////////////////////////////////////////////////////////////////////////////////////////////////
