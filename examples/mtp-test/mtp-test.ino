  #include "MTPspiffs.h"
  #include "usb1_mtp.h"

  MTPStorage_SPIFFS storage;
  MTPD       mtpd(&storage);


void logg(uint32_t del, const char *txt)
{ static uint32_t to;
  if(millis()-to > del)
  {
    Serial.println(txt); 
#if USE_SDIO==1
    digitalWriteFast(13,!digitalReadFast(13));
#endif
    to=millis();
  }
}

void setup()
{ 
  while(!Serial && millis()<3000); 
  usb_mtp_configure();
  if(!Storage_init()) {Serial.println("No storage"); while(1);};

  Serial.println("MTP test");
  pinMode(13,OUTPUT);
}

void loop()
{ 
  mtpd.loop();

  logg(1000,"loop");
  //asm("wfi"); // may wait forever on T4.x
}