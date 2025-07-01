#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();

void setup() {
  Serial.begin(9600);
  mySwitch.enableReceive(0);  // D2 pin Arduino'da - interrupt 0
  Serial.println("433 MHz RF signal receiver ready...");
}

void loop() {
  if (mySwitch.available()) {
    long receivedCode = mySwitch.getReceivedValue();

    if (receivedCode == 0) {
      Serial.println("Signalni o'qib bo'lmadi.");
    } else {
      Serial.print("Qabul qilingan kod✅: ");
      Serial.println(receivedCode);
    }

    mySwitch.resetAvailable();
  }
}
