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
uint8_t readHue();
uint8_t calcHue(float r, float g, float b);
void defaultAllHues();
void changeAllHues(uint8_t hue);
void stealColorAnimation(uint8_t hue);
void setHeadpieceColumn(uint8_t column, CRGB color);
uint8_t calculateHeadpieceColumnLength(uint16_t column);
void readAccelerometer();

uint16_t xy2Pos(uint16_t x, uint16_t y);

#define NUM_STREAKS 1

#define SATURATION 244

uint8_t pinkHue = 240;
uint8_t blueHue = 137;
uint8_t greenHue = 55;

// Streak * pinkS[NUM_STREAKS];
// Streak * blueS[NUM_STREAKS];
Streak * greenS[NUM_STREAKS];

Sparkle * s1;
Sparkle * s2;

SoundReaction * soundReaction;

Spectrum * spectrumTop;
Spectrum * spectrumBottom;

TeensyAudioFFT * taFFT;

bool colorStolen;

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
  FastLED.setBrightness(48);
  FastLED.setDither(0);

  // INDICATE BOOT SEQUENCE
  for (int i=HEADPIECE_START; i<HEADPIECE_END; i++) {
    leds[i] = 0x0F0F0F;
  }
  FastLED.show();

  // COLOR SENSOR
  pinMode(SENSOR_LED_PIN, OUTPUT);
  pinMode(SENSOR_ACTIVATE_PIN, INPUT);

  if (tcs.begin()) {
    Serial.println("Found color sensor");
    digitalWrite(SENSOR_LED_PIN, LOW);
  } else {
    Serial.println("No color sensor found!");
    setAll(0x330000);
    FastLED.show();
    while (1); // halt!
  }

  // ACCELEROMETER SETUP
  if (mma.begin()) {
    Serial.println("Found accelerometer");
    mma.setRange(MMA8451_RANGE_8_G);
  } else {
    Serial.println("No accelerometer found!");
    setAll(0x30700);
    FastLED.show();
    while (1);
  }

  // AUDIO setup
  TeensyAudioFFTSetup(AUDIO_INPUT_PIN);
  samplingBegin();
  taFFT = new TeensyAudioFFT();
  Serial.println("Audio Sampling Started");

  // DISPLAY STUFF
  clear();
  Serial.println("cleared");
  FastLED.show();
  Serial.println("clear shown");

  for(unsigned int i=0; i<NUM_STREAKS; i++) {
    greenS[i] = new Streak(COLUMNS, ROWS, greenHue, SATURATION, leds);
    greenS[i]->setRandomHue(true);
    greenS[i]->setIntervalMinMax(0, 1);
    // blueS[i] = new Streak(COLUMNS, ROWS, blueHue, SATURATION, leds);
    // pinkS[i] = new Streak(COLUMNS, ROWS, pinkHue, SATURATION, leds);
  }

  Serial.println("Streaks Setup");

  s1 = new Sparkle(NUM_LEDS, 0, 0, leds, 201);
  Serial.println("Sparkles!");

  soundReaction = new SoundReaction(HEADPIECE_START, HEADPIECE_END,
    pinkHue, blueHue, SATURATION, leds);
  Serial.println("Sound Reaction Setup");

  spectrumTop = new Spectrum(COLUMNS, ROWS, ROWS/2,
    blueHue, SATURATION, true, 100, leds);
  spectrumBottom = new Spectrum(COLUMNS, ROWS, ROWS/2,
    blueHue, SATURATION, false, 100, leds);
  Serial.println("Spectrum Setup");

  colorStolen = false;

  Serial.println("setup complete");
}

int maxX = 0;
int maxY = 0;
int maxZ = 0;

