#include <AccessSystem.h>
#include <TokenCache.h>

#define THING_ID "1"

AccessSystem accessSystem(THING_ID);
TokenCache tokenCache(accessSystem);

void setup()
{
    tokenCache.init();

    TOKEN uid;  // Buffer to store the returned UID
    uint8_t uidLength; // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

    TOKEN_CACHE_ITEM* item;

    item = tokenCache.fetch(&uid, uidLength, "TOKEN STRING");
    
    if (item != NULL) {
        
        if (item->flags && TOKEN_ACCESS) {
            // permission given, so open/power the machine
            item->count++;

            Serial.print(F("Permission granted: "));
            Serial.println(item->count);

            // TODO: turn on power
        }
    }
}


void loop()
{

}

 
 
