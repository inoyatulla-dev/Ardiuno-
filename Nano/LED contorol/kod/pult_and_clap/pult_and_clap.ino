
#include <FastLED.h>
#include <RCSwitch.h>
#include <EEPROM.h>

// LED sozlamalari
#define NUM_LEDS 52 // 40 ta LED
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
#define ANTARCTICA_END 52 // NUM_LEDS - 1 ga moslashtirildi

// Pinlar
#define MIC_PIN 7 // Mikrofon pin
#define RF_PIN 2 // RF moduli pin

// Holat boshqaruvi
enum Mode { FULL_LED, CONTINENT };
Mode currentMode = FULL_LED; // Boshlang‘ich rejim: butun LEDlar
int currentContinent = 0; // 0=Osiyo, 1=Afrika, 2=Yevropa, 3=Shimoliy Amerika, 4=Janubiy Amerika, 5=Avstraliya, 6=Antarktida
int currentEffect = 0; // 0=Avto, 1-23 maxsus effektlar
int lastEffect = 1; // Avto bo‘lmagan oxirgi effekt
bool powerState = false; // O‘chirilgan holda boshlanadi
uint8_t effectSpeed = 50; // Animatsiya tezligi (0-100)
unsigned long lastAnimationUpdate = 0;
const int ANIMATION_INTERVAL = 30; // Animatsiya yangilanishi (ms), CPU yukini kamaytirish uchun 30ms
unsigned long autoTimer = 0;
bool isTransitioning = false;
int firePos = 0; // Yong‘in effekti pozitsiyasi
uint8_t hue = 0; // Animatsiyalar uchun rang
uint8_t pos = 0; // Animatsiyalar uchun pozitsiya

// Clap aniqlash
unsigned long lastClapTime = 0;
int clapCount = 0;
unsigned long clapWindowStart = 0;
const unsigned long CLAP_WINDOW = 3000; // 3 soniya clap oynasi
const unsigned long CLAP_DEBOUNCE = 200; // Claplar orasidagi kechikish (ms)

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
  FastLED.setBrightness(255); // LED yorqinligi
  
  pinMode(MIC_PIN, INPUT);
  Serial.begin(9600);
  mySwitch.enableReceive(0); // RF moduli D2 pinida (interrupt 0)
  Serial.println("433 MHz RF signal qabul qiluvchisi tayyor...");
  
  // EEPROM’dan oxirgi effektni yuklash
  currentEffect = EEPROM.read(0);
  if (currentEffect > 23) { // Agar noto‘g‘ri bo‘lsa, avto rejim
    currentEffect = 0;
  }
  lastEffect = (currentEffect == 0) ? 1 : currentEffect; // Avto bo‘lmasa, oxirgi effekt
  
  // Boshlang‘ichda LEDlar o‘chiriladi
  FastLED.clear();
  FastLED.show();
}

void loop() {
  // RF va clap aniqlash birinchi navbatda
  handleRF();
  handleClaps();
  
  // LEDlar yoniq bo‘lsa va o‘tish jarayoni bo‘lmasa, animatsiyani yangilash
  if (powerState && !isTransitioning) {
    updateAnimation();
  }
  
  // Interruptlar uchun vaqt berish
  delayMicroseconds(100);
}

