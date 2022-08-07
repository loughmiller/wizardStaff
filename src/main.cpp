#include <Arduino.h>
#include <FastLED.h>
#include <i2c_t3.h>
#define ARM_MATH_CM4
#include <arm_math.h>
#include <algorithm>    // std::sort
#include <Visualization.h>
#include <Streak.h>
#include <Sparkle.h>
#include <Spectrum2.h>

using namespace std;

// DEFINE PINS HERE
#define AUDIO_INPUT_PIN A19  // (38) Input pin for audio data.

#define BATTERY_PIN A13       // Input pin for reading battery level
// WS2811_PORTDC: 2,14,7,8,6,20,21,5,15,22 - 10 way parallel

////////////////////////////////////////////////////////////////////////////////////////////////
// LED AND VISUALIZATION
////////////////////////////////////////////////////////////////////////////////////////////////

// GEOMETRY CONSTANTS
const uint_fast8_t rows = 200;
const uint_fast8_t columns = 10;
const uint_fast16_t numLEDs = rows * columns;

// COLORS
const uint_fast8_t pinkHue = 240;
const uint_fast8_t blueHue = 137;
const uint_fast8_t greenHue = 55;
CRGB off = 0x000000;

const uint_fast8_t lowBatteryBrightness = 60;

// CONTROLLABLE STATE
uint_fast8_t brightness = 244;
uint_fast8_t saturation = 244;
uint_fast8_t sparkles = 65;
uint_fast8_t numStreaks = 1;

// LED display array
CRGB leds[numLEDs];

// streaks array
const uint_fast8_t maxStreaks = 128;
Streak * streaks[maxStreaks];

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
void receiveEvent(uint_fast8_t messageSize);

////////////////////////////////////////////////////////////////////////////////////////////////
// BATTERY
////////////////////////////////////////////////////////////////////////////////////////////////

// CONSTANTS
const float batteryAlpha = 0.3;
const float batteryReading4v = 847.0;
const float batteryReading3v = 625.0;
const float batteryReadingDivider = (batteryReading4v + batteryReading3v) / 7;

const uint_fast32_t maxBatteryReadInteral = 32000;
uint_fast32_t batteryReadInterval = 5000;

CHSV blueBatteryMeterColor(blueHue, saturation, 64);
CHSV redBatteryMeeterColor(0, 255, 64);

// STATE
uint_fast16_t batteryReading = 840;  // start with the lowest 100% reading
uint_fast32_t batteryTimestamp = 0;
float batteryPercentage = 100;


////////////////////////////////////////////////////////////////////////////////////////////////
// RECEIVER
////////////////////////////////////////////////////////////////////////////////////////////////

// message types
const byte typeCycle = 1;
const byte typeBrightness = 2;
const byte typeDensity = 3;
const byte typeSparkles = 4;
const byte typeHue = 5;
const byte typeStreaks = 7;
const byte typeSolid = 8;
const byte typeSteal = 9;

byte messageType = 0;
byte messageData = 0;

////////////////////////////////////////////////////////////////////////////////////////////////
// ACTIONS / CONTROLS
////////////////////////////////////////////////////////////////////////////////////////////////

// STATE
uint_fast32_t loops = 0;
uint_fast32_t setupTime = 0;

bool colorStolen = false;

