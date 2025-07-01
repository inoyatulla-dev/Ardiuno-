#include <FastLED.h>
#include <RCSwitch.h>
#include <EEPROM.h>

// LED sozlamalari
#define NUM_LEDS 52 // 52 ta LED
#define DATA_PIN 6 // LEDlar uchun pin
CRGB leds[NUM_LEDS];

// RF masofaviy boshqaruv kodlari
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
#define BUTTON_100 429100 // Yangi 100 tugmasi

// Qit’a LED diapazonlari
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
#define ANTARCTICA_END 51

// Pinlar
#define MIC_PIN 7 // Mikrofon pin
#define RF_PIN 2 // RF moduli pin

// Holat boshqaruvi
enum Mode { FULL_LED, CONTINENT };
Mode currentMode = FULL_LED;
int currentContinent = 0;
int currentEffect = 0;
int lastEffect = 1;
bool powerState = false;
uint8_t effectSpeed = 50;
unsigned long lastAnimationUpdate = 0;
unsigned long autoTimer = 0;
bool isTransitioning = false;
int firePos = 0;
uint8_t hue = 0;
uint8_t pos = 0;

// Clap aniqlash
unsigned long lastClapTime = 0;
int clapCount = 0;
unsigned long clapWindowStart = 0;
const unsigned long CLAP_WINDOW = 3000;
const unsigned long CLAP_DEBOUNCE = 200;

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
  mySwitch.enableReceive(0);
  Serial.println("433 MHz RF signal qabul qiluvchisi tayyor...");
  
  currentEffect = EEPROM.read(0);
  if (currentEffect > 20) {
    currentEffect = 0;
  }
  lastEffect = (currentEffect == 0) ? 1 : currentEffect;
  
  FastLED.clear();
  FastLED.show();
}

void loop() {
  handleRF();
  handleClaps();
  
  if (powerState && !isTransitioning) {
    updateAnimation();
  }
  
  delayMicroseconds(100);
}

void handleRF() {
  if (mySwitch.available()) {
    long code = mySwitch.getReceivedValue();
    if (code == 0) {
      Serial.println("Signal o‘qilmadi.");
      mySwitch.resetAvailable();
      mySwitch.enableReceive(0);
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
        if (!powerState) {
          powerState = true;
          Serial.println("MODE_PLUS orqali yoqildi");
        }
        break;
      case MODE_MINUS:
        changeMode(-1);
        if (!powerState) {
          powerState = true;
          Serial.println("MODE_MINUS orqali yoqildi");
        }
        break;
      case SPEED_PLUS:
        effectSpeed = min(100, effectSpeed + 10);
        Serial.print("Tezlik oshirildi: ");
        Serial.println(effectSpeed);
        break;
      case SPEED_MINUS:
        effectSpeed = max(0, effectSpeed - 10);
        Serial.print("Tezlik kamaytirildi: ");
        Serial.println(effectSpeed);
        break;
      case CONTINENT_PLUS:
        if (powerState) {
          currentMode = CONTINENT;
          currentContinent = (currentContinent + 1) % 7;
          Serial.print("Qit’a rejimi: ");
          Serial.println(currentContinent);
        }
        break;
      case CONTINENT_MINUS:
        if (powerState) {
          currentMode = CONTINENT;
          currentContinent = (currentContinent - 1 + 7) % 7;
          Serial.print("Qit’a rejimi: ");
          Serial.println(currentContinent);
        }
        break;
      case AUTO:
        currentMode = FULL_LED;
        currentEffect = 0;
        Serial.println("Avto rejim yoqildi");
        break;
      case WHITE: case RED: case GREEN: case BLUE: case YELLOW: case LIGHT_BLUE: case PINK:
        if (powerState) {
          setColorEffect(code);
        }
        break;
      case BUTTON_100:
        if (powerState) {
          currentEffect = 8; // Piliblab yonib-o‘chadigan effekt
          lastEffect = currentEffect;
          Serial.println("BUTTON_100: Piliblab effekt yoqildi");
        }
        break;
      default:
        Serial.println("Noma’lum RF kodi");
        break;
    }
    saveState();
    mySwitch.resetAvailable();
  }
}

