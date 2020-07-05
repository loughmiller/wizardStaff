#include <Arduino.h>
#include <FastLED.h>
#include <Wire.h>
#include <VirtualWire.h>
#define ARM_MATH_CM4
#include <arm_math.h>
#include <algorithm>    // std::sort

#include <Visualization.h>
#include <Streak.h>
#include <Sparkle.h>
#include <Spectrum2.h>

using namespace std;

// DEFINE PINS HERE
#define AUDIO_INPUT_PIN A8            // Input pin for audio data.
#define BATTERY_PIN A5                // Input pin for reading battery level
// WS2811_PORTD: 2,14,7,8,6,20,21,5   // FastLED parallel output pins


////////////////////////////////////////////////////////////////////////////////////////////////
// LED AND VISUALIZATION
////////////////////////////////////////////////////////////////////////////////////////////////

// GEOMETRY CONSTANTS
const uint_fast8_t rows = 200;
const uint_fast8_t columns = 3;
const uint_fast16_t numLEDs = rows * columns;

// COLORS
const uint_fast8_t saturation = 244;
const uint_fast8_t brightness = 208;
const uint_fast8_t lowBatteryBrightness = 64;

const uint_fast8_t pinkHue = 240;
const uint_fast8_t blueHue = 137;
const uint_fast8_t greenHue = 55;
CRGB off = 0x000000;

// STATE
uint_fast8_t currentBrightness = brightness;

// LED display array
CRGB leds[numLEDs];

// streaks array
const uint_fast8_t numStreaks = 3;
Streak * streaks[numStreaks];

// random sparkle object
Sparkle * sparkle;

// 4 note visualization objects:
Spectrum2 * spectrum1;
Spectrum2 * spectrum2;
Spectrum2 * spectrum3;
Spectrum2 * spectrum4;

// FUNTION DEFINITIONS
void clear();
void setAll(CRGB color);
uint_fast8_t calcHue(float r, float g, float b);
void defaultAllHues();
void changeAllHues(uint_fast8_t hue);
void stealColorAnimation(uint_fast8_t hue);
uint_fast16_t xy2Pos(uint_fast16_t x, uint_fast16_t y);
void displayGauge(uint_fast16_t x, uint_fast16_t yTop, uint_fast16_t length, CHSV color, float value);


////////////////////////////////////////////////////////////////////////////////////////////////
// BATTERY
////////////////////////////////////////////////////////////////////////////////////////////////

// CONSTANTS
const uint_fast8_t numBatteries = 4;

const float analogRatio = 310.3;
const float batteryAlpha = 0.3;
const float voltageDividerRatio = 0.096;
const float batteryLoadAdjustment = 1.1;
const uint_fast32_t maxBatteryReadInteral = 32000;
uint_fast32_t batteryReadInterval = 5000;

CHSV blueBatteryMeterColor(blueHue, saturation, 64);
CHSV redBatteryMeeterColor(0, 255, 64);

// STATE
uint_fast16_t batteryReading = 435;  // start with the lowest 100% reading
uint_fast32_t batteryTimestamp = 0;
float batteryPercentage = 100;


////////////////////////////////////////////////////////////////////////////////////////////////
// RECEIVER
////////////////////////////////////////////////////////////////////////////////////////////////

const uint_fast8_t receive_pin = 9;
const uint_fast8_t maxMessageLength = 3;
uint_fast8_t messageID = 255;

const byte colorReadMessage = 0;
const byte colorClearMessage = 1;
const byte brightnessUpMessage = 2;
const byte brightnessDownMessage = 3;
const byte densityUpMessage = 4;
const byte densityDownMessage = 5;

////////////////////////////////////////////////////////////////////////////////////////////////
// ACTIONS / CONTROLS
////////////////////////////////////////////////////////////////////////////////////////////////

// STATE
uint_fast32_t loops = 0;
uint_fast32_t setupTime = 0;

bool colorStolen = false;

// FUNCTIONS
void stealColor();
void clearStolenColor();
void increaseBrightness();
void decreaseBrightness();
void increaseDensity();
void decreaseDensity();


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


