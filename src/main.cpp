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

// BUTTONS
#define CONTROL_UP 0
#define CONTROL_DOWN 1
#define CONTROL_MODE 23
#define BUTTON_POWER_PIN 11
#define DEBOUNCE_TIME 500
#define BUTTON_THRESHOLD 1.09
#define BUTTON_VALUE 64


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

#define GAUGE_COLUMN 1

#define BRIGHTNESS 208
#define SATURATION 244
#define NUM_STREAKS 4


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
void increaseDensity();
void decreaseDensity();

// GLOBALS (OMG - WTF?)
uint_fast8_t currentMode = 0;
uint_fast16_t modeAvg;
uint_fast16_t upAvg;
uint_fast16_t downAvg;

uint_fast32_t loops = 0;
uint_fast32_t setupTime = 0;

uint_fast8_t currentBrightness = BRIGHTNESS;
CHSV currentModeColor(((256/MODES) * currentMode) + pinkHue, SATURATION, BUTTON_VALUE);
bool colorStolen = false;
unsigned long buttonTimestamp = 0;

uint16_t batteryReading = 2000;
unsigned long batteryTimestamp = 0;

CHSV blueBatteryMeterColor(blueHue, SATURATION, 64);
CHSV redBatteryMeeterColor(0, 255, 64);

CHSV pinkMeterColor(pinkHue, SATURATION, 255);
CHSV blueMeterColor(blueHue, SATURATION, 255);

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
  pinMode(BUTTON_POWER_PIN, OUTPUT);
  digitalWrite(BUTTON_POWER_PIN, HIGH);

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
    pinkHue, SATURATION, true, leds);
  // spectrum2 = new Spectrum2(COLUMNS, ROWS, (ROWS / 4), noteCount,
  //   pinkHue, SATURATION, false, leds);
  spectrum2 = new Spectrum2(COLUMNS, ROWS, (ROWS / 2) - 1, noteCount,
    pinkHue, SATURATION, true, leds);
  spectrum3 = new Spectrum2(COLUMNS, ROWS, ((ROWS / 4) * 3) - 1 , noteCount,
    pinkHue, SATURATION, true, leds);
  // spectrum4 = new Spectrum2(COLUMNS, ROWS, (ROWS / 4) * 3, noteCount,
  //   pinkHue, SATURATION, false, leds);
  spectrum4 = new Spectrum2(COLUMNS, ROWS, ROWS - 1, noteCount,
    pinkHue, SATURATION, true, leds);

  defaultAllHues();

  modeAvg = touchRead(CONTROL_MODE) * BUTTON_THRESHOLD;
  upAvg = touchRead(CONTROL_UP) * BUTTON_THRESHOLD;
  downAvg = touchRead(CONTROL_DOWN) * BUTTON_THRESHOLD;

  Serial.println("setup complete");
  setupTime = millis();
}

uint32_t loggingTimestamp = 0;

// LOOP
void loop() {
  loops++;
  clear();  // this just sets the array, no reason it can't be at the top
  unsigned long currentTime = millis();

  // put things we want to log here
  if (currentTime > loggingTimestamp + 5000) {
    loggingTimestamp = currentTime;

    Serial.print("Frame Rate: ");
    Serial.println(loops / ((currentTime - setupTime) / 1000));
  }

  uint_fast16_t modeTouch = touchRead(CONTROL_MODE);
  modeAvg = (float)modeAvg * 0.98 + (float)modeTouch * 0.02;

  uint_fast16_t upTouch = touchRead(CONTROL_UP);
  upAvg = (float)upAvg * 0.98 + (float)upTouch * 0.02;

  uint_fast16_t downTouch = touchRead(CONTROL_DOWN);
  downAvg = (float)downAvg * 0.98 + (float)downTouch * 0.02;

  if (currentTime > buttonTimestamp + DEBOUNCE_TIME) {
    Serial.print(downTouch);
    Serial.print("\t");
    Serial.println(downAvg);

    if ((float)modeTouch/(float)modeAvg > BUTTON_THRESHOLD) {
      buttonTimestamp = currentTime;
      currentMode = (currentMode + 1) % MODES;
      currentModeColor.hue = (256/MODES) * currentMode;
      Serial.print("Change Mode to ");
      Serial.println(currentMode);
    }

    if ((float)upTouch/(float)upAvg > BUTTON_THRESHOLD) {
      buttonTimestamp = currentTime;
      switch(currentMode) {
        case CHANGE_BRIGHTNESS: increaseBrightness();
          break;
        case CHANGE_DENSITY: increaseDensity();
      }
    }

    if ((float)downTouch/(float)downAvg > BUTTON_THRESHOLD) {
      buttonTimestamp = currentTime;
      switch(currentMode) {
        case STEAL_COLOR:
          if (colorStolen) {
            clearStolenColor();
          } else {
            stealColor();
          }
          break;
        case CHANGE_BRIGHTNESS: decreaseBrightness();
          break;
        case CHANGE_DENSITY: decreaseDensity();
      }
    }

    // Serial.print(modeTouch);
    // Serial.print("\t");
    // Serial.print(modeAvg);
    // Serial.print("\t");
    // Serial.println((float)modeTouch/(float)modeAvg);
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

  displayGauge(GAUGE_COLUMN, 154, 10, batteryMeterColor, batteryPercentage);


  // BUTTON INDICATORS
  // CHANGE MODE
  leds[xy2Pos(GAUGE_COLUMN, 35)] = currentModeColor;
  leds[xy2Pos(GAUGE_COLUMN, 36)] = currentModeColor;

  // UP
  if (currentMode != STEAL_COLOR) {
    leds[xy2Pos(GAUGE_COLUMN, 14)] = CHSV(pinkHue, SATURATION, BUTTON_VALUE);
    leds[xy2Pos(GAUGE_COLUMN, 15)] = CHSV(pinkHue, SATURATION, BUTTON_VALUE);
  }

  // DOWN
  leds[xy2Pos(GAUGE_COLUMN, 21)] = CHSV(blueHue, SATURATION, BUTTON_VALUE);
  leds[xy2Pos(GAUGE_COLUMN, 22)] = CHSV(blueHue, SATURATION, BUTTON_VALUE);

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
    displayGauge(GAUGE_COLUMN, 0, 15, pinkMeterColor, ((float)currentBrightness)/240.0);
  }

  // DENSITY GAUGE
  if (currentMode == CHANGE_DENSITY) {
    displayGauge(GAUGE_COLUMN, 0, 10, blueMeterColor, spectrum1->getDensity());
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
  colorStolen = true;
}

void clearStolenColor() {
  Serial.println("Clear Color");
  colorStolen = false;
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

void increaseDensity() {
  float newDensity = min(0.8, spectrum1->getDensity() + 0.05);
  spectrum1->setDensity(newDensity);
  spectrum2->setDensity(newDensity);
  spectrum3->setDensity(newDensity);
  spectrum4->setDensity(newDensity);
}

void decreaseDensity() {
  float newDensity = max(0.05, spectrum1->getDensity() - 0.05);
  spectrum1->setDensity(newDensity);
  spectrum2->setDensity(newDensity);
  spectrum3->setDensity(newDensity);
  spectrum4->setDensity(newDensity);
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
