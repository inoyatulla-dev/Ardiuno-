#include "FastLED.h"
#include <EEPROM.h>
#include <RCSwitch.h>
#include <SoundSensor.h> // Assuming a library for sound/clap detection

#define NUM_LEDS 280
CRGB leds[NUM_LEDS];
#define LED_PIN 6
#define BUTTON_PIN 2
#define SOUND_PIN A0 // Microphone module pin

// RF Remote Codes
#define POWER 429057
#define MODE_PLUS 429061
#define MODE_MINUS 429067
#define SPEED_PLUS 429065
#define SPEED_MINUS 429063
#define CONTINENT_PLUS 429066
#define CONTINENT_MINUS 429069
#define WHITE 429070
#define RED 429072
#define GREEN 429073
#define BLUE 429047
#define YELLOW 429075
#define LIGHT_BLUE 429076
#define PINK 429077
#define AUTO 429064

// Continent LED ranges
#define ASIA_START 0
#define ASIA_END 50
#define AFRICA_START 51
#define AFRICA_END 100
#define EUROPE_START 101
#define EUROPE_END 150
#define NORTH_AMERICA_START 151
#define NORTH_AMERICA_END 200
#define SOUTH_AMERICA_START 201
#define SOUTH_AMERICA_END 230
#define AUSTRALIA_START 231
#define AUSTRALIA_END 260
#define ANTARCTICA_START 261
#define ANTARCTICA_END 279

RCSwitch mySwitch = RCSwitch();
SoundSensor soundSensor(SOUND_PIN); // Initialize sound sensor

// Mode and state variables
enum Mode { FULL_LED, CONTINENT };
Mode currentMode = FULL_LED;
byte selectedEffect = 0;
byte currentContinent = 0;
int effectSpeed = 50; // Default speed
bool powerState = true; // LEDs on by default
unsigned long lastClapTime = 0;
int clapCount = 0;
const unsigned long CLAP_WINDOW = 3000; // 3 seconds for clap counting

struct State {
  Mode mode;
  byte effect;
  byte continent;
  bool power;
  int speed;
};

void setup() {
  FastLED.addLeds<WS2811, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonInterrupt, CHANGE);
  mySwitch.enableReceive(0); // RF receiver on interrupt 0 (D2)
  Serial.begin(9600);
  soundSensor.begin(); // Initialize sound sensor
  loadState();
}

void loop() {
  // Handle RF remote
  if (mySwitch.available()) {
    handleRFCommand(mySwitch.getReceivedValue());
    mySwitch.resetAvailable();
  }

  // Handle clap detection
  if (soundSensor.detectClap()) {
    unsigned long currentTime = millis();
    if (currentTime - lastClapTime < CLAP_WINDOW) {
      clapCount++;
    } else {
      clapCount = 1; // Reset count if outside window
    }
    lastClapTime = currentTime;

    // Process claps after window or max claps
    if (clapCount >= 3 || (currentTime - lastClapTime >= CLAP_WINDOW && clapCount > 0)) {
      handleClapCommands(clapCount);
      clapCount = 0;
    }
  }

  if (!powerState) {
    setAll(0, 0, 0);
    return;
  }

  // Execute effect based on mode
  if (currentMode == FULL_LED) {
    runEffect(selectedEffect, 0, NUM_LEDS - 1);
  } else {
    runContinentEffect();
  }
}

void buttonInterrupt() {
  if (digitalRead(BUTTON_PIN) == HIGH) {
    selectedEffect = (selectedEffect + 1) % 24;
    saveState();
  }
}