void setup() {
  delay(2000);

  Serial.begin(38400);
  Serial.println("setup started");

  // Initialise the IO and ISR
  vw_set_rx_pin(receive_pin);
  vw_setup(2000);	 // Bits per sec
  vw_rx_start();   // Start the receiver PLL running

  randomSeed(analogRead(14));

  noteDetectionSetup();

  // SETUP LEDS
  // Parallel  Pin layouts on the teensy 3/3.1:
  // Connector:    1, 2,3,4,5, 6, 7,8
  // WS2811_PORTD: 2,14,7,8,6,20,21,5  << THIS IS US
  // // WS2811_PORTC: 15,22,23,9,10,13,11,12,28,27,29,30 (these last 4 are pads on the bottom of the teensy)
  // // WS2811_PORTDC: 2,14,7,8,6,20,21,5,15,22,23,9,10,13,11,12 - 16 way parallel

  FastLED.addLeds<WS2811_PORTD,columns>(leds, rows);
  FastLED.setDither(1);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 3000);
  FastLED.setBrightness(currentBrightness);

  // INDICATE BOOT SEQUENCE
  setAll(0x000200);
  FastLED.show();
  FastLED.delay(1000);

  // DISPLAY STUFF
  clear();
  FastLED.show();
  Serial.println("cleared");

  for (uint_fast16_t i=0;i<numStreaks;i++) {
    streaks[i] = new Streak(columns, rows, greenHue, saturation, leds);
    streaks[i]->setRandomHue(true);
    streaks[i]->setIntervalMinMax(9, 23);
    streaks[i]->setLengthMinMax(13, 37);
    streaks[i]->inititalize(millis());
  }

  Serial.println("Streaks Setup");

  sparkle = new Sparkle(numLEDs, 0, 0, leds, 2477);
  Serial.println("Sparkles!");

  spectrum1 = new Spectrum2(columns, rows, (rows / 4) - 1, noteCount,
    pinkHue, saturation, true, leds);
  // spectrum2 = new Spectrum2(columns, rows, (rows / 4), noteCount,
  //   pinkHue, saturation, false, leds);
  spectrum2 = new Spectrum2(columns, rows, (rows / 2) - 1, noteCount,
    pinkHue, saturation, true, leds);
  spectrum3 = new Spectrum2(columns, rows, ((rows / 4) * 3) - 1 , noteCount,
    pinkHue, saturation, true, leds);
  // spectrum4 = new Spectrum2(columns, rows, (rows / 4) * 3, noteCount,
  //   pinkHue, saturation, false, leds);
  spectrum4 = new Spectrum2(columns, rows, rows - 1, noteCount,
    pinkHue, saturation, true, leds);

  defaultAllHues();

  Serial.println("setup complete");
  setupTime = millis();
}

uint_fast32_t loggingTimestamp = 0;

uint_fast32_t rfTime = 0;
uint_fast32_t fftTime = 0;
uint_fast32_t batteryTime = 0;
uint_fast32_t fastLEDTime = 0;

