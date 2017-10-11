#ifndef ACCESS_SYSTEM_H
#define ACCESS_SYSTEM_H

#include <Arduino.h>

#define ACCESS_SYSTEM_HOST       "192.168.1.70"
#define ACCESS_SYSTEM_PORT       3000
#define ACCESS_SYSTEM_URLPREFIX  "/"
#define ACCESS_SYSTEM_TIMEOUT    3000

// flags for TOKEN_CACHE_ITEM
#define TOKEN_ACCESS    0x01
#define TOKEN_TRAINER   0x02
#define TOKEN_ERROR     0x04

class AccessSystem
{

private:
    String thingId;

    // From https://github.com/zenmanenergy/ESP8266-Arduino-Examples/blob/master/helloWorld_urlencoded/urlencode.ino
    String urlencode(String str)
    {
        String encodedString="";
        char c;
        char code0;
        char code1;
        for (unsigned int i=0; i < str.length(); i++) {
          c=str.charAt(i);
          if (c == ' ') {
            encodedString += '+';
          } else if (isalnum(c)){
            encodedString += c;
          } else {
            code1=(c & 0xf)+'0';
            if ((c & 0xf) > 9){
                code1=(c & 0xf) - 10 + 'A';
            }
            c=(c>>4)&0xf;
            code0=c+'0';
            if (c > 9) {
                code0=c - 10 + 'A';
            }
            encodedString+='%';
            encodedString+=code0;
            encodedString+=code1;
          }
          yield();
        }

        return encodedString;
    }

public:
    AccessSystem(String thingId);
    void sendLogMsg(String msg);    
    uint8_t getAccess(String cardID);
};

#endif