void handleRFCommand(long code) {
  switch (code) {
    case POWER:
      powerState = !powerState;
      if (!powerState) setAll(0, 0, 0);
      break;
    case MODE_PLUS:
      selectedEffect = (selectedEffect + 1) % 24;
      break;
    case MODE_MINUS:
      selectedEffect = (selectedEffect == 0) ? 23 : selectedEffect - 1;
      break;
    case SPEED_PLUS:
      effectSpeed = max(10, effectSpeed - 10);
      break;
    case SPEED_MINUS:
      effectSpeed = min(200, effectSpeed + 10);
      break;
    case CONTINENT_PLUS:
      currentMode = CONTINENT;
      currentContinent = (currentContinent + 1) % 7;
      break;
    case CONTINENT_MINUS:
      currentMode = CONTINENT;
      currentContinent = (currentContinent == 0) ? 6 : currentContinent - 1;
      break;
    case AUTO:
      currentMode = FULL_LED;
      selectedEffect = 0;
      break;
    case WHITE:
      selectedEffect = 1;
      break;
    case RED:
      selectedEffect = 2;
      break;
    case GREEN:
      selectedEffect = 3;
      break;
    case YELLOW:
      selectedEffect = 4;
      break;
    case LIGHT_BLUE:
      selectedEffect = 5;
      break;
    case PINK:
      selectedEffect = 6;
      break;
  }
  saveState();
}

void handleClapCommands(int count) {
  switch (count) {
    case 1:
      powerState = !powerState;
      if (!powerState) setAll(0, 0, 0);
      break;
    case 2:
      currentMode = CONTINENT;
      currentContinent = (currentContinent + 1) % 7;
      break;
    case 3:
      currentMode = CONTINENT;
      currentContinent = (currentContinent == 0) ? 6 : currentContinent - 1;
      break;
  }
  saveState();
}

void loadState() {
  State state;
  EEPROM.get(0, state);
  if (state.mode <= CONTINENT && state.effect < 24 && state.continent < 7) {
    currentMode = state.mode;
    selectedEffect = state.effect;
    currentContinent = state.continent;
    powerState = state.power;
    effectSpeed = state.speed;
  }
}

void saveState() {
  State state = {currentMode, selectedEffect, currentContinent, powerState, effectSpeed};
  EEPROM.put(0, state);
}

void runContinentEffect() {
  int start, end;
  getContinentRange(currentContinent, start, end);
  // Turn off all LEDs outside the current continent
  for (int i = 0; i < NUM_LEDS; i++) {
    if (i < start || i > end) {
      setPixel(i, 0, 0, 0);
    }
  }
  runEffect(selectedEffect, start, end);
}

void getContinentRange(byte continent, int &start, int &end) {
  switch (continent) {
    case 0: // Asia
      start = ASIA_START;
      end = ASIA_END;
      break;
    case 1: // Africa
      start = AFRICA_START;
      end = AFRICA_END;
      break;
    case 2: // Europe
      start = EUROPE_START;
      end = EUROPE_END;
      break;
    case 3: // North America
      start = NORTH_AMERICA_START;
      end = NORTH_AMERICA_END;
      break;
    case 4: // South America
      start = SOUTH_AMERICA_START;
      end = SOUTH_AMERICA_END;
      break;
    case 5: // Australia
      start = AUSTRALIA_START;
      end = AUSTRALIA_END;
      break;
    case 6: // Antarctica
      start = ANTARCTICA_START;
      end = ANTARCTICA_END;
      break;
  }
}

