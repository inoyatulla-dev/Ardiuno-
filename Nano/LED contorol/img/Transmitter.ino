    /*
    PIN  DESCRIPTION      ARDUINO PIN
    1    GND              GND
    2    VCC (3.5-12V)    VCC
    3    TX DATA          D2
    */
    
    #include <VirtualWire.h>
    const int TX_DIO_Pin = 2;
    
    void setup() {
      pinMode(13, OUTPUT);
      initialize_transmitter();
    }
    
    /* Main program */
    void loop() {
      int counter;
      for(counter=0; counter<100; counter++) {
        digitalWrite(13, HIGH);  
        transmit_integer(counter);
        digitalWrite(13, LOW); 
        delay(200);
      }
    }
    
    
    /* DO NO EDIT BELOW */
    
    void initialize_transmitter() {
      /* Initialises the DIO pin used to send data to the Tx module */
      vw_set_tx_pin(TX_DIO_Pin);
      /* Set the transmit logic level (LOW = transmit for this version of module)*/ 
      vw_set_ptt_inverted(true); 
      
      /* Transmit at 2000 bits per second */
      vw_setup(2000);    // Bits per sec
    }
    
    void transmit_integer(unsigned int Data) {
      /* The transmit buffer that will hold the data to be 
         transmitted. */
      byte TxBuffer[2];
      /* ...and store it as high and low bytes in the transmit 
         buffer */
      TxBuffer[0] = Data >> 8;
      TxBuffer[1] = Data;
      
      /* Send the data (2 bytes) */
      vw_send((byte *)TxBuffer, 2);
      /* Wait until the data has been sent */
      vw_wait_tx();
    }
