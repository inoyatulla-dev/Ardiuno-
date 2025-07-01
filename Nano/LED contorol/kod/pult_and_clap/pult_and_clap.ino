#include <FastLED.h>
#include <RCSwitch.h>
#include <EEPROM.h>

// LED Configuration
#define NUM_LEDS 52
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
#define ASIA_START 0
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
#define ANTARCTICA_END 52 // Adjusted to NUM_LEDS - 1

// Pins
#define MIC_PIN 7
#define RF_PIN 2

// State Management
enum Mode { FULL_LED, CONTINENT };
Mode currentMode = FULL_LED;
int currentContinent = 0; // 0=Asia, 1=Africa, 2=Europe, 3=North America, 4=South America, 5=Australia, 6=Antarctica
int currentEffect = 0; // 0=Auto, 1-23 for specific effects
int lastEffect = 1; // Store last non-auto effect
bool powerState = false; // Start powered off
uint8_t effectSpeed = 50; // 0-100 scale
unsigned long lastAnimationUpdate = 0;
const int ANIMATION_INTERVAL = 20; // Reduced for faster animations
unsigned long autoTimer = 0;
bool isTransitioning = false;
int firePos = 0; // Position for fire effect animation
uint8_t hue = 0; // Global hue for animations
uint8_t pos = 0; // Global position for animations

// Clap Detection
unsigned long lastClapTime = 0;
int clapCount = 0;
unsigned long clapWindowStart = 0;
const unsigned long CLAP_WINDOW = 3000; // 3 seconds
const unsigned long CLAP_DEBOUNCE = 200; // Debounce time for claps

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
  mySwitch.enableReceive(0); // D2 pin, interrupt 0
  Serial.println("433 MHz RF signal receiver ready...");
  
  // Load last effect from EEPROM
  currentEffect = EEPROM.read(0);
  if (currentEffect > 23) { // Validate effect (0-23)
    currentEffect = 0; // Default to Auto mode if invalid
  }
  lastEffect = (currentEffect == 0) ? 1 : currentEffect; // Set lastEffect
  
  // Ensure LEDs are off on startup
  FastLED.clear();
  FastLED.show();
}

void loop() {
  // Prioritize RF and clap detection
  handleRF();
  handleClaps();
  
  // Update animations only if powered on and not transitioning
  if (powerState && !isTransitioning) {
    updateAnimation();
  }
}

void handleRF() {
  if (mySwitch.available()) {
    Serial.print("Received code: ");
    long code = mySwitch.getReceivedValue();
    mySwitch.resetAvailable();
    
    
    if (code == 0) {
      Serial.println("Signal could not be read.");
      return;
    }
    
    Serial.print("Received code: ");
    Serial.println(code);
    
    switch (code) {
      case POWER:
        togglePower();
        break;
      case MODE_PLUS:
        changeMode(1);
        if (!powerState) { // Turn on if off
          powerState = true;
          Serial.println("Power turned on by MODE_PLUS");
        }
        break;
      case MODE_MINUS:
        changeMode(-1);
        if (!powerState) { // Turn on if off
          powerState = true;
          Serial.println("Power turned on by MODE_MINUS");
        }
        break;
      case SPEED_PLUS:
        effectSpeed = min(100, effectSpeed + 10);
        Serial.print("Speed increased: ");
        Serial.println(effectSpeed);
        break;
      case SPEED_MINUS:
        effectSpeed = max(0, effectSpeed - 10);
        Serial.print("Speed decreased: ");
        Serial.println(effectSpeed);
        break;
      case CONTINENT_PLUS:
        currentMode = CONTINENT;
        currentContinent = (currentContinent + 1) % 7;
        Serial.print("Continent mode: ");
        Serial.println(currentContinent);
        if (!powerState) { // Turn on if off
          powerState = true;
          Serial.println("Power turned on by CONTINENT_PLUS");
        }
        break;
      case CONTINENT_MINUS:
        currentMode = CONTINENT;
        currentContinent = (currentContinent - 1 + 7) % 7;
        Serial.print("Continent mode: ");
        Serial.println(currentContinent);
        if (!powerState) { // Turn on if off
          powerState = true;
          Serial.println("Power turned on by CONTINENT_MINUS");
        }
        break;
      case AUTO:
        currentMode = FULL_LED;
        currentEffect = 0;
        Serial.println("Auto mode activated");
        if (!powerState) { // Turn on if off
          powerState = true;
          Serial.println("Power turned on by AUTO");
        }
        
        break;
      case WHITE: case RED: case GREEN: case BLUE: case YELLOW: case LIGHT_BLUE: case PINK:
        setColorEffect(code);
        if (!powerState) { // Turn on if off
          powerState = true;
          Serial.println("Power turned on by color effect");
        }
      
        break;
      default:
        Serial.println("Unknown RF code");
        break;
    }
    saveState();
  }
}

void handleClaps() {
  if (digitalRead(MIC_PIN) == HIGH && millis() - lastClapTime >= CLAP_DEBOUNCE) {
    if (clapWindowStart == 0) {
      clapWindowStart = millis();
    }
    clapCount++;
    lastClapTime = millis();
    Serial.print("Clap detected: ");
    Serial.println(clapCount);
  }
  
  if (clapWindowStart > 0 && millis() - clapWindowStart > CLAP_WINDOW) {
    processClaps();
    clapCount = 0;
    clapWindowStart = 0;
  }
}