void runEffect(byte effect, int start, int end) {
  static unsigned long lastAutoSwitch = 0;
  if (effect == 0) { // Auto mode
    if (millis() - lastAutoSwitch > 10000) {
      selectedEffect = (selectedEffect % 23) + 1; // Cycle through effects 1-23
      lastAutoSwitch = millis();
      saveState();
    }
  }

  switch (effect) {
    case 0: // Auto handled above
      break;
    case 1: // White
      for (int i = start; i <= end; i++) setPixel(i, 255, 255, 255);
      showStrip();
      break;
    case 2: // Red
      for (int i = start; i <= end; i++) setPixel(i, 255, 0, 0);
      showStrip();
      break;
    case 3: // Green
      for (int i = start; i <= end; i++) setPixel(i, 0, 255, 0);
      showStrip();
      break;
    case 4: // Yellow
      for (int i = start; i <= end; i++) setPixel(i, 255, 255, 0);
      showStrip();
      break;
    case 5: // Light Blue
      for (int i = start; i <= end; i++) setPixel(i, 0, 191, 255);
      showStrip();
      break;
    case 6: // Pink
      for (int i = start; i <= end; i++) setPixel(i, 255, 20, 147);
      showStrip();
      break;
    case 7: // Ocean Waves
      OceanWaves(start, end, effectSpeed);
      break;
    case 8: // Fire (already implemented)
      Fire(55, 120, effectSpeed, start, end);
      break;
    case 9: // Color Wipe (already implemented)
      colorWipe(0, 255, 0, effectSpeed, start, end);
      colorWipe(0, 0, 0, effectSpeed, start, end);
      break;
    case 10: // Rainbow (already implemented)
      rainbowCycle(effectSpeed, start, end);
      break;
    case 11: // Theater Chase (already implemented)
      theaterChase(255, 0, 0, effectSpeed, start, end);
      break;
    case 12: // Running Lights (already implemented)
      RunningLights(255, 0, 0, effectSpeed, start, end);
      break;
    case 13: // Fade In/Out (already implemented)
      FadeInOut(255, 0, 0, start, end);
      break;
    case 14: // Twinkle (already implemented)
      Twinkle(255, 0, 0, 10, effectSpeed, false, start, end);
      break;
    case 15: // Fire (already implemented, repeated for consistency)
      Fire(55, 120, effectSpeed, start, end);
      break;
    case 16: // Pulse
      Pulse(255, 0, 0, effectSpeed, start, end);
      break;
    case 17: // Strobe (already implemented)
      Strobe(255, 255, 255, 10, effectSpeed, 1000, start, end);
      break;
    case 18: // Color Cycle
      ColorCycle(effectSpeed, start, end);
      break;
    case 19: // Sparkle (already implemented)
      Sparkle(255, 255, 255, effectSpeed, start, end);
      break;
    case 20: // Breathe
      Breathe(255, 0, 0, effectSpeed, start, end);
      break;
    case 21: // Meteor Rain (already implemented)
      meteorRain(255, 255, 255, 10, 64, true, effectSpeed, start, end);
      break;
    case 22: // Cylon (already implemented)
      CylonBounce(255, 0, 0, 4, effectSpeed, 50, start, end);
      break;
    case 23: // Confetti
      Confetti(effectSpeed, start, end);
      break;
  }
}

// Modified FastLED functions to support start/end ranges
void setPixel(int Pixel, byte red, byte green, byte blue) {
  leds[Pixel].r = red;
  leds[Pixel].g = green;
  leds[Pixel].b = blue;
}

void setAll(byte red, byte green, byte blue, int start, int end) {
  for (int i = start; i <= end; i++) {
    setPixel(i, red, green, blue);
  }
  showStrip();
}

void showStrip() {
  FastLED.show();
}

void RGBLoop(int start, int end) {
  for (int j = 0; j < 3; j++) {
    for (int k = 0; k < 256; k++) {
      switch (j) {
        case 0: setAll(k, 0, 0, start, end); break;
        case 1: setAll(0, k, 0, start, end); break;
        case 2: setAll(0, 0, k, start, end); break;
      }
      delay(3);
    }
    for (int k = 255; k >= 0; k--) {
      switch (j) {
        case 0: setAll(k, 0, 0, start, end); break;
        case 1: setAll(0, k, 0, start, end); break;
        case 2: setAll(0, 0, k, start, end); break;
      }
      delay(3);
    }
  }
}

void FadeInOut(byte red, byte green, byte blue, int start, int end) {
  float r, g, b;
  for (int k = 0; k < 256; k++) {
    r = (k / 256.0) * red;
    g = (k / 256.0) * green;
    b = (k / 256.0) * blue;
    setAll(r, g, b, start, end);
  }
  for (int k = 255; k >= 0; k -= 2) {
    r = (k / 256.0) * red;
    g = (k / 256.0) * green;
    b = (k / 256.0) * blue;
    setAll(r, g, b, start, end);
  }
}

void Strobe(byte red, byte green, byte blue, int StrobeCount, int FlashDelay, int EndPause, int start, int end) {
  for (int j = 0; j < StrobeCount; j++) {
    setAll(red, green, blue, start, end);
    delay(FlashDelay);
    setAll(0, 0, 0, start, end);
    delay(FlashDelay);
  }
  delay(EndPause);
}

