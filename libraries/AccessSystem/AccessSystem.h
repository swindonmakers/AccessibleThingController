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

public:
    AccessSystem(String thingId);
    void sendLogMsg(String msg);    
    uint8_t getAccess(String cardID);
};

#endif