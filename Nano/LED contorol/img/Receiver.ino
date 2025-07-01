/* 
PIN  DESCRIPTION      ARDUINO PIN
1    GND              GND
2    RX DATA          D2
3    RX DATA          N/A
4    VCC (5V)         VCC
*/

#include <VirtualWire.h>

const int RX_DIO_Pin = 2;
int received;

void setup() {
    Serial.begin(9600);
    initialize_receiver();
}

/* Main program */
void loop() {
  received = receive_integer();
  if(received != -1) Serial.println(received);
}


/* DO NOT EDIT BELOW */

void initialize_receiver() {
  /* Initialises the DIO pin used to receive data from the Rx module */
  vw_set_rx_pin(RX_DIO_Pin);  
  /* Receive at 2000 bits per second */
  vw_setup(2000);
  /* Enable the receiver */
  vw_rx_start();  
}

int receive_integer() {
 /* Set the receive buffer size to 2 bytes */
  uint8_t Buffer_Size = 2;
  
  /* Holds the recived data */
  unsigned int Data;
  
  /* The receive buffer */
  uint8_t RxBuffer[Buffer_Size];

  /* Has a message been received? */
  if (vw_get_message(RxBuffer, &Buffer_Size)) // Non-blocking
  {

      /* Store the received high and low byte data */
      Data = RxBuffer[0] << 8 | RxBuffer[1];
 
      return Data;
  }
  return -1;
}
