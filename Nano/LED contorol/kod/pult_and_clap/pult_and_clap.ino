#include <FastLED.h>
#include <RCSwitch.h>
#include <EEPROM.h>

// LED Configuration
#define NUM_LEDS 40
#define DATA_PIN 6
CRGB leds[NUM_LEDS];

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
#define ASIA_START 1
#define ASIA_END 3
#define AFRICA_START 4
#define AFRICA_END 10
#define EUROPE_START 11
#define EUROPE_END 15
#define NORTH_AMERICA_START 16
#define NORTH_AMERICA_END 20
#define SOUTH_AMERICA_START 21
#define SOUTH_AMERICA_END 30
#define AUSTRALIA_START 31
#define AUSTRALIA_END 35
#define ANTARCTICA_START 36
#define ANTARCTICA_END 40

// Pins
#define MIC_PIN 7
#define RF_PIN 2

// State Management
enum Mode { FULL_LED, CONTINENT };
Mode currentMode = FULL_LED;
int currentContinent = 0; // 0=Asia, 1=Africa, 2=Europe, 3=North America, 4=South America, 5=Australia, 6=Antarctica
int currentEffect = 0; // 0=Auto, 1-23 for specific effects
int lastEffect = 1; // Store last non-auto effect
bool powerState = false;
uint8_t effectSpeed = 50; // 0-100 scale
unsigned long lastUpdate = 0;
unsigned long autoTimer = 0;
bool isTransitioning = false;
bool isFireAnimating = false; // Track fire animation state
int firePos = 0; // Position for fire effect animation

// Clap Detection
unsigned long lastClapTime = 0;
int clapCount = 0;
unsigned long clapWindowStart = 0;
const unsigned long CLAP_WINDOW = 3000; // 3 seconds

// Animation States
uint8_t hue = 0;
uint8_t pos = 0;
unsigned long lastAnimationUpdate = 0;
const int ANIMATION_INTERVAL = 50; // Base animation speed

RCSwitch mySwitch = RCSwitch();

struct Continent {
  int start;
  int end;
};

Continent continents[] = {
  {ASIA_START, ASIA_END},
  {AFRICA_START, AFRICA_END},
  {EUROPE_START, EUROPE_END},
  {NORTH_AMERICA_START, NORTH_AMERICA_END},
  {SOUTH_AMERICA_START, SOUTH_AMERICA_END},
  {AUSTRALIA_START, AUSTRALIA_END},
  {ANTARCTICA_START, ANTARCTICA_END}
};

void setup() {
  FastLED.addLeds<WS2811, DATA_PIN, RGB>(leds, NUM_LEDS);
  FastLED.setBrightness(255);
  
  pinMode(MIC_PIN, INPUT);
  Serial.begin(9600);
  mySwitch.enableReceive(0); // D2 pin
  
  // Load saved state
  currentMode = (Mode)EEPROM.read(0);
  currentContinent = EEPROM.read(1);
  currentEffect = EEPROM.read(2);
  lastEffect = EEPROM.read(3);
  powerState = EEPROM.read(4);
  
  if (!powerState) {
    FastLED.clear();
    FastLED.show();
  }
}

void loop() {
  handleRF();
  handleClaps();
  if (powerState && !isTransitioning) {
    updateAnimation();
  }
}

void handleRF() {
  if (mySwitch.available()) {
    long code = mySwitch.getReceivedValue();
    mySwitch.resetAvailable();
    
    if (code == 0) {
      Serial.println("Signalni o'qib bo'lmadi.");
      return;
    }
    
    Serial.print("Qabul qilingan kod: ");
    Serial.println(code);
    
    switch (code) {
      case POWER:
        togglePower();
        break;
      case MODE_PLUS:
        changeMode(1);
        break;
      case MODE_MINUS:
        changeMode(-1);
        break;
      case SPEED_PLUS:
        effectSpeed = min(100, effectSpeed + 10);
        break;
      case SPEED_MINUS:
        effectSpeed = max(0, effectSpeed - 10);
        break;
      case CONTINENT_PLUS:
        currentMode = CONTINENT;
        currentContinent = (currentContinent + 1) % 7;
        startTransition();
        break;
      case CONTINENT_MINUS:
        currentMode = CONTINENT;
        currentContinent = (currentContinent - 1 + 7) % 7;
        startTransition();
        break;
      case AUTO:
        currentMode = FULL_LED;
        currentEffect = 0;
        startTransition();
        break;
      case WHITE: case RED: case GREEN: case BLUE: case YELLOW: case LIGHT_BLUE: case PINK:
        setColorEffect(code);
        break;
    }
    saveState();
  }
}

