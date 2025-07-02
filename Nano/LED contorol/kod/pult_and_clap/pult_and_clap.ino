#include <FastLED.h>
#include <RCSwitch.h>
#include <EEPROM.h>

// LED sozlamalari
#define NUM_LEDS 120 // 52 ta LED
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
#define BLUE 429074
#define YELLOW 429075
#define LIGHT_BLUE 429076
#define PINK 429077
#define AUTO 429064

// Qit’a LED diapazonlari
#define ASIA_START 0
#define ASIA_END 17
#define AFRICA_START 18
#define AFRICA_END 36
#define EUROPE_START 37
#define EUROPE_END 60
#define NORTH_AMERICA_START 61
#define NORTH_AMERICA_END 70
#define SOUTH_AMERICA_START 71
#define SOUTH_AMERICA_END 78
#define AUSTRALIA_START 79
#define AUSTRALIA_END 85
#define ANTARCTICA_START 86
#define ANTARCTICA_END 120

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
uint8_t effectSpeed = 100;
unsigned long lastAnimationUpdate = 0;
unsigned long autoTimer = 0;
bool isTransitioning = false;
int firePos = 0;
uint8_t hue = 0;
uint8_t pos = 0;
unsigned long lastButtonTime = 0; // Tugma debouncing uchun
const unsigned long BUTTON_DEBOUNCE = 500; // 1 soniya debouncing

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
  if (currentEffect > 23) {
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
    
    if (millis() - lastButtonTime < BUTTON_DEBOUNCE) {
      Serial.println("Tugma ikki marta bosilishi oldi olindi");
      mySwitch.resetAvailable();
      return;
    }
    
    lastButtonTime = millis();
    
    Serial.print("Qabul qilingan kod: ");
    Serial.println(code);
    
    switch (code) {
      case POWER:
        togglePower();
        break;
      case MODE_PLUS:
        changeMode(1);
        if (powerState) {
            changeMode(1);
          powerState = true;
          Serial.println("MODE_PLUS orqali yoqildi");
        }
        break;
      case MODE_MINUS:
        
        if (powerState) {
            changeMode(-1);
          powerState = true;
          Serial.println("MODE_MINUS orqali yoqildi");
        }
        break;
      case SPEED_PLUS:
        effectSpeed = min(100, effectSpeed+ 10);
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
    for (int i = 255; i >= 0; i -= 10) {
    for (int j = 0; j < NUM_LEDS; j++) {
      leds[j].fadeToBlackBy(10); // har bir bosqichda so‘nadi
    }
    FastLED.show();
    delay(30); // qisqa kutish (xohlasangiz millis() bilan ham qilamiz)
  }
    firePos = 0;
    pos = 0;
    hue = 0;
    Serial.println("Quvvat o‘chirildi");
    FastLED.clear();
    FastLED.show();
  } else {
    Serial.println("Quvvat yoqildi");
     for(int dot = 0; dot < NUM_LEDS; dot++) { 
            leds[dot] = CRGB::Blue;
            FastLED.show();
            // clear this led for the next time around the loop
            leds[dot] = CRGB::Black;
            delay(30);
    
  }
  }
  saveState();
}