// FUNCTIONS
void setBrightness();
void setDensity(uint_fast8_t density);
void setSparkles(uint_fast8_t sparkles);
void setStreaks(uint_fast8_t streaks);

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
const uint_fast16_t noteCount{50};              // how many notes are we trying to detect
const uint_fast16_t notesBelowMiddleA{33};

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

  // SETUP LEDS
  // Parallel  Pin layouts on the teensy 3/3.1:
  // // WS2811_PORTD: 2,14,7,8,6,20,21,5
  // // WS2811_PORTC: 15,22,23,9,10,13,11,12,28,27,29,30 (these last 4 are pads on the bottom of the teensy)
  // WS2811_PORTDC: 2,14,7,8,6,20,21,5,15,22,23,9,10,13,11,12 - 16 way parallel

  FastLED.addLeds<WS2811_PORTDC,columns>(leds, rows);
  FastLED.setDither(1);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
  setBrightness();

  // INDICATE BOOT SEQUENCE
  setAll(0x000500);
  FastLED.show();

  while(!Serial && millis() < 10000);
  Serial.println("setup");

  randomSeed(analogRead(A4));

  noteDetectionSetup();

  Wire.begin(4);                // join i2c bus with address #4
  Wire.onReceive(receiveEvent); // register event

  // DISPLAY STUFF
  clear();
  FastLED.show();
  Serial.println("cleared");

  for (uint_fast16_t i=0;i<maxStreaks;i++) {
    streaks[i] = new Streak(columns, rows, greenHue, saturation, leds);
    streaks[i]->setRandomHue(true);
    streaks[i]->setIntervalMinMax(6, 18);
    streaks[i]->setLengthMinMax(9, 57);
    streaks[i]->inititalize(millis());
  }

  Serial.println("Streaks Setup");

  sparkle = new Sparkle(numLEDs, 0, 0, leds, 145);
  setSparkles(sparkles);
  Serial.println("Sparkles!");

  // spectrum1 = new Spectrum2(columns, rows, (rows / 4) - 1, noteCount,
  //   pinkHue, saturation, true, leds);
  // spectrum2 = new Spectrum2(columns, rows, (rows / 4), noteCount,
  //   pinkHue, saturation, false, leds);
  // spectrum3 = new Spectrum2(columns, rows, ((rows / 4) * 3) - 1 , noteCount,
  //   pinkHue, saturation, true, leds);
  // spectrum4 = new Spectrum2(columns, rows, (rows / 4) * 3, noteCount,
  //   pinkHue, saturation, false, leds);

  spectrum1 = new Spectrum2(columns, rows, 0, noteCount,
    pinkHue, saturation, false, true, leds);
 spectrum2 = new Spectrum2(columns, rows, (rows / 2) - 1, noteCount,
    pinkHue, saturation, true, false, leds);
  spectrum3 = new Spectrum2(columns, rows, (rows / 2), noteCount,
    pinkHue, saturation, false, false, leds);
  spectrum4 = new Spectrum2(columns, rows, ((rows / 4) * 3) - 1, noteCount,
    pinkHue, saturation, true, true, leds);


  defaultAllHues();

  Serial.println("setup complete");
  setupTime = millis();
}

uint_fast32_t loggingTimestamp = 0;