void HalloweenEyes(byte red, byte green, byte blue, int EyeWidth, int EyeSpace, boolean Fade, int Steps, int FadeDelay, int EndPause, int start, int end) {
  randomSeed(analogRead(0));
  int range = end - start + 1;
  int StartPoint = random(start, end - (2 * EyeWidth) - EyeSpace + 1);
  int Start2ndEye = StartPoint + EyeWidth + EyeSpace;

  for (int i = 0; i < EyeWidth; i++) {
    if (StartPoint + i <= end) setPixel(StartPoint + i, red, green, blue);
    if (Start2ndEye + i <= end) setPixel(Start2ndEye + i, red, green, blue);
  }
  showStrip();

  if (Fade) {
    float r, g, b;
    for (int j = Steps; j >= 0; j--) {
      r = j * (red / Steps);
      g = j * (green / Steps);
      b = j * (blue / Steps);
      for (int i = 0; i < EyeWidth; i++) {
        if (StartPoint + i <= end) setPixel(StartPoint + i, r, g, b);
        if (Start2ndEye + i <= end) setPixel(Start2ndEye + i, r, g, b);
      }
      showStrip();
      delay(FadeDelay);
    }
  }
  setAll(0, 0, 0, start, end);
  delay(EndPause);
}

void CylonBounce(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay, int start, int end) {
  for (int i = start; i <= end - EyeSize - 2; i++) {
    setAll(0, 0, 0, start, end);
    setPixel(i, red / 10, green / 10, blue / 10);
    for (int j = 1; j <= EyeSize; j++) {
      setPixel(i + j, red, green, blue);
    }
    setPixel(i + EyeSize + 1, red / 10, green / 10, blue / 10);
    showStrip();
    delay(SpeedDelay);
  }
  delay(ReturnDelay);
  for (int i = end - EyeSize - 2; i >= start; i--) {
    setAll(0, 0, 0, start, end);
    setPixel(i, red / 10, green / 10, blue / 10);
    for (int j = 1; j <= EyeSize; j++) {
      setPixel(i + j, red, green, blue);
    }
    setPixel(i + EyeSize + 1, red / 10, green / 10, blue / 10);
    showStrip();
    delay(SpeedDelay);
  }
  delay(ReturnDelay);
}

void NewKITT(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay, int start, int end) {
  RightToLeft(red, green, blue, EyeSize, SpeedDelay, ReturnDelay, start, end);
  LeftToRight(red, green, blue, EyeSize, SpeedDelay, ReturnDelay, start, end);
  OutsideToCenter(red, green, blue, EyeSize, SpeedDelay, ReturnDelay, start, end);
  CenterToOutside(red, green, blue, EyeSize, SpeedDelay, ReturnDelay, start, end);
  LeftToRight(red, green, blue, EyeSize, SpeedDelay, ReturnDelay, start, end);
  RightToLeft(red, green, blue, EyeSize, SpeedDelay, ReturnDelay, start, end);
  OutsideToCenter(red, green, blue, EyeSize, SpeedDelay, ReturnDelay, start, end);
  CenterToOutside(red, green, blue, EyeSize, SpeedDelay, ReturnDelay, start, end);
}

void CenterToOutside(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay, int start, int end) {
  int mid = (start + end) / 2;
  for (int i = mid; i >= start; i--) {
    setAll(0, 0, 0, start, end);
    setPixel(i, red / 10, green / 10, blue / 10);
    for (int j = 1; j <= EyeSize; j++) {
      if (i + j <= end) setPixel(i + j, red, green, blue);
    }
    if (i + EyeSize + 1 <= end) setPixel(i + EyeSize + 1, red / 10, green / 10, blue / 10);
    setPixel(end - (i - start), red / 10, green / 10, blue / 10);
    for (int j = 1; j <= EyeSize; j++) {
      if (end - (i - start) - j >= start) setPixel(end - (i - start) - j, red, green, blue);
    }
    if (end - (i - start) - EyeSize - 1 >= start) setPixel(end - (i - start) - EyeSize - 1, red / 10, green / 10, blue / 10);
    showStrip();
    delay(SpeedDelay);
  }
  delay(ReturnDelay);
}