void changeMode(int direction) {
 // currentMode = FULL_LED;
  firePos = 0;
  pos = 0;
  hue = 0;
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
    case RED: currentEffect = 3; break;
    case GREEN: currentEffect = 2; break;
    case BLUE: currentEffect = 4; break;
    case YELLOW: currentEffect = 5; break;
    case LIGHT_BLUE: currentEffect = 7; break;
    case PINK: currentEffect = 6; break;
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
    currentEffect = (lastEffect % 23) + 1;
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
    case 1: setLeds(CHSV(0, 0, 255)); break;
    case 2: setLeds(CHSV(0, 255, 255)); break;
    case 3: setLeds(CHSV(96, 255, 255)); break;
    case 4: setLeds(CHSV(160, 255, 255)); break;
    case 5: setLeds(CHSV(16, 255, 255)); break;
    case 6: setLeds(CHSV(128, 200, 255)); break;
    case 7: setLeds(CHSV(192, 255, 255)); break;
    case 8: mode_8(); break;
    case 9: mode_9(); break;
    case 10: mode_10(); break;
    case 11: mode_11(); break;
    case 12: mode_12(); break;
    case 13: mode_13(); break;
    case 14: mode_14(); break;
    case 15: mode_15(); break;
    case 16: mode_16(); break;
    case 17: mode_17(); break;
    case 18: mode_18(); break;
    case 19: mode_19(); break;
    case 20: mode_20(); break;
    case 21: mode_21(); break;
    case 22: mode_22(); break;
    case 23: mode_23(); break;
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

// void mode_8() {
//   static unsigned long previousMillis = 0;
//   static int twinkleIndex = 0;
//   if (millis() - previousMillis > 80) {
//     previousMillis = millis();
//     leds[random(NUM_LEDS)] = CHSV(random8(), 255, 255);
//     fadeToBlackBy(leds, NUM_LEDS, 40);
//   }
// }
void mode_8() {
  static unsigned long previousMillis = 0;
  const int DelayDuration = 50;
  const int Color = 0;
  const int ColorSaturation = 255;
  const int PixelVolume = 30;
  const int FadeAmount = 30;

  if (millis() - previousMillis > DelayDuration) {
    previousMillis = millis();

    for (int i = 0; i < NUM_LEDS; i++) {
      if (random(PixelVolume) < 2) {
        uint8_t intensity = random(100, 255);
        CHSV colorHSV = CHSV(Color, ColorSaturation, intensity);
        leds[i] = CRGB(colorHSV);
      }

      if (leds[i].r > 0 || leds[i].g > 0 || leds[i].b > 0) {
        leds[i].fadeToBlackBy(FadeAmount);
      }
    }
  }
}
void mode_9() {
  static byte heat[NUM_LEDS];
  static unsigned long previousMillis = 0;
  const int DelayDuration = 40;
  const int Cooling = 55;
  const int Sparks = 120;
  const int Color = 1; // 0-7 rang tanlash
  const bool ReverseDirection = false;

  if (millis() - previousMillis > DelayDuration) {
    previousMillis = millis();

    int cooldown;
    for (int i = 0; i < NUM_LEDS; i++) {
      cooldown = random(0, ((Cooling * 10) / NUM_LEDS) + 2);
      heat[i] = (cooldown > heat[i]) ? 0 : heat[i] - cooldown;
    }

    for (int k = NUM_LEDS - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }

    if (random(255) < Sparks) {
      int y = random(7);
      heat[y] = heat[y] + random(160, 255);
    }

    for (int n = 0; n < NUM_LEDS; n++) {
      byte temperature = heat[n];
      byte t192 = (temperature * 191) / 255;
      byte heatramp = (t192 & 0x3F) << 2;

      int Pixel = ReverseDirection ? (NUM_LEDS - 1 - n) : n;

      CRGB c;
      if (t192 > 0x80) {
        c.setRGB(heatramp, heatramp, heatramp);
      } else if (t192 > 0x40) {
        c.setRGB(255, heatramp, 0);
      } else {
        c.setRGB(heatramp, 0, 0);
      }

      leds[Pixel] = c;
    }
  }
}


// void mode_9() {
//   static unsigned long previousMillis = 0;
//   if (millis() - previousMillis > 50) {
//     previousMillis = millis();
//     for (int i = 0; i < NUM_LEDS; i++) {
//       byte flicker = random8(192, 255);
//       leds[i] = CRGB(flicker, flicker / 3, 0);
//     }
//   }
// }
void mode_10() {
  static uint8_t hue = 0;
  fill_solid(leds, NUM_LEDS, CHSV(hue++, 255, 255));
}
void mode_11() {
  static int pos = 0;
  static unsigned long previousMillis = 0;
  if (millis() - previousMillis > 30) {
    previousMillis = millis();
    fadeToBlackBy(leds, NUM_LEDS, 64);
    leds[pos] = CRGB::White;
    pos++;
    if (pos >= NUM_LEDS) pos = 0;
  }
}
void mode_12() {
  static uint8_t brightness = 0;
  static int8_t direction = 5;
  fill_solid(leds, NUM_LEDS, CHSV(0, 255, brightness));
  brightness += direction;
  if (brightness == 0 || brightness == 255) direction = -direction;
}

void mode_13() {
  static byte heat[NUM_LEDS];
  static unsigned long previousMillis = 0;
  if (millis() - previousMillis > 40) {
    previousMillis = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      heat[i] = qsub8(heat[i], random8(0, ((55 * 10) / NUM_LEDS) + 2));
    }

    for (int k = NUM_LEDS - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    }

    if (random8() < 120) {
      int y = random8(7);
      heat[y] = qadd8(heat[y], random8(160, 255));
    }

    for (int j = 0; j < NUM_LEDS; j++) {
      leds[j] = HeatColor(heat[j]);
    }
  }
}
void mode_14() {
  static uint8_t hue = 0;
  static unsigned long previousMillis = 0;
  EVERY_N_MILLISECONDS(20) { hue++; }
  if (millis() - previousMillis > 50) {
    previousMillis = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
      if (random8() < 30)
        leds[i] = CHSV(hue, 255, 255);
      else
        leds[i].fadeToBlackBy(20);
    }
  }
}
void mode_15() {
  static uint8_t startIndex = 0;
  startIndex++;
  fill_rainbow(leds, NUM_LEDS, startIndex, 7);
}
void mode_16() {
  static int pos = 0;
  static int dir = 1;
  static unsigned long previousMillis = 0;
  if (millis() - previousMillis > 20) {
    previousMillis = millis();
    fadeToBlackBy(leds, NUM_LEDS, 80);
    leds[pos] = CRGB::Red;
    pos += dir;
    if (pos == NUM_LEDS - 1 || pos == 0) dir = -dir;
  }
}
void mode_17() {
  fadeToBlackBy(leds, NUM_LEDS, 20);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV(random8(), 200, 255);
}
void mode_18() {
  static uint8_t baseHue = 100;
  fill_solid(leds, NUM_LEDS, CHSV(baseHue, 255, 150));
  if (random8() < 80) {
    leds[random16(NUM_LEDS)] += CRGB::White;
  }
}
void mode_19() {
  fadeToBlackBy(leds, NUM_LEDS, 20);
  int pos = beatsin16(13, 0, NUM_LEDS - 1);
  leds[pos] += CHSV(200, 255, 192);
}
void mode_20() {
  fadeToBlackBy(leds, NUM_LEDS, 20);
  byte dothue = 0;
  for (int i = 0; i < 8; i++) {
    leds[beatsin16(i + 7, 0, NUM_LEDS - 1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}
void mode_21() {
  static int pos = 0;
  static int dir = 1;
  fadeToBlackBy(leds, NUM_LEDS, 30);
  leds[pos] = CHSV(160, 255, 255);
  pos += dir;
  if (pos >= NUM_LEDS - 1 || pos <= 0) dir = -dir;
}
void mode_22() {
  static int pos = 0;
  static int dir = 1;
  fadeToBlackBy(leds, NUM_LEDS, 40);
  leds[pos] = CRGB::Blue;
  pos += dir;
  if (pos <= 0 || pos >= NUM_LEDS - 1) dir = -dir;
}
void mode_23() {
  fadeToBlackBy(leds, NUM_LEDS, 40);
  if (random8() < 50) {
    leds[random16(NUM_LEDS)] += CHSV(random8(), 200, 255);
  }
}

// void softBlink() {
//   uint8_t brightness = beatsin8(5, 64, 255);
//   setLeds(CHSV(hue, 200, brightness));
//   hue += 2;
// }

// void oceanWave() {
//   uint8_t brightness = beatsin8(3, 64, 255);
//   setLeds(CHSV(160 + (beatsin8(2, 0, 20)), 200, brightness));
// }

// void smoothRainbow() {
//   setLeds(CHSV(hue++, 200, 255));
// }

// void gentlePulse() {
//   uint8_t brightness = beatsin8(4, 100, 255);
//   setLeds(CHSV(180 + (beatsin8(3, 0, 20)), 200, brightness));
// }

// void twinklingStars() {
//   setLeds(CRGB::Black);
//   int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
//   int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
//   leds[random(start, end + 1)] = CRGB(CHSV(160 + random8(20), 200, 255));
// }

// void colorFade() {
//   uint8_t h = beatsin8(2, 0, 255);
//   setLeds(CHSV(h, 200, 255));
// }

// void calmBreathe() {
//   uint8_t brightness = beatsin8(2, 64, 255);
//   setLeds(CHSV(180 + (beatsin8(1, 0, 20)), 200, brightness));
// }

// void meteorShower() {
//   static int pos = 0;
//   setLeds(CRGB::Black);
//   int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
//   int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
//   for (int i = 0; i < 8; i++) {
//     if (pos - i >= start && pos - i <= end) {
//       leds[pos - i] = CRGB(CHSV(160 + random8(20), 200, 255 - (i * 30)));
//     }
//   }
//   pos = (pos + 1) % (end - start + 1);
// }

// void soothingWipe() {
//   static int pos = 0;
//   setLeds(CRGB::Black);
//   int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
//   int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
//   leds[(start + pos) % (end + 1)] = CRGB(CHSV(180 + (beatsin8(2, 0, 20)), 200, 255));
//   pos = (pos + 1) % (end - start + 1);
// }

// void softSparkle() {
//   setLeds(CRGB::Black);
//   int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
//   int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
//   leds[random(start, end + 1)] = CRGB(CHSV(160 + random8(20), 200, 255));
// }

// void gradientFlow() {
//   setLeds(CHSV(180 + (beatsin8(2, 0, 40)), 200, 255));
// }

// void slowCylon() {
//   static int pos = 0;
//   static bool forward = true;
//   setLeds(CRGB::Black);
//   int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
//   int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
//   leds[start + pos] = CRGB(CHSV(180 + (beatsin8(2, 0, 20)), 200, 255));
//   if (forward) {
//     pos++;
//     if (pos >= end - start) forward = false;
//   } else {
//     pos--;
//     if (pos <= 0) forward = true;
//   }
// }

// void confettiGlow() {
//   setLeds(CRGB::Black);
//   int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
//   int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
//   leds[random(start, end + 1)] = CRGB(CHSV(160 + random8(40), 200, 255));
// }

// void gradientTransition() {
//   static uint8_t startHue = 0;
//   int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
//   int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
//   for (int i = start; i <= end; i++) {
//     leds[i] = CHSV(startHue + (i * 5), 200, 255);
//   }
//   startHue += 2;
// }

// void rainbowCycle() {
//   static uint8_t startHue = 0;
//   int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
//   int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
//   for (int i = start; i <= end; i++) {
//     leds[i] = CHSV(startHue + (i * 255 / (end - start + 1)), 200, 255);
//   }
//   startHue += 2;
// }

// void staticColors() {
//   int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
//   int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
//   for (int i = start; i <= end; i++) {
//     switch ((i - start) % 7) {
//       case 0: leds[i] = CRGB::White; break;
//       case 1: leds[i] = CRGB::Red; break;
//       case 2: leds[i] = CRGB::Green; break;
//       case 3: leds[i] = CRGB::Blue; break;
//       case 4: leds[i] = CRGB::Yellow; break;
//       case 5: leds[i] = CRGB(135, 206, 250); break;
//       case 6: leds[i] = CRGB::Pink; break;
//     }
//   }
// }

// void youtubeAnimation() {
//   static uint8_t startHue = 0;
//   int start = currentMode == CONTINENT ? continents[currentContinent].start : 0;
//   int end = currentMode == CONTINENT ? continents[currentContinent].end : NUM_LEDS - 1;
  
//   for (int i = start; i <= end; i++) {
//     uint8_t brightness = beatsin8(5, 64, 255);
//     leds[i] = CHSV(startHue + (i * 10), 200, brightness);
//   }
//   startHue += 3;
// }