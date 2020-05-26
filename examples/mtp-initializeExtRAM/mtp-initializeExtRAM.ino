/*
   This test uses the optional quad spi flash on Teensy 4.1
   https://github.com/pellepl/spiffs/wiki/Using-spiffs
   https://github.com/pellepl/spiffs/wiki/FAQ

   ATTENTION: Flash needs to be empty before first use of SPIFFS


   Frank B, 2020
*/
#include <spiffs_t4.h>
#include <spiffs.h>

spiffs_t4 eRAM;

void setup() {
  while (!Serial);
  delay(5000);
  Serial.println("\n" __FILE__ " " __DATE__ " " __TIME__);
#if 1
  Serial.println("\n Enter 'y' in 6 seconds to format FlashChip - other to skip");
  uint32_t pauseS = millis();
  char chIn = 9;
  while ( pauseS + 6000 > millis() && 9 == chIn ) {
    if ( Serial.available() ) {
      do {
        if ( chIn != 'y' )
          chIn = Serial.read();
        else
          Serial.read();
      }
      while ( Serial.available() );
    }
  }
  if ( chIn == 'y' ) {
    int8_t result = eRAM.begin();
  	if(result == 0) {
  		eRAM.eraseFlashChip();
  	} else if(result == 1){
  		eRAM.eraseDevice();
  	} else {
  		Serial.println("ERROR!!!!");
  	}
  }
#endif

  Serial.println();
  Serial.println("Mount SPIFFS:");
  eRAM.begin();
  eRAM.fs_mount();
}

void loop() {
}