void OutsideToCenter(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay, int start, int end) {
  int mid = (start + end) / 2;
  for (int i = start; i <= mid; i++) {
    setAll(0, 0, 0, start, end);
    setPixel(i, red / 10, green / 10, blue / 10);
    for (int j = 1; j <= EyeSize; j++) {
      if (i + j <= end) setPixel(i + j, red, green, blue);
    }
    if (i + EyeSize + 1 <= end) setPixel(i + EyeSize + 1, red / 10, green / 10, blue / 10);
    setPixel(end - (i - start), red / 10, green / 10, blue / 10);
    for (int j = 1; j <= EyeSize; j++) {
      if (end - (i - start) - j >= start) setPixel(end - (i - start) - j, red, green, blue);
    }
    if (end - (i - start) - EyeSize - 1 >= start) setPixel(end - (i - start) - EyeSize - 1, red / 10, green / 10, blue / 10);
    showStrip();
    delay(SpeedDelay);
  }
  delay(ReturnDelay);
}

void LeftToRight(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay, int start, int end) {
  for (int i = start; i <= end - EyeSize - 2; i++) {
    setAll(0, 0, 0, start, end);
    setPixel(i, red / 10, green / 10, blue / 10);
    for (int j = 1; j <= EyeSize; j++) {
      setPixel(i + j, red, green, blue);
    }
    setPixel(i + EyeSize + 1, red / 10, green / 10, blue / 10);
    showStrip();
    delay(SpeedDelay);
  }
  delay(ReturnDelay);
}

void RightToLeft(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay, int start, int end) {
  for (int i = end - EyeSize - 2; i >= start; i--) {
    setAll(0, 0, 0, start, end);
    setPixel(i, red / 10, green / 10, blue / 10);
    for (int j = 1; j <= EyeSize; j++) {
      setPixel(i + j, red, green, blue);
    }
    setPixel(i + EyeSize + 1, red / 10, green / 10, blue / 10);
    showStrip();
    delay(SpeedDelay);
  }
  delay(ReturnDelay);
}

void Twinkle(byte red, byte green, byte blue, int Count, int SpeedDelay, boolean OnlyOne, int start, int end) {
  setAll(0, 0, 0, start, end);
  for (int i = 0; i < Count; i++) {
    setPixel(random(start, end + 1), red, green, blue);
    showStrip();
    delay(SpeedDelay);
    if (OnlyOne) {
      setAll(0, 0, 0, start, end);
    }
  }
  delay(SpeedDelay);
}

void TwinkleRandom(int Count, int SpeedDelay, boolean OnlyOne, int start, int end) {
  setAll(0, 0, 0, start, end);
  for (int i = 0; i < Count; i++) {
    setPixel(random(start, end + 1), random(0, 255), random(0, 255), random(0, 255));
    showStrip();
    delay(SpeedDelay);
    if (OnlyOne) {
      setAll(0, 0, 0, start, end);
    }
  }
  delay(SpeedDelay);
}

void Sparkle(byte red, byte green, byte blue, int SpeedDelay, int start, int end) {
  int Pixel = random(start, end + 1);
  setPixel(Pixel, red, green, blue);
  showStrip();
  delay(SpeedDelay);
  setPixel(Pixel, 0, 0, 0);
}

void SnowSparkle(byte red, byte green, byte blue, int SparkleDelay, int SpeedDelay, int start, int end) {
  setAll(red, green, blue, start, end);
  int Pixel = random(start, end + 1);
  setPixel(Pixel, 255, 255, 255);
  showStrip();
  delay(SparkleDelay);
  setPixel(Pixel, red, green, blue);
  showStrip();
  delay(SpeedDelay);
}

void RunningLights(byte red, byte green, byte blue, int WaveDelay, int start, int end) {
  int Position = 0;
  for (int i = 0; i < (end - start + 1) * 2; i++) {
    Position++;
    for (int j = start; j <= end; j++) {
      setPixel(j, ((sin((j - start) + Position) * 127 + 128) / 255) * red,
               ((sin((j - start) + Position) * 127 + 128) / 255) * green,
               ((sin((j - start) + Position) * 127 + 128) / 255) * blue);
    }
    showStrip();
    delay(WaveDelay);
  }
}

void colorWipe(byte red, byte green, byte blue, int SpeedDelay, int start, int end) {
  for (int i = start; i <= end; i++) {
    setPixel(i, red, green, blue);
    showStrip();
    delay(SpeedDelay);
  }
}

