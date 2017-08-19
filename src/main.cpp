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
CRGB readColor();
CRGB maximizeSaturation(float r, float g, float b);
void defaultAllColors();
void changeAllColors(CRGB color);
void stealColorAnimation(CRGB color);
void setHeadpieceColumn(uint8_t column, CRGB color);
uint8_t calculateHeadpieceColumnLength(int column);
void readAccelerometer();

uint16_t xy2Pos (uint16_t x, uint16_t y);

#define NUM_STREAKS 5
#define NUM_LADDERS 3

#define pinkHEX 0xFF0B20
#define blueHEX 0x0BFFDD

uint8_t pinkHue = 240;
uint8_t blueHue = 137;

CRGB pink = pinkHEX;
CRGB blue = blueHEX;
CRGB green = 0xB9FF0B;

Streak * pinkS[NUM_STREAKS];
Streak * blueS[NUM_STREAKS];
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
  Serial.println("MMA8451 found!");

  // AUDIO setup
  TeensyAudioFFTSetup(AUDIO_INPUT_PIN);
  samplingBegin();
  taFFT = new TeensyAudioFFT();

  // DISPLAY STUFF
  clear();
  FastLED.show();

  for(unsigned int i=0; i<NUM_STREAKS; i++) {
    greenS[i] = new Streak(COLUMNS, ROWS, leds, green);
    blueS[i] = new Streak(COLUMNS, ROWS, leds, blue);
    pinkS[i] = new Streak(COLUMNS, ROWS, leds, pink);
  }

  s1 = new Sparkle(1, NUM_LEDS, leds, 0xFFFFFF, 201);

  CRGB lowBlue = blue;
  lowBlue.fadeLightBy(244);
  soundReaction = new SoundReaction(HEADPIECE_START, HEADPIECE_END, leds, pink, lowBlue);

  spectrumTop = new Spectrum(COLUMNS, ROWS, ROWS/2, true, leds, blueHue, 100);
  spectrumBottom = new Spectrum(COLUMNS, ROWS, ROWS/2, false, leds, blueHue, 100);

  colorStolen = false;

  Serial.println("setup complete");
}

int maxX = 0;
int maxY = 0;
int maxZ = 0;

void loop() {
  CRGB color;
  clear();  // this just sets the array, no reason it can't be at the top

  if (colorStolen) {
    readAccelerometer();
  }

  // Serial.println(touchRead(A3));
  if (touchRead(A3) > 1900) {
    Serial.println("Read Color");
    color = readColor();
    stealColorAnimation(color);
    colorStolen = true;
    changeAllColors(color);
  }

  unsigned long currentTime = millis();

  if (colorStolen) {
    readAccelerometer();
  }

  // HEADPIECE
  taFFT->loop();
  float intensity = readRelativeIntensity(currentTime, 2, 5);
  // Serial.print(currentTime);
  // Serial.print(": ");
  // Serial.print(i);
  // Serial.print(" | ");
  // Serial.println(intensity);
  soundReaction->display(intensity);

  Serial.println();

  taFFT->updateRelativeIntensities(currentTime);
  spectrumTop->display(taFFT->intensities);
  spectrumBottom->display(taFFT->intensities);

  //
  // float rowIntensities[ROWS] = {0};
  // uint8_t halfwayPoint = ROWS/2;
  //
  // for (uint16_t i=0; i<=halfwayPoint; i++) {
  //   rowIntensities[halfwayPoint - i] = taFFT->intensities[i + 2];
  //   rowIntensities[halfwayPoint + i] = taFFT->intensities[i + 2];
  // }
  //
  // for (uint8_t y=0; y<ROWS; y++) {
  //   float intensity = rowIntensities[y];
  //   if (intensity < 0.5) {
  //     continue;
  //   }
  //
  //   intensity = (intensity - 0.5) * 2;
  //   uint8_t travel = (pinkHue - blueHue) * intensity;
  //   // uint8_t travel = 70 * intensity;
  //
  //   CHSV c = CHSV(blueHue + travel, 245, 255);
  //
  //   for (uint8_t x=0; x<COLUMNS; x++) {
  //     leds[xy2Pos(x, y)] = c;
  //   }
  // }

  // bool foo = false;
  //
  // if (currentTime > 10000) {
  //   foo = true;
  // }
  //
  // while (foo) {
  //   // hang
  // }


  if (colorStolen) {
    readAccelerometer();
  }

  // for(unsigned int i=0; i<NUM_STREAKS; i++) {
  //   pinkS[i]->display(currentTime);
  //   blueS[i]->display(currentTime);
  //   greenS[i]->display(currentTime);
  // }

  s1->display();

  if (colorStolen) {
    readAccelerometer();
  }

  FastLED.show();

  if (colorStolen) {
    readAccelerometer();
  }

  if (maxY > 6000) {
    colorStolen = false;
    defaultAllColors();

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
  delay(200);
  tcs.getRawData(&red, &green, &blue, &clear);
  digitalWrite(SENSOR_LED_PIN, LOW);

  r = (float)red / (float)clear;
  g = (float)green / (float)clear;
  b = (float)blue / (float)clear;

  return maximizeSaturation(r, g, b);
}

CRGB maximizeSaturation(float r, float g, float b) {
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

  hue255 = (uint8_t)((hue/360) * 255);

  maxColor = CHSV(hue255, 244, 255);
  return maxColor;
}

void stealColorAnimation(CRGB color) {
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
      leds[pinkS[0]->xy2Pos(x, y)] = color;
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

uint8_t calculateHeadpieceColumnLength(int column) {
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