void handleClaps() {
  if (digitalRead(MIC_PIN) == HIGH && millis() - lastClapTime > 200) {
    if (clapWindowStart == 0) {
      clapWindowStart = millis();
    }
    clapCount++;
    lastClapTime = millis();
  }
  
  if (clapWindowStart > 0 && millis() - clapWindowStart > CLAP_WINDOW) {
    processClaps();
    clapCount = 0;
    clapWindowStart = 0;
  }
}

void processClaps() {
  if (clapCount == 1) {
    togglePower();
  } else if (clapCount == 2) {
    if (currentMode == CONTINENT) {
      currentContinent = (currentContinent + 1) % 7;
    } else {
      changeMode(1);
    }
    startTransition();
  } else if (clapCount == 3) {
    if (currentMode == CONTINENT) {
      currentContinent = (currentContinent - 1 + 7) % 7;
    } else {
      changeMode(-1);
    }
    startTransition();
  }
  saveState();
}

void togglePower() {
  powerState = !powerState;
  isFireAnimating = false; // Reset fire animation on power toggle
  if (!powerState) {
    FastLED.clear();
    FastLED.show();
  } else {
    startTransition();
  }
  saveState();
}

void changeMode(int direction) {
  currentMode = FULL_LED;
  if (currentEffect == 0) {
    currentEffect = lastEffect;
  }
  currentEffect = (currentEffect + direction + 24) % 24;
  if (currentEffect == 0) {
    lastEffect = (lastEffect == 0) ? 1 : lastEffect;
  } else {
    lastEffect = currentEffect;
  }
  isFireAnimating = false; // Reset fire animation on mode change
  startTransition();
}

void startTransition() {
  isTransitioning = true;
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
  FastLED.show();
  
  // Flash transition
  for (int i = 0; i < 5; i++) {
    setLeds(CHSV(random8(), 255, 255));
    FastLED.show();
    delay(50); // Reduced delay for faster transition
    setLeds(CRGB::Black);
    FastLED.show();
    delay(50);
  }
  isTransitioning = false;
}

void setColorEffect(long code) {
  currentMode = FULL_LED;
  isFireAnimating = false; // Reset fire animation
  switch (code) {
    case WHITE: currentEffect = 1; break;
    case RED: currentEffect = 2; break;
    case GREEN: currentEffect = 3; break;
    case YELLOW: currentEffect = 4; break;
    case LIGHT_BLUE: currentEffect = 5; break;
    case PINK: currentEffect = 6; break;
  }
  lastEffect = currentEffect;
  startTransition();
}

void saveState() {
  EEPROM.update(0, currentMode);
  EEPROM.update(1, currentContinent);
  EEPROM.update(2, currentEffect);
  EEPROM.update(3, lastEffect);
  EEPROM.update(4, powerState);
}

void updateAnimation() {
  if (millis() - lastAnimationUpdate < ANIMATION_INTERVAL * (100 - effectSpeed) / 100) return;
  lastAnimationUpdate = millis();
  
  if (currentEffect == 0 && millis() - autoTimer > 10000) {
    currentEffect = (lastEffect % 23) + 1;
    lastEffect = currentEffect;
    isFireAnimating = false; // Reset fire animation
    autoTimer = millis();
  }
  
  FastLED.clear();
  
  if (isFireAnimating) {
    fireEffect();
  } else {
    switch (currentEffect) {
      case 0: // Auto (handled above)
        break;
      case 1: setLeds(CRGB::White); break;
      case 2: setLeds(CRGB::Red); break;
      case 3: setLeds(CRGB::Green); break;
      case 4: setLeds(CRGB::Yellow); break;
      case 5: setLeds(CRGB(135, 206, 250)); break; // LightBlue
      case 6: setLeds(CRGB::Pink); break;
      case 7: oceanEffect(); break;
      case 8: startFireEffect(); break;
      case 9: colorWipe(); break;
      case 10: rainbow(); break;
      case 11: theaterChase(); break;
      case 12: runningLights(); break;
      case 13: fadeInOut(); break;
      case 14: twinkle(); break;
      case 15: fireEffect(); break; // Note: Effect 15 is also fire, so we'll redirect to startFireEffect
      case 16: pulse(); break;
      case 17: strobe(); break;
      case 18: colorCycle(); break;
      case 19: sparkle(); break;
      case 20: breathe(); break;
      case 21: meteorRain(); break;
      case 22: cylon(); break;
      case 23: confetti(); break;
    }
  }
  FastLED.show();
}

