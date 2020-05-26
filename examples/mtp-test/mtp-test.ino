  #include "MTPspiffs.h"
  #include "usb1_mtp.h"

  MTPStorage_SPIFFS storage;
  MTPD       mtpd(&storage);


void logg(uint32_t del, const char *txt)
{ static uint32_t to;
  if(millis()-to > del)
  {
    Serial.println(txt); 
    digitalWriteFast(13,!digitalReadFast(13));
    to=millis();
  }
}

void setup()
{ 
  while(!Serial && millis()<3000); 
  usb_mtp_configure();
  
  delay(5000);	//need this to give time for Serial to come on line
  if(!Storage_init()) {Serial.println("No storage"); while(1);};
  
  Serial.println("\n Enter 'y' in 6 seconds to format Storage - other to skip");
  Serial.println("\n for FLASH you only have to do this once!!!!!");
  Serial.println("\n for PSRAM you have to do this when you first power on Teensy 4.1!!!!!");
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
	initializeStorage();
  }
  
  

  Serial.println("MTP test");
  pinMode(13,OUTPUT);
}

void loop()
{ 
  mtpd.loop();

  logg(1000,"loop");
  //asm("wfi"); // may wait forever on T4.x
}