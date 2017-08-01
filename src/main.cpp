#include <Arduino.h>
#include <FastLED.h>
#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <Visualization.h>
#include <Streak.h>
#include <Ladder.h>
#include <Sparkle.h>
#include <Frequency.h>
#include <TeensyAudioFFT.h>

#define NUM_LEDS 358
#define ROWS 45
#define COLUMNS 6
#define SENSOR_ACTIVATE_PIN 0
#define SENSOR_LED_PIN 16
#define DISPLAY_LED_PIN 22

const int AUDIO_INPUT_PIN = 14;         // Input ADC pin for audio data.

CRGB leds[NUM_LEDS];
CRGB off = 0x000000;

void clear();
void setAll(CRGB color);
CRGB readColor();

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
Ladder * pinkL[NUM_STREAKS];
Ladder * blueL[NUM_STREAKS];
Ladder * greenL[NUM_STREAKS];

Sparkle * s1;
Sparkle * s2;

Frequency * freq;

int active = 0;

int FreqVal[7];
int MaxFreqVal[7];

// COLOR SENSOR
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

// our RGB -> eye-recognized gamma color
byte gammatable[256];

void setup() {
  delay(2000);
  Serial.begin(38400);
  Serial.println("setup started");

  randomSeed(analogRead(14));

  // AUDIO setup
  TeensyAudioFFTSetup(AUDIO_INPUT_PIN);
  samplingBegin();

  // COLOR SENSOR
  pinMode(SENSOR_LED_PIN, OUTPUT);
  pinMode(SENSOR_ACTIVATE_PIN, INPUT);

  if (tcs.begin()) {
    Serial.println("Found sensor");
    digitalWrite(SENSOR_LED_PIN, LOW);
  } else {
    Serial.println("No TCS34725 found ... check your connections");
    while (1); // halt!
  }

  for (int i=0; i<256; i++) {
    float x = i;
    x /= 255;
    x = pow(x, 2.5);
    x *= 255;

    gammatable[i] = x;
    // Serial.println(gammatable[i]);

    Serial.println("setup complete");
  }

  // DISPLAY STUFF
  FastLED.setBrightness(48);
  FastLED.addLeds<NEOPIXEL, DISPLAY_LED_PIN>(leds, NUM_LEDS).setCorrection( Typical8mmPixel );;

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

  s1 = new Sparkle(1, NUM_LEDS, leds, 0xFFFFFF, 201);
  //s2 = new Sparkle(COLUMNS, ROWS, leds, green, 421);

}

void loop() {
  CRGB color;
  clear();  // this just sets the array, no reason it can't be at the top

  // Serial.println(touchRead(A3));

  if (touchRead(A3) > 1900) {
    Serial.println("Read Color");
    color = readColor();
    for(unsigned int i=0; i<NUM_STREAKS; i++) {
      pinkS[i]->setColor(color);
    }
  }

  unsigned long currentTime = millis();

  CRGB reactiveBlue = blueHEX;
  reactiveBlue.fadeLightBy(244);
  for (int i=270; i<358; i++) {
    leds[i] = reactiveBlue;
  }

  float intensity = readRelativeIntensity(currentTime, 2, 3);
  if (intensity > 0.85) {
    intensity = (intensity - 0.5) / 0.5;
    CRGB reactivePink = pinkHEX;
    reactivePink.fadeLightBy((1-(intensity)) * 256);
    for (int i=270; i<358; i++) {
      leds[i] = reactivePink;
    }
  }
  // int intensityCount = min(44, int(intensity * 44));
  // for(int i=0; i<intensityCount; i++) {
  //   leds[i+270] = pink;
  //   leds[357-i] = pink;
  // }



  for(unsigned int i=0; i<NUM_STREAKS; i++) {
    pinkS[i]->display(currentTime);
    blueS[i]->display(currentTime);
    greenS[i]->display(currentTime);
  }
  //
  // for(unsigned int i=0; i<NUM_STREAKS; i++) {
  //   pinkL[i]->display(currentTime);
  //   blueL[i]->display(currentTime);
  //   greenL[i]->display(currentTime);
  // }
  //

  // leds[314] = 0xFFFF00;
  // leds[313] = 0xFFFF00;

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

CRGB readColor() {
  uint16_t clear, red, green, blue;
  float r, g, b, ratio;
  uint32_t color;
  CRGB c;

  digitalWrite(SENSOR_LED_PIN, HIGH);
  delay(400);
  tcs.getRawData(&red, &green, &blue, &clear);
  delay(400);
  digitalWrite(SENSOR_LED_PIN, LOW);

  r = red;
  red = (r / clear) * 256;
  //red = gammatable[red];
  g = green;
  green = (g / clear) * 256;
  //green = gammatable[green];
  b = blue;
  blue = (b / clear) * 256;
  //blue = gammatable[blue];

  // Serial.print((int)clear, DEC);
  // Serial.print(" ");
  Serial.print(red, HEX);
  Serial.print(" ");
  Serial.print(green, HEX);
  Serial.print(" ");
  Serial.print(blue, HEX);
  Serial.print(" ");

  red = gammatable[red];
  green = gammatable[green];
  blue = gammatable[blue];

  color = (red * 65536) + (green * 256) + blue;

  Serial.print(red, HEX);
  Serial.print(" ");
  Serial.print(green, HEX);
  Serial.print(" ");
  Serial.print(blue, HEX);

  Serial.print(" ");
  Serial.print(color, HEX);

  Serial.println();

  c = color;
  c.fadeLightBy(16);
  return c;
}