void setLeds(CRGB color) {
  if (currentMode == FULL_LED) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = color;
    }
  } else {
    for (int i = continents[currentContinent].start; i <= continents[currentContinent].end; i++) {
      leds[i] = color;
    }
  }
}

void setLeds(CHSV color) {
  setLeds(CRGB(color)); // Convert CHSV to CRGB
}

void startFireEffect() {
  isFireAnimating = true;
  firePos = 0; // Start from beginning
}

void fireEffect() {
  int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
  int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
  
  // Clear LEDs
  setLeds(CRGB::Black);
  
  // Animate flame from start to end
  for (int i = 0; i <= firePos && (start + i) <= end; i++) {
    uint8_t heat = random8(160, 255); // Random heat value for flame
    leds[start + i] = CRGB(HeatColor(heat));
  }
  
  firePos++;
  
  // When animation reaches the end, switch to last mode or mode 1
  if (start + firePos > end) {
    isFireAnimating = false;
    currentEffect = (lastEffect == 0 || lastEffect == 8 || lastEffect == 15) ? 1 : lastEffect;
  }
}

void oceanEffect() {
  uint8_t beat = beatsin8(10, 0, 255);
  setLeds(CHSV(160 + (beat / 16), 200, beat));
}

void colorWipe() {
  static int pos = 0;
  setLeds(CRGB::Black);
  int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
  int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
  leds[(start + pos) % (end + 1)] = CRGB(CHSV(hue++, 255, 255));
  pos = (pos + 1) % (end - start + 1);
}

void rainbow() {
  setLeds(CHSV(hue++, 255, 255));
}

void theaterChase() {
  static int pos = 0;
  setLeds(CRGB::Black);
  int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
  int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
  for (int i = start; i <= end; i += 3) {
    leds[(i + pos) % (end + 1)] = CRGB(CHSV(hue, 255, 255));
  }
  pos = (pos + 1) % 3;
}

void runningLights() {
  static int pos = 0;
  setLeds(CRGB::Black);
  int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
  int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
  leds[(start + pos) % (end + 1)] = CRGB(CHSV(hue, 255, 255));
  pos = (pos + 1) % (end - start + 1);
}

void fadeInOut() {
  uint8_t brightness = beatsin8(10, 0, 255);
  setLeds(CHSV(hue, 255, brightness));
}

void twinkle() {
  setLeds(CRGB::Black);
  int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
  int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
  leds[random(start, end + 1)] = CRGB(CHSV(random8(), 255, 255));
}

void pulse() {
  uint8_t brightness = beatsin8(20, 64, 255);
  setLeds(CHSV(hue, 255, brightness));
}

void strobe() {
  static bool on = false;
  on = !on;
  setLeds(on ? CRGB(CHSV(hue, 255, 255)) : CRGB::Black);
}

void colorCycle() {
  setLeds(CHSV(hue++, 255, 255));
}

void sparkle() {
  setLeds(CRGB::Black);
  int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
  int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
  leds[random(start, end + 1)] = CRGB::White;
}

void breathe() {
  uint8_t brightness = beatsin8(5, 0, 255);
  setLeds(CHSV(hue, 255, brightness));
}

void meteorRain() {
  static int pos = 0;
  setLeds(CRGB::Black);
  int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
  int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
  for (int i = 0; i < 10; i++) {
    if (pos - i >= start && pos - i <= end) {
      leds[pos - i] = CRGB(CHSV(hue, 255, 255 - (i * 25)));
    }
  }
  pos = (pos + 1) % (end - start + 1);
}

void cylon() {
  static int pos = 0;
  static bool forward = true;
  setLeds(CRGB::Black);
  int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
  int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
  leds[start + pos] = CRGB(CHSV(hue, 255, 255));
  if (forward) {
    pos++;
    if (pos >= end - start) forward = false;
  } else {
    pos--;
    if (pos <= 0) forward = true;
  }
}

void confetti() {
  setLeds(CRGB::Black);
  int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
  int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
  leds[random(start, end + 1)] = CRGB(CHSV(random8(), 255, 255));
}