// LOOP
void loop() {
  loops++;
  clear();  // this just sets the array, no reason it can't be at the top
  uint_fast32_t currentTime = millis();

  // put things we want to log here
  if (currentTime > loggingTimestamp + 5000) {
    loggingTimestamp = currentTime;

    Serial.print(currentTime);

    Serial.print("\tFrame Rate: ");
    Serial.print(loops / ((currentTime - setupTime) / 1000));
    Serial.println();
    Serial.print(rfTime/loops);
    Serial.print("\t");
    Serial.print(fftTime/loops);
    Serial.print("\t");
    Serial.print(batteryTime/loops);
    Serial.print("\t");
    Serial.print(fastLEDTime/loops);
    Serial.println("");
  }

  uint_fast32_t loopZero = millis();

  // RECEIVER
  uint8_t buflen = maxMessageLength;
  byte buf[maxMessageLength];

  if (vw_get_message(buf, &buflen)) {

    if (buf[0] != messageID) {
      messageID = buf[0];

      // logging
      Serial.print("Got: ");
      for (uint_fast8_t i = 0; i < buflen; i++)
      {
        Serial.print(buf[i], HEX);
        Serial.print(' ');
      }
      Serial.println();

      byte messageType = buf[1];
      byte messageData = buf[2];

      switch(messageType) {
        case colorReadMessage:
          stealColorAnimation(messageData);
          changeAllHues(messageData);
          break;
        case colorClearMessage:
          defaultAllHues();
          break;
        case brightnessUpMessage:
          increaseBrightness();
          Serial.println("Increase Brightness.");
          break;
        case brightnessDownMessage:
          decreaseBrightness();
          Serial.println("Decrease Brightness.");
          break;
        case densityUpMessage:
          increaseDensity();
          Serial.println("Increase Density.");
          break;
        case densityDownMessage:
          decreaseDensity();
          Serial.println("Decrease Density.");
          break;
      }

      // Serial.println(loops/(millis() - startTime));
    }
  }

  uint_fast32_t loopOne = millis();

  rfTime += loopOne - loopZero;

  // BATTERY READ
  if (currentTime > batteryTimestamp + batteryReadInterval) {
    batteryReadInterval = min(maxBatteryReadInteral, batteryReadInterval + 1000);
    float currentReading = analogRead(BATTERY_PIN);

    batteryReading = (uint_fast16_t)(((float)currentReading * batteryAlpha) + (1 - batteryAlpha) * (float)batteryReading);

    // Battery Log:
    // Timestamp  currentReading  batteryReading, divided voltage, batteryVoltage

    batteryTimestamp = currentTime;
    Serial.print(currentTime);
    Serial.print("\t");
    Serial.print(currentReading);
    Serial.print("\t");
    Serial.print(batteryReading);

    float dividedVoltage = (float)batteryReading / analogRatio;
    Serial.print("\t");
    Serial.print(dividedVoltage);

    float totalVoltage = dividedVoltage / voltageDividerRatio;
    float batteryVoltage = (totalVoltage / numBatteries) * batteryLoadAdjustment;
    Serial.print("\t");
    Serial.print(batteryVoltage);

    // %   Voltage
    // 100	4.1
    // 95	  4.03
    // 85	  3.98
    // 75	  3.87
    // 65 	3.8
    // 55	  3.7
    // 45	  3.6
    // 35	  3.54
    // 25	  3.48
    // 15	  3.39
    // 5	  3.2
    // 0	  3

    batteryPercentage = 0;
    if (batteryVoltage > 3.1) { batteryPercentage = 0.1; }
    if (batteryVoltage > 3.3) { batteryPercentage = 0.2; }
    if (batteryVoltage > 3.4) { batteryPercentage = 0.3; }
    if (batteryVoltage > 3.5) { batteryPercentage = 0.4; }
    if (batteryVoltage > 3.6) { batteryPercentage = 0.5; }
    if (batteryVoltage > 3.7) { batteryPercentage = 0.6; }
    if (batteryVoltage > 3.8) { batteryPercentage = 0.7; }
    if (batteryVoltage > 3.85) { batteryPercentage = 0.8; }
    if (batteryVoltage > 3.95) { batteryPercentage = 0.9; }
    if (batteryVoltage > 4.0) { batteryPercentage = 1; }

    Serial.print("\t");
    Serial.println(batteryPercentage);

    if (batteryVoltage < 2.8) {
      Serial.println("");
      Serial.println("Batteries are dead!");
      clear();
      FastLED.show();
      exit(0);
    }
  }

  // BATTERY GAUGE
  CHSV batteryMeterColor = blueBatteryMeterColor;

  if (batteryPercentage < 0.2) {
    batteryMeterColor = redBatteryMeeterColor;
  }

  if (batteryPercentage == 0) {
    displayGauge(1, 190, 10, batteryMeterColor, 1);   // display a full red gauge when we're near empty
    FastLED.setBrightness(lowBatteryBrightness);      // lower brightness to extend battery life
  } else {
    displayGauge(1, 190, 10, batteryMeterColor, batteryPercentage);
  }

  uint_least32_t loopTwo = millis();
  batteryTime += loopTwo - loopOne;

  // MAIN DISPLAY

  ////////////////////////////////////////////////////////////////////////////////////////////////
  // NOTE DETECTION
  ////////////////////////////////////////////////////////////////////////////////////////////////
  noteDetectionLoop();

  spectrum1->display(noteMagnatudes);
  spectrum2->display(noteMagnatudes);
  spectrum3->display(noteMagnatudes);
  spectrum4->display(noteMagnatudes);

  uint_least32_t loopThree = millis();
  fftTime += loopThree - loopTwo;

  ////////////////////////////////////////////////////////////////////////////////////////////////
  // \ NOTE DETECTION
  ////////////////////////////////////////////////////////////////////////////////////////////////

  for (uint_fast16_t i=0;i<numStreaks;i++) {
    streaks[i]->display(currentTime);
  }

  sparkle->display();

  FastLED.show();

  fastLEDTime += millis() - loopThree;
}
// /LOOP

// ACTIONS

void stealColor() {
  uint_fast8_t hue = 0;  // Will need to figure this out later
  Serial.print("hue: ");
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
  for (uint_fast16_t i=0; i<numLEDs; i++) {
    leds[i] = color;
  }
}

void clear() {
  setAll(off);
}

void changeAllHues(uint_fast8_t hue) {
  for (uint_fast16_t i=0;i<numStreaks;i++) {
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
  for (uint_fast16_t i=0;i<numStreaks;i++) {
    streaks[i]->setRandomHue(true);
  }

  spectrum1->setDrift(5);
  spectrum2->setDrift(-3);
  spectrum3->setDrift(3);
  spectrum4->setDrift(-5);
}

uint_fast8_t calcHue(float r, float g, float b) {
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

  return (uint_fast8_t)((hue/360) * 255);
}

void stealColorAnimation(uint_fast8_t hue) {
  float z = 0;
  CRGB color = CHSV(hue, saturation, 255);
  setAll(off);

  FastLED.setBrightness(64);

  for (uint_fast16_t y=1; y<rows; y++) {
    for (uint_fast8_t x=0; x<columns; x++) {
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


uint_fast16_t xy2Pos(uint_fast16_t x, uint_fast16_t y) {
  uint_fast16_t pos = x * rows;
  pos = pos + y;

  // if (x % 2 == 0) {
  //   pos = pos + y;
  // } else {
  //   pos = pos + ((rows - 1) - y);
  // }

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