void rainbowCycle(int SpeedDelay, int start, int end) {
  byte *c;
  uint16_t i, j;
  for (j = 0; j < 256 * 5; j++) {
    for (i = start; i <= end; i++) {
      c = Wheel(((i - start) * 256 / (end - start + 1) + j) & 255);
      setPixel(i, *c, *(c + 1), *(c + 2));
    }
    showStrip();
    delay(SpeedDelay);
  }
}

byte *Wheel(byte WheelPos) {
  static byte c[3];
  if (WheelPos < 85) {
    c[0] = WheelPos * 3;
    c[1] = 255 - WheelPos * 3;
    c[2] = 0;
  } else if (WheelPos < 170) {
    WheelPos -= 85;
    c[0] = 255 - WheelPos * 3;
    c[1] = 0;
    c[2] = WheelPos * 3;
  } else {
    WheelPos -= 170;
    c[0] = 0;
    c[1] = WheelPos * 3;
    c[2] = 255 - WheelPos * 3;
  }
  return c;
}

void theaterChase(byte red, byte green, byte blue, int SpeedDelay, int start, int end) {
  for (int j = 0; j < 10; j++) {
    for (int q = 0; q < 3; q++) {
      for (int i = start; i <= end; i += 3) {
        if (i + q <= end) setPixel(i + q, red, green, blue);
      }
      showStrip();
      delay(SpeedDelay);
      for (int i = start; i <= end; i += 3) {
        if (i + q <= end) setPixel(i + q, 0, 0, 0);
      }
    }
  }
}

void theaterChaseRainbow(int SpeedDelay, int start, int end) {
  byte *c;
  for (int j = 0; j < 256; j++) {
    for (int q = 0; q < 3; q++) {
      for (int i = start; i <= end; i += 3) {
        if (i + q <= end) {
          c = Wheel((i - start + j) % 255);
          setPixel(i + q, *c, *(c + 1), *(c + 2));
        }
      }
      showStrip();
      delay(SpeedDelay);
      for (int i = start; i <= end; i += 3) {
        if (i + q <= end) setPixel(i + q, 0, 0, 0);
      }
    }
  }
}

void Fire(int Cooling, int Sparking, int SpeedDelay, int start, int end) {
  static byte heat[NUM_LEDS];
  int cooldown;
  for (int i = start; i <= end; i++) {
    cooldown = random(0, ((Cooling * 10) / (end - start + 1)) + 2);
    if (cooldown > heat[i]) {
      heat[i] = 0;
    } else {
      heat[i] = heat[i] - cooldown;
    }
  }
  for (int k = end; k >= start + 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
  }
  if (random(255) < Sparking) {
    int y = random(start, start + 7);
    if (y <= end) heat[y] = heat[y] + random(160, 255);
  }
  for (int j = start; j <= end; j++) {
    setPixelHeatColor(j, heat[j]);
  }
  showStrip();
  delay(SpeedDelay);
}

void setPixelHeatColor(int Pixel, byte temperature) {
  byte t192 = round((temperature / 255.0) * 191);
  byte heatramp = t192 & 0x3F;
  heatramp <<= 2;
  if (t192 > 0x80) {
    setPixel(Pixel, 255, 255, heatramp);
  } else if (t192 > 0x40) {
    setPixel(Pixel, 255, heatramp, 0);
  } else {
    setPixel(Pixel, heatramp, 0, 0);
  }
}