void handleRF() {
  if (mySwitch.available()) {
    long code = mySwitch.getReceivedValue();
    if (code == 0) {
      Serial.println("Signal o‘qilmadi.");
      mySwitch.resetAvailable();
      mySwitch.enableReceive(0); // Qayta yoqish
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
        if (powerState) { // Agar o‘chiq bo‘lsa, yoqish
          powerState = true;
          Serial.println("MODE_PLUS orqali yoqildi");
        }
        break;
      case MODE_MINUS:
        changeMode(-1);
        if (powerState) {
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
        Serial.println("CONTINENT_PLUS orqali yoqildi");
        }
        break;
      case CONTINENT_MINUS:
      
        if (powerState) {
           currentMode = CONTINENT;
        currentContinent = (currentContinent - 1 + 7) % 7;
        Serial.print("Qit’a rejimi: ");
        Serial.println(currentContinent);
      
          Serial.println("CONTINENT_MINUS orqali yoqildi");
        }
        break;
      case AUTO:
        if (powerState) {
        currentMode = FULL_LED;
        currentEffect = 0;
        Serial.println("Avto rejim yoqildi");
        }

        break;
      case WHITE: case RED: case GREEN: case BLUE: case YELLOW: case LIGHT_BLUE: case PINK:
        
        if (powerState) {
          setColorEffect(code);
          Serial.println("Rang effekti orqali yoqildi");
        }

        break;
      default:
        Serial.println("Noma’lum RF kodi");
        break;
    }
    saveState();
    mySwitch.resetAvailable();
  } else {
    // Debug: LEDlar yoniq bo‘lsa va RF ma’lumotlari kelmasa
    static unsigned long lastDebug = 0;
    if (powerState && millis() - lastDebug >= 1000) {
      Serial.println("RF ma’lumotlari yo‘q (LEDlar yoniq)");
      lastDebug = millis();
    }
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
  firePos = 0; // Yong‘in effekti qayta boshlanadi
  Serial.print("Quvvat holati: ");
  Serial.println(powerState ? "YOQILDI" : "O‘CHIRILDI");
  
  if (!powerState) {
    // O‘chirish: 500ms chaqnash
    for (int i = 0; i < 5; i++) {
      setLeds(CHSV(random8(), 255, 255));
      FastLED.show();
      delay(50);
      setLeds(CRGB::Black);
      FastLED.show();
      delay(50);
    }
  } else {
    // Yoqish: 500ms chaqnash
    for (int i = 0; i < 5; i++) {
      setLeds(CHSV(random8(), 255, 255));
      FastLED.show();
      delay(50);
      setLeds(CRGB::Black);
      FastLED.show();
      delay(50);
    }
  }
  saveState();
}

void changeMode(int direction) {
  currentMode = FULL_LED;
  firePos = 0; // Yong‘in effekti qayta boshlanadi
  if (currentEffect == 0) {
    currentEffect = lastEffect;
  }
  currentEffect = (currentEffect + direction + 24) % 24;
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
  firePos = 0; // Yong‘in effekti qayta boshlanadi
  pos = 0; // Boshqa animatsiyalar uchun pozitsiya
  hue = 0; // Rang qayta boshlanadi
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
  FastLED.show();
  isTransitioning = false;
}

void setColorEffect(long code) {
  currentMode = FULL_LED;
  firePos = 0; // Yong‘in effekti qayta boshlanadi
  switch (code) {
    case WHITE: currentEffect = 1; break;
    case RED: currentEffect = 2; break;
    case GREEN: currentEffect = 3; break;
    case YELLOW: currentEffect = 4; break;
    case LIGHT_BLUE: currentEffect = 5; break;
    case PINK: currentEffect = 6; break;
  }
  lastEffect = currentEffect;
  Serial.print("Rang effekti o‘rnatildi: ");
  Serial.println(currentEffect);
}

void saveState() {
  EEPROM.update(0, currentEffect); // Faqat currentEffect saqlanadi
}

void updateAnimation() {
  if (millis() - lastAnimationUpdate < ANIMATION_INTERVAL * (100 - effectSpeed) / 100) return;
  lastAnimationUpdate = millis();
  
  // Interruptlarni yoqish
  interrupts();
  
  if (currentEffect == 0 && millis() - autoTimer > 10000) {
    currentEffect = (lastEffect % 23) + 1;
    lastEffect = currentEffect;
    firePos = 0; // Yong‘in effekti qayta boshlanadi
    pos = 0; // Pozitsiya qayta boshlanadi
    autoTimer = millis();
    Serial.print("Avto rejim effekti o‘zgardi: ");
    Serial.println(currentEffect);
    saveState(); // Yangi effektni saqlash
  }
  
  FastLED.clear();
  
  switch (currentEffect) {
    case 0: // Avto (yuqorida boshqariladi)
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
    case 15: fireEffect(); break; // 8 bilan bir xil
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
  setLeds(CRGB(color)); // CHSV dan CRGB ga o‘tkazish
}

void fireEffect() {
  int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
  int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
  
  // LEDlarni tozalash
  setLeds(CRGB::Black);
  
  // Bitta qizil nuqta harakatlanadi
  if (firePos <= end - start) {
    leds[start + firePos] = CRGB(255, 50, 0); // Qizil rang
  }
  
  firePos = (firePos + 1) % (end - start + 1); // Doimiy aylanish
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