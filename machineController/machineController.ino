/* Connections

    CardReader522
    - SDA => D4
    - SCK => D5
    - MOSI => D7
    - MISO => D6
    - IRQ => nc
    - GND
    - RST => D0
    - 3.3V => 3.3V

    Relay D3

*/

#include <ESP8266WiFi.h>

#include <AccessSystem.h>
#include <TokenCache.h>
#include <CardReader522.h>

#define THING_ID "1"

AccessSystem accessSystem(THING_ID);
TokenCache tokenCache(accessSystem);
CardReader522 cardReader;

TOKEN_CACHE_ITEM* item;

unsigned long lastOn = 0;

void setup()
{
    Serial.begin(115200);
    Serial.println("Machine controller v1");

    cardReader.init();
    tokenCache.init();

    pinMode(D3, OUTPUT);
    digitalWrite(D3, HIGH);

    Serial.print("Connecting wifi");
    WiFi.mode(WIFI_STA);
    WiFi.begin("swindon-makerspace2", "makeallthethings");
    while(WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("Connected");
}

void loop()
{
    if (cardReader.check()) {

        item = tokenCache.fetch(&cardReader.lastUID, cardReader.lastLen, cardReader.lastToken);
        
        if (item != NULL) {
            if (item->flags && TOKEN_ACCESS) {
                // permission given, so open/power the machine
                item->count++;

                Serial.print(F("Permission granted: "));
                Serial.println(item->count);

                Serial.println("turning relay on");
                digitalWrite(D3, LOW);
                lastOn = millis();
            }
        }
        else
        {
            Serial.println("Token not found");
        }
    }

    if (millis() - lastOn > 5000){
        digitalWrite(D3, HIGH);
    }
}