void handleClaps() {
  if (digitalRead(MIC_PIN) == HIGH && millis() - lastClapTime >= CLAP_DEBOUNCE) {
    if (clapWindowStart == 0) {
      clapWindowStart = millis();
    }
    clapCount++;
    lastClapTime = millis();
    Serial.print("Clap aniqlandi: ");
    Serial.println(clapCount);
  }
  
  if (clapWindowStart > 0 && millis() - clapWindowStart > CLAP_WINDOW) {
    processClaps();
    clapCount = 0;
    clapWindowStart = 0;
  }
}

void processClaps() {
  Serial.print("Claplar qayta ishlanmoqda: ");
  Serial.println(clapCount);
  if (clapCount == 1) {
    togglePower();
  } else if (clapCount == 2) {
    if (currentMode == CONTINENT) {
      currentContinent = (currentContinent + 1) % 7;
      Serial.print("Keyingi qit’a: ");
      Serial.println(currentContinent);
    } else {
      changeMode(1);
    }
    if (!powerState) {
      powerState = true;
      Serial.println("2 clap orqali yoqildi");
    }
    startTransition();
  } else if (clapCount == 3) {
    if (currentMode == CONTINENT) {
      currentContinent = (currentContinent - 1 + 7) % 7;
      Serial.print("Oldingi qit’a: ");
      Serial.println(currentContinent);
    } else {
      changeMode(-1);
    }
    if (!powerState) {
      powerState = true;
      Serial.println("3 clap orqali yoqildi");
    }
    startTransition();
  }
  saveState();
}

void togglePower() {
  powerState = !powerState;
  if (!powerState) {
    currentMode = FULL_LED;
    firePos = 0;
    pos = 0;
    hue = 0;
    Serial.println("Quvvat o‘chirildi");
    FastLED.clear();
    FastLED.show();
  } else {
    Serial.println("Quvvat yoqildi");
    for (int i = 0; i < 5; i++) {
      setLeds(CHSV(random8(), 255, 255));
      FastLED.show();
      delay(50);
      FastLED.clear();
      FastLED.show();
      delay(50);
    }
  }
  saveState();
}

void changeMode(int direction) {
  currentMode = FULL_LED;
  firePos = 0;
  pos = 0;
  hue = 0;
  if (currentEffect == 0) {
    currentEffect = lastEffect;
  }
  currentEffect = (currentEffect + direction + 21) % 21;
  if (currentEffect == 0) {
    lastEffect = (lastEffect == 0) ? 1 : lastEffect;
  } else {
    lastEffect = currentEffect;
  }
  Serial.print("Rejim o‘zgartirildi: ");
  Serial.println(currentEffect);
}

void startTransition() {
  isTransitioning = true;
  firePos = 0;
  pos = 0;
  hue = 0;
  FastLED.clear();
  FastLED.show();
  isTransitioning = false;
}

void setColorEffect(long code) {
  currentMode = FULL_LED;
  firePos = 0;
  pos = 0;
  hue = 0;
  switch (code) {
    case WHITE: currentEffect = 1; break;
    case RED: currentEffect = 2; break;
    case GREEN: currentEffect = 3; break;
    case BLUE: currentEffect = 4; break;
    case YELLOW: currentEffect = 5; break;
    case LIGHT_BLUE: currentEffect = 6; break;
    case PINK: currentEffect = 7; break;
  }
  lastEffect = currentEffect;
  Serial.print("Rang effekti o‘rnatildi: ");
  Serial.println(currentEffect);
}

void saveState() {
  EEPROM.update(0, currentEffect);
}