void processClaps() {
  Serial.print("Processing claps: ");
  Serial.println(clapCount);
  if (clapCount == 1) {
    togglePower();
  } else if (clapCount == 2) {
    if (currentMode == CONTINENT) {
      currentContinent = (currentContinent + 1) % 7;
      Serial.print("Next continent: ");
      Serial.println(currentContinent);
    } else {
      changeMode(1);
    }
    if (!powerState) { // Turn on if off
      powerState = true;
      Serial.println("Power turned on by 2 claps");
    }
    
  } else if (clapCount == 3) {
    if (currentMode == CONTINENT) {
      currentContinent = (currentContinent - 1 + 7) % 7;
      Serial.print("Previous continent: ");
      Serial.println(currentContinent);
    } else {
      changeMode(-1);
    }
    if (!powerState) { // Turn on if off
      powerState = true;
      Serial.println("Power turned on by 3 claps");
    }
    
  }
  saveState();
}

void togglePower() {
  powerState = !powerState;
  firePos = 0; // Reset fire animation
  Serial.print("Power state: ");
  Serial.println(powerState ? "ON" : "OFF");
  
  if (!powerState) {
    // Power off: Flash animation
    for (int i = 0; i < 5; i++) {
      setLeds(CHSV(random8(), 255, 255));
      FastLED.show();
      delay(50);
      setLeds(CRGB::Black);
      FastLED.show();
      delay(50);
    }
  } else {
    uint8_t trailLength = 10;   // Quyruq uzunligi
uint8_t starHue = 160; 
  for (int head = 0; head < NUM_LEDS + trailLength; head++) {
    // Barcha LEDlarni biroz qoraytirish (iz qoldirish)
    fadeToBlackBy(leds, NUM_LEDS, 50);

    // Shooting Star boshi
    if (head < NUM_LEDS) {
      leds[head] = CHSV(starHue, 255, 255);
    }

    // Quyruq (iz)
    for (int i = 1; i <= trailLength; i++) {
      int pos = head - i;
      if (pos >= 0 && pos < NUM_LEDS) {
        leds[pos] += CHSV(starHue, 255, 255 - (255 / trailLength) * i);
      }
    }

    FastLED.show();
    delay(30);
  }
  }
  saveState();
}

void changeMode(int direction) {
  currentMode = FULL_LED;
  firePos = 0; // Reset fire animation
  if (currentEffect == 0) {
    currentEffect = lastEffect;
  }
  currentEffect = (currentEffect + direction + 24) % 24;
  if (currentEffect == 0) {
    lastEffect = (lastEffect == 0) ? 1 : lastEffect;
  } else {
    lastEffect = currentEffect;
  }
  Serial.print("Mode changed to: ");
  Serial.println(currentEffect);
}

void startTransition() {
  isTransitioning = true;
  firePos = 0; // Reset fire animation
  pos = 0; // Reset position for other animations
  hue = 0; // Reset hue for animations
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
  FastLED.show();
  isTransitioning = false;
}

void setColorEffect(long code) {
  currentMode = FULL_LED;
  firePos = 0; // Reset fire animation
  switch (code) {
    case WHITE: currentEffect = 1; break;
    case RED: currentEffect = 2; break;
    case GREEN: currentEffect = 3; break;
    case YELLOW: currentEffect = 4; break;
    case LIGHT_BLUE: currentEffect = 5; break;
    case PINK: currentEffect = 6; break;
  }
  lastEffect = currentEffect;
  Serial.print("Color effect set: ");
  Serial.println(currentEffect);
}

void saveState() {
  EEPROM.update(0, currentEffect); // Save only currentEffect
}

void updateAnimation() {
  if (millis() - lastAnimationUpdate < ANIMATION_INTERVAL * (100 - effectSpeed) / 100) return;
  lastAnimationUpdate = millis();
  
  if (currentEffect == 0 && millis() - autoTimer > 10000) {
    currentEffect = (lastEffect % 23) + 1;
    lastEffect = currentEffect;
    firePos = 0; // Reset fire animation
    pos = 0; // Reset position
    autoTimer = millis();
    Serial.print("Auto mode switched to effect: ");
    Serial.println(currentEffect);
    saveState(); // Save new effect in Auto mode
  }
  
  FastLED.clear();
  
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
    case 8: fireEffect(); break;
    case 9: colorWipe(); break;
    case 10: rainbow(); break;
    case 11: theaterChase(); break;
    case 12: runningLights(); break;
    case 13: fadeInOut(); break;
    case 14: twinkle(); break;
    case 15: fireEffect(); break; // Same as mode 8
    case 16: pulse(); break;
    case 17: strobe(); break;
    case 18: colorCycle(); break;
    case 19: sparkle(); break;
    case 20: breathe(); break;
    case 21: meteorRain(); break;
    case 22: cylon(); break;
    case 23: confetti(); break;
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

void fireEffect() {
  int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
  int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
  
  // Clear LEDs
  setLeds(CRGB::Black);
  
  // Single red-hot pixel moving from start to end
  if (firePos <= end - start) {
    leds[start + firePos] = CRGB(255, 50, 0); // Red-hot color
  }
  
  firePos = (firePos + 1) % (end - start + 1); // Loop back to start for continuous animation
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