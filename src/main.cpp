#include <Arduino.h>
#include <FastLED.h>
#include <AudioAnalyzer.h>
#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <Visualization.h>
#include <Streak.h>
#include <Ladder.h>
#include <Sparkle.h>
#include <Pulse.h>
#include <Frequency.h>

#define NUM_LEDS 358
#define ROWS 45
#define COLUMNS 6
#define SENSOR_ACTIVATE_PIN 0
#define SENSOR_LED_PIN 16
#define DISPLAY_LED_PIN 22

CRGB leds[NUM_LEDS];
CRGB off = 0x000000;

void clear();
void setAll(CRGB color);
CRGB readColor();

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

Frequency * freq;

int active = 0;

Analyzer Audio = Analyzer(4,5,0);//Strobe pin ->4  RST pin ->5 Analog Pin ->0
//Analyzer Audio = Analyzer();//Strobe->4 RST->5 Analog->0

int FreqVal[7];
int MaxFreqVal[7];

// COLOR SENSOR
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_50MS, TCS34725_GAIN_4X);

// our RGB -> eye-recognized gamma color
byte gammatable[256];


void setup() {
  delay(2000);
  Serial.begin(9600);
  Serial.println("setup started");


  // AUDIO STUFF
  Audio.Init();
  for(int i=0;i<7;i++) {
    MaxFreqVal[i] = 0;
  }

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

  s1 = new Sparkle(1, NUM_LEDS, leds, blue, 201);
  //s2 = new Sparkle(COLUMNS, ROWS, leds, green, 421);

  pulseHeadPiece = new Pulse(270, 88, leds, pink);

  // freq = new Frequency(270, 88, leds, pink);
}

void loop() {
  CRGB color;

  Serial.println(touchRead(A3));

  if (touchRead(A3) > 1900) {
    Serial.println('Read Color');
    color = readColor();
    for(unsigned int i=0; i<NUM_STREAKS; i++) {
      pinkS[i]->setColor(color);
    }
    pulseHeadPiece->setColor(color);
  }

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