void updateAnimation() {
  int animationInterval = map(effectSpeed, 0, 100, 500, 120); // 120-500ms diapazon
  if (millis() - lastAnimationUpdate < animationInterval) return;
  lastAnimationUpdate = millis();
  
  interrupts();
  
  if (currentEffect == 0 && millis() - autoTimer > 10000) {
    currentEffect = (lastEffect % 20) + 1;
    lastEffect = currentEffect;
    firePos = 0;
    pos = 0;
    hue = 0;
    autoTimer = millis();
    Serial.print("Avto rejim effekti o‘zgardi: ");
    Serial.println(currentEffect);
    saveState();
  }
  
  FastLED.clear();
  
  switch (currentEffect) {
    case 0: break;
    case 1: setLeds(CRGB::White); break;
    case 2: setLeds(CRGB::Red); break;
    case 3: setLeds(CRGB::Green); break;
    case 4: setLeds(CRGB::Blue); break;
    case 5: setLeds(CRGB::Yellow); break;
    case 6: setLeds(CRGB(135, 206, 250)); break;
    case 7: setLeds(CRGB::Pink); break;
    case 8: softBlink(); break;
    case 9: oceanWave(); break;
    case 10: smoothRainbow(); break;
    case 11: gentlePulse(); break;
    case 12: twinklingStars(); break;
    case 13: colorFade(); break;
    case 14: calmBreathe(); break;
    case 15: meteorShower(); break;
    case 16: soothingWipe(); break;
    case 17: softSparkle(); break;
    case 18: gradientFlow(); break;
    case 19: slowCylon(); break;
    case 20: confettiGlow(); break;
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
  setLeds(CRGB(color));
}

void softBlink() {
  uint8_t brightness = beatsin8(5, 64, 255);
  setLeds(CHSV(hue, 200, brightness));
  hue += 2;
}

void oceanWave() {
  uint8_t brightness = beatsin8(3, 64, 255);
  setLeds(CHSV(160 + (beatsin8(2, 0, 20)), 200, brightness));
}

void smoothRainbow() {
  setLeds(CHSV(hue++, 200, 255));
}

void gentlePulse() {
  uint8_t brightness = beatsin8(4, 100, 255);
  setLeds(CHSV(180 + (beatsin8(3, 0, 20)), 200, brightness));
}

void twinklingStars() {
  setLeds(CRGB::Black);
  int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
  int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
  leds[random(start, end + 1)] = CRGB(CHSV(160 + random8(20), 200, 255));
}

void colorFade() {
  uint8_t h = beatsin8(2, 0, 255);
  setLeds(CHSV(h, 200, 255));
}

void calmBreathe() {
  uint8_t brightness = beatsin8(2, 64, 255);
  setLeds(CHSV(180 + (beatsin8(1, 0, 20)), 200, brightness));
}

void meteorShower() {
  static int pos = 0;
  setLeds(CRGB::Black);
  int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
  int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
  for (int i = 0; i < 8; i++) {
    if (pos - i >= start && pos - i <= end) {
      leds[pos - i] = CRGB(CHSV(160 + random8(20), 200, 255 - (i * 30)));
    }
  }
  pos = (pos + 1) % (end - start + 1);
}

void soothingWipe() {
  static int pos = 0;
  setLeds(CRGB::Black);
  int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
  int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
  leds[(start + pos) % (end + 1)] = CRGB(CHSV(180 + (beatsin8(2, 0, 20)), 200, 255));
  pos = (pos + 1) % (end - start + 1);
}

void softSparkle() {
  setLeds(CRGB::Black);
  int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
  int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
  leds[random(start, end + 1)] = CRGB(CHSV(160 + random8(20), 200, 255));
}

void gradientFlow() {
  setLeds(CHSV(180 + (beatsin8(2, 0, 40)), 200, 255));
}

void slowCylon() {
  static int pos = 0;
  static bool forward = true;
  setLeds(CRGB::Black);
  int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
  int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
  leds[start + pos] = CRGB(CHSV(180 + (beatsin8(2, 0, 20)), 200, 255));
  if (forward) {
    pos++;
    if (pos >= end - start) forward = false;
  } else {
    pos--;
    if (pos <= 0) forward = true;
  }
}

void confettiGlow() {
  setLeds(CRGB::Black);
  int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
  int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
  leds[random(start, end + 1)] = CRGB(CHSV(160 + random8(40), 200, 255));
}