void BouncingColoredBalls(int BallCount, byte colors[][3], boolean continuous, int start, int end) {
  float Gravity = -9.81;
  int StartHeight = 1;
  float Height[BallCount];
  float ImpactVelocityStart = sqrt(-2 * Gravity * StartHeight);
  float ImpactVelocity[BallCount];
  float TimeSinceLastBounce[BallCount];
  int Position[BallCount];
  long ClockTimeSinceLastBounce[BallCount];
  float Dampening[BallCount];
  boolean ballBouncing[BallCount];
  boolean ballsStillBouncing = true;

  for (int i = 0; i < BallCount; i++) {
    ClockTimeSinceLastBounce[i] = millis();
    Height[i] = StartHeight;
    Position[i] = start;
    ImpactVelocity[i] = ImpactVelocityStart;
    TimeSinceLastBounce[i] = 0;
    Dampening[i] = 0.90 - float(i) / pow(BallCount, 2);
    ballBouncing[i] = true;
  }

  while (ballsStillBouncing) {
    for (int i = 0; i < BallCount; i++) {
      TimeSinceLastBounce[i] = millis() - ClockTimeSinceLastBounce[i];
      Height[i] = 0.5 * Gravity * pow(TimeSinceLastBounce[i] / 1000, 2.0) + ImpactVelocity[i] * TimeSinceLastBounce[i] / 1000;
      if (Height[i] < 0) {
        Height[i] = 0;
        ImpactVelocity[i] = Dampening[i] * ImpactVelocity[i];
        ClockTimeSinceLastBounce[i] = millis();
        if (ImpactVelocity[i] < 0.01) {
          if (continuous) {
            ImpactVelocity[i] = ImpactVelocityStart;
          } else {
            ballBouncing[i] = false;
          }
        }
      }
      Position[i] = round(Height[i] * (end - start) / StartHeight) + start;
    }

    ballsStillBouncing = false;
    for (int i = 0; i < BallCount; i++) {
      if (Position[i] <= end) setPixel(Position[i], colors[i][0], colors[i][1], colors[i][2]);
      if (ballBouncing[i]) ballsStillBouncing = true;
    }
    showStrip();
    setAll(0, 0, 0, start, end);
  }
}

void meteorRain(byte red, byte green, byte blue, byte meteorSize, byte meteorTrailDecay, boolean meteorRandomDecay, int SpeedDelay, int start, int end) {
  setAll(0, 0, 0, start, end);
  for (int i = start; i < end + (end - start + 1); i++) {
    for (int j = start; j <= end; j++) {
      if ((!meteorRandomDecay) || (random(10) > 5)) {
        leds[j].fadeToBlackBy(meteorTrailDecay);
      }
    }
    for (int j = 0; j < meteorSize; j++) {
      if ((i - j <= end) && (i - j >= start)) {
        setPixel(i - j, red, green, blue);
      }
    }
    showStrip();
    delay(SpeedDelay);
  }
}

// New effects
void OceanWaves(int start, int end, int SpeedDelay) {
  // Simulate ocean waves with blue and green hues moving back and forth
  for (int j = 0; j < 256; j++) {
    for (int i = start; i <= end; i++) {
      byte blue = (sin((i - start + j) * 0.1) * 127 + 128);
      byte green = (sin((i - start + j) * 0.1 + 1.0) * 64 + 64);
      setPixel(i, 0, green, blue);
    }
    showStrip();
    delay(SpeedDelay);
  }
}

void Pulse(byte red, byte green, byte blue, int SpeedDelay, int start, int end) {
  for (int k = 0; k < 256; k += 2) {
    float r = (sin(k * 0.05) * 127 + 128) * red / 255.0;
    float g = (sin(k * 0.05) * 127 + 128) * green / 255.0;
    float b = (sin(k * 0.05) * 127 + 128) * blue / 255.0;
    setAll(r, g, b, start, end);
    delay(SpeedDelay);
  }
}

void ColorCycle(int SpeedDelay, int start, int end) {
  byte *c;
  for (int j = 0; j < 256; j++) {
    for (int i = start; i <= end; i++) {
      c = Wheel(j);
      setPixel(i, *c, *(c + 1), *(c + 2));
    }
    showStrip();
    delay(SpeedDelay);
  }
}

void Breathe(byte red, byte green, byte blue, int SpeedDelay, int start, int end) {
  for (int k = 0; k < 256; k += 2) {
    float r = (sin(k * 0.05) * 127 + 128) * red / 255.0;
    float g = (sin(k * 0.05) * 127 + 128) * green / 255.0;
    float b = (sin(k * 0.05) * 127 + 128) * blue / 255.0;
    setAll(r, g, b, start, end);
    delay(SpeedDelay);
  }
}

void Confetti(int SpeedDelay, int start, int end) {
  setAll(0, 0, 0, start, end);
  for (int i = 0; i < 20; i++) {
    setPixel(random(start, end + 1), random(0, 255), random(0, 255), random(0, 255));
    showStrip();
    delay(SpeedDelay);
    setAll(0, 0, 0, start, end);
  }
}