void loop() {
  clear();  // this just sets the array, no reason it can't be at the top

  // we have to do this a lot so we don't miss events
  if (colorStolen) {
    readAccelerometer();
  }

  // Serial.println(touchRead(A3));
  if (touchRead(A3) > 1900) {
    Serial.println("Read Color");
    uint8_t hue = readHue();
    stealColorAnimation(hue);
    colorStolen = true;
    changeAllHues(hue);
  }

//  Serial.println(spectrumTop->getHue());

  unsigned long currentTime = millis();

  // we have to do this a lot so we don't miss events
  if (colorStolen) {
    readAccelerometer();
  }

  // HEADPIECE
  taFFT->loop();
  float intensity = readRelativeIntensity(currentTime, 2, 5);
  // Serial.print(currentTime);
  // Serial.print(": ");
  // Serial.println(intensity);
  soundReaction->display(intensity);

  // Serial.println();

  taFFT->updateRelativeIntensities(currentTime);
  spectrumTop->display(taFFT->intensities);
  spectrumBottom->display(taFFT->intensities);

  // we have to do this a lot so we don't miss events
  if (colorStolen) {
    readAccelerometer();
  }

  for(unsigned int i=0; i<NUM_STREAKS; i++) {
    // pinkS[i]->display(currentTime);
    // blueS[i]->display(currentTime);
    greenS[i]->display(currentTime);

  }

  s1->display();

  // we have to do this a lot so we don't miss events
  if (colorStolen) {
    readAccelerometer();
  }

  FastLED.show();

  if (colorStolen) {
    readAccelerometer();
  }

  if (maxY > 6000) {
    colorStolen = false;
    defaultAllHues();

    Serial.print("X:\t"); Serial.print(maxX);
    Serial.print("\tY:\t"); Serial.print(maxY);
    Serial.print("\tZ:\t"); Serial.print(maxZ);
    Serial.println();

    maxX = 0;
    maxY = 0;
    maxZ = 0;
  }
}

void setAll(CRGB color) {
//  Serial.println("setAll");
  for (int i=0; i<NUM_LEDS; i++) {
//    Serial.println(i);
    leds[i] = color;
  }
}

void clear() {
  setAll(off);
}

void changeAllHues(uint8_t hue) {
  for(unsigned int i=0; i<NUM_STREAKS; i++) {
    // pinkS[i]->setHue(hue);
    // blueS[i]->setHue(hue);
    greenS[i]->setHue(hue);
  }

  soundReaction->setOnHue(hue);
  soundReaction->setOffHue(hue);

  spectrumTop->setHue(hue);
  spectrumTop->setTravel(0);
  spectrumBottom->setHue(hue);
  spectrumBottom->setTravel(0);
}

void defaultAllHues() {
  for(unsigned int i=0; i<NUM_STREAKS; i++) {
    // pinkS[i]->setHue(pinkHue);
    // blueS[i]->setHue(blueHue);
    greenS[i]->setHue(greenHue);
  }

  soundReaction->setOnHue(pinkHue);
  soundReaction->setOffHue(blueHue);

  spectrumTop->setHue(blueHue);
  spectrumTop->setTravel(100);
  spectrumBottom->setHue(blueHue);
  spectrumBottom->setTravel(100);
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

  setHeadpieceColumn(3, color);
  FastLED.show();
  delay(1000);

  setHeadpieceColumn(2, color);
  setHeadpieceColumn(4, color);
  FastLED.show();
  delay(500);

  setHeadpieceColumn(1, color);
  setHeadpieceColumn(5, color);
  FastLED.show();
  delay(250);

  setHeadpieceColumn(0, color);
  setHeadpieceColumn(6, color);
  FastLED.show();
  delay(125);

  float d = 125;
  for (uint16_t y=0; y<ROWS; y++) {
    for (uint8_t x=0; x<COLUMNS; x++) {
      leds[xy2Pos(x, y)] = color;
    }
    FastLED.show();
    delay((int)(d *= 0.8));
  }

  Serial.println("done");
}


void setHeadpieceColumn(uint8_t column, CRGB color) {
  uint16_t pos = HEADPIECE_START;

  for (uint8_t i=0; i<column; i++) {
    pos += calculateHeadpieceColumnLength(i);
  }

  for (uint16_t i=0; i<calculateHeadpieceColumnLength(column); i++) {
    leds[i + pos] = color;
  }
}

uint8_t calculateHeadpieceColumnLength(uint16_t column) {
  return 16 - (abs(column - 3) * 2);
}

void readAccelerometer() {
  mma.read();

  maxX = max(mma.x, maxX);
  maxY = max(mma.y, maxY);
  maxZ = max(mma.z, maxZ);
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