// LOOP
void loop() {

  loops++;
  clear();  // this just sets the array, no reason it can't be at the top
  uint_fast32_t currentTime = millis();

  // put things we want to log here
  if (currentTime > loggingTimestamp + 5000) {
  // if (false) {
    loggingTimestamp = currentTime;

    // Serial.print(currentTime);

    // Serial.print("\tFrame Rate: ");
    // Serial.print(loops / ((currentTime - setupTime) / 1000));
//     // Serial.println();
//     // Serial.print(rfTime/loops);
//     // Serial.print("\t");
//     // Serial.print(fftTime/loops);
//     // Serial.print("\t");
//     // Serial.print(batteryTime/loops);
//     // Serial.print("\t");
//     // Serial.print(fastLEDTime/loops);
    // Serial.println("");
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////
  // REMOTE CONTROL
  ////////////////////////////////////////////////////////////////////////////////////////////////

  if (messageType > 0) {
    Serial.print("Control message:");
    Serial.print("\t");
    Serial.print(messageType);
    Serial.print("\t");
    Serial.println(messageData);

  switch(messageType) {
    case typeSteal:
      stealColorAnimation(messageData);
      changeAllHues(messageData);
      Serial.println("Steal Color.");
      break;
    case typeCycle:
      defaultAllHues();
      Serial.println("Cycle Colors.");
      break;
    case typeBrightness:
      brightness = messageData;
      setBrightness();
      Serial.print("Brightness: ");
      Serial.println(messageData);
      break;
    case typeDensity:
      setDensity(messageData);
      Serial.print("Density: ");
      Serial.println(messageData);
      break;
    case typeSparkles:
      setSparkles(messageData);
      Serial.print("Sparkles: ");
      Serial.println(messageData);
      break;
    case typeStreaks:
      setStreaks(messageData);
      Serial.print("Streaks: ");
      Serial.println(messageData);
      break;
    case typeHue:
      changeAllHues(messageData);
      Serial.print("Hue: ");
      Serial.println(messageData);
      break;
    }

    messageType = 0;
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////
  // \ REMOTE CONTROL
  ////////////////////////////////////////////////////////////////////////////////////////////////

//   // uint_fast32_t loopZero = millis();

//   // uint_fast32_t loopOne = millis();
//   // rfTime += loopOne - loopZero;

  // BATTERY READ
  if (currentTime > batteryTimestamp + batteryReadInterval) {
    batteryReadInterval = min(maxBatteryReadInteral, batteryReadInterval + 1000);
    float currentReading = analogRead(BATTERY_PIN);

    batteryReading = (uint_fast16_t)(((float)currentReading * batteryAlpha) + (1 - batteryAlpha) * (float)batteryReading);

    // Battery Log:
    // Timestamp  currentReading  batteryReading, divided voltage, batteryVoltage

    batteryTimestamp = currentTime;
    float batteryVoltage = (float)batteryReading / batteryReadingDivider;

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
    if (batteryVoltage > 3.19) { batteryPercentage = 0.1; }
    if (batteryVoltage > 3.3) { batteryPercentage = 0.2; }
    if (batteryVoltage > 3.38) { batteryPercentage = 0.3; }
    if (batteryVoltage > 3.42) { batteryPercentage = 0.4; }
    if (batteryVoltage > 3.54) { batteryPercentage = 0.5; }
    if (batteryVoltage > 3.64) { batteryPercentage = 0.6; }
    if (batteryVoltage > 3.73) { batteryPercentage = 0.7; }
    if (batteryVoltage > 3.79) { batteryPercentage = 0.8; }
    if (batteryVoltage > 3.89) { batteryPercentage = 0.9; }
    if (batteryVoltage > 3.91) { batteryPercentage = 1; }

    // Serial.print(currentTime);
    // Serial.print("\t");
    // Serial.print(currentReading);
    // Serial.print("\t");
    // Serial.print(batteryReading);
    // Serial.print("\t");
    // Serial.print(dividedVoltage);
    // Serial.print("\t");
    // Serial.print(batteryVoltage);
    // Serial.print("\t");
    // Serial.print(batteryPercentage);
    // Serial.println("");

    // Voltage under 2 means we're likely disconnected from the battery sensor
    // and we're testing things
    if (batteryVoltage > 2 && batteryVoltage < 2.6) {
      Serial.println("");
      Serial.print("Battery Voltage: ");
      Serial.print(batteryVoltage);
      Serial.println("");
      Serial.println("Batteries are dead!");
      clear();
      FastLED.show();
      exit(0);
    }
  }

//   // BATTERY GAUGE
  CHSV batteryMeterColor = blueBatteryMeterColor;

  if (batteryPercentage < 0.2) {
    batteryMeterColor = redBatteryMeeterColor;
  }

  if (batteryPercentage == 0) {
    displayGauge(1, 190, 10, batteryMeterColor, 1);   // display a full red gauge when we're near empty
    // brightness = lowBatteryBrightness;                // lower brightness to extend battery life
    // setBrightness();
  } else {
    displayGauge(1, 190, 10, batteryMeterColor, batteryPercentage);
  }

//   // uint_least32_t loopTwo = millis();
//   // batteryTime += loopTwo - loopOne;

//   // MAIN DISPLAY

  ////////////////////////////////////////////////////////////////////////////////////////////////
  // NOTE DETECTION
  ////////////////////////////////////////////////////////////////////////////////////////////////
  float currentDensity = spectrum1->getDensity();

  if (currentDensity > 0.02) {
    noteDetectionLoop();

    spectrum1->display(noteMagnatudes);
    spectrum2->display(noteMagnatudes);
    spectrum3->display(noteMagnatudes);
    spectrum4->display(noteMagnatudes);
  }

  // uint_least32_t loopThree = millis();
  // fftTime += loopThree - loopTwo;

  ////////////////////////////////////////////////////////////////////////////////////////////////
  // \ NOTE DETECTION
  ////////////////////////////////////////////////////////////////////////////////////////////////

  for (uint_fast16_t i=0;i<numStreaks;i++) {
    streaks[i]->display(currentTime);
  }

  sparkle->display();

  //     // Serial.println(loops/(millis() - startTime));
  //   }
  // }


  FastLED.show();

//   // fastLEDTime += millis() - loopThree;
}

// /LOOP

// ACTIONS

// function that executes whenever data is received from master
// this function is registered as an event, see setup()
void receiveEvent(uint_fast8_t messageSize) {
  if (messageSize != 2) {
    Serial.print("Received bad I2C data.  Expected 2 bytes, got: ");
    Serial.println(messageSize);
    return;
  }

  byte data[2];
  uint_fast8_t i = 0;

  while(0 < Wire.available()) {
    data[i] = Wire.read();
    i++;
  }
  messageType = data[0];
  messageData = data[1];
}

void clearStolenColor() {
  Serial.println("Clear Color");
  colorStolen = false;
  defaultAllHues();
}

void setBrightness() {
  FastLED.setBrightness(brightness);
}

void setDensity(uint_fast8_t density) {
  float newDensity = (float)density/255.0;
  spectrum1->setDensity(newDensity);
  spectrum2->setDensity(newDensity);
  spectrum3->setDensity(newDensity);
  spectrum4->setDensity(newDensity);
}

void setSparkles(uint_fast8_t sparkles) {
  if (sparkles == 0) {
    sparkles = 1;
  }
  sparkle->setEmptiness(4294967295/((float)pow(sparkles, 3.1)));
}

void setStreaks(uint_fast8_t streaks) {
  numStreaks = min(streaks/(256/maxStreaks), maxStreaks);
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
  for (uint_fast16_t i=0;i<maxStreaks;i++) {
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
  for (uint_fast16_t i=0;i<maxStreaks;i++) {
    streaks[i]->setRandomHue(true);
  }

  spectrum1->setDrift(20000);
  spectrum2->setDrift(20000);
  spectrum3->setDrift(20000);
  spectrum4->setDrift(20000);
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

  uint_fast8_t delayMS = 30;

  for (uint_fast16_t y=1; y<rows/2; y++) {
    for (uint_fast8_t x=0; x<columns; x++) {
      leds[xy2Pos(x, (rows/2) + y)] = color;
      leds[xy2Pos(x, (rows/2) - y)] = color;
    }
    FastLED.show();
    delay(delayMS);
    delayMS = max(delayMS * 0.99, 1);
  }

  // Serial.println("StealColorAnimation Complete.");
  setBrightness();
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

  // We're suppose to ignore the top half of the FFT results due to aliasing issues, however
  // we're not trying to sound good, we're trying to look good, and in this context it seems ok
  // There are bigger harmonic(?) issues due to our low sample rate and/or small FFT size.  This
  // looks cool, so we're rolling with it.  This should allow us ~60 notes, but we only need 50
  // due to current staff geometery.
  for (uint_fast16_t i=1; i<fftSize; i++) {
    // Serial.print(magnitudes[i]);
    // Serial.print("\t");
    float frequency = i * (fftBinSize);
    uint_fast16_t note = roundf(12 * (log(frequency / middleA) / log(2))) + notesBelowMiddleA;

    if (note < 0) {
      continue;
    }

    if (note >= noteCount) {
      break;
    }

    // note = note % noteCount;  // I'd like a side by side of this vs the above break

    // Multiple bins can point to the same note, use the largest magnitude
    noteMagnatudes[note] = max(noteMagnatudes[note], magnitudes[i]);
  }
  // Serial.println("");
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
