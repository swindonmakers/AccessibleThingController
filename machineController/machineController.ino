/*
== About ==
    Machine controller code, used for access to control to
    an arbitrary "machine" (eg lathe, mill, 3d printer, etc)

== Hardware ==
    - NodeMCU
    - RC522 RFID card reader
    - 240V 10A Relay module
    - RGB LED (for status feedback)
    - Active buzzer (for audibile feedback at timeout / timeup)

== Connections ==

    RC522 CardReader -> NodeMCU pin
    - SDA  -> D4
    - SCK  -> D5
    - MOSI -> D7
    - MISO -> D6
    - RST  -> D0
    - IRQ  -> nc
    - GND  -> GND
    - 3.3V -> 3.3V

    Relay -> NodeMCU pin
    - In   -> D8

    RGB LED -> NodeMCU pin
    - R -> D1
    - G -> D2
    - B -> D3

    Active Buzzer -> NodeMCU pin
    - +ve -> A0
    - -ve -> GND

*/

// Config
// TODO: maybe move this config into flash and add a (password protected) web UI to update it
#define WIFI_SSID "swindon-makerspace"
#define WIFI_PWD "makeallthethings"
#define THING_ID "1"
#define ACTIVE_TIME_MS 5000

// Pin Defines
#define PIN_RELAY D8
#define PIN_LED_R D1
#define PIN_LED_G D2
#define PIN_LED_B D3
#define PIN_BUZZER A0

// Includes
#include <ESP8266WiFi.h>
#include <AccessSystem.h>
#include <TokenCache.h>
#include <CardReader522.h>

// Objects
AccessSystem accessSystem(THING_ID);
TokenCache tokenCache(accessSystem);
CardReader522 cardReader;

// Global state
TOKEN_CACHE_ITEM* item;
unsigned long lastOn = 0;

void relayOff()
{
    Serial.println(F("Turning relay off"));
    digitalWrite(PIN_RELAY, HIGH);
}

void relayOn()
{
    Serial.println(F("Turning relay on"));
    digitalWrite(PIN_RELAY, LOW);
}

void setup()
{
    Serial.begin(115200);
    Serial.println(F("Machine controller v1"));
    
    pinMode(PIN_RELAY, OUTPUT);
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_LED_B, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);

    relayOff();

    Serial.println(F("Init..."));
    cardReader.init();
    tokenCache.init();

    Serial.print(F("Connecting wifi"));
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PWD);
    while(WiFi.status() != WL_CONNECTED) {
        Serial.print(F("."));
        delay(500);
    }
    Serial.println(F("Connected"));

    // TODO: log node startup to server
}

void loop()
{
    // TODO: make sure wifi stays connected (should be in a resuable library)

    if (cardReader.check()) {

        item = tokenCache.fetch(&cardReader.lastUID, cardReader.lastLen, cardReader.lastToken);
        
        if (item != NULL) {
            if (item->flags && TOKEN_ACCESS) {
                // Permission granted, so open/power the machine
                item->count++;

                Serial.print(F("Permission granted: "));
                Serial.println(item->count);
                // TODO: log to sever that user has activated machine
                // TODO: change LED to tell user machine is powered
                relayOn();
                lastOn = millis();
            }
            else
            {
                Serial.println(F("Permission denied."));
                // TODO: log to server that user has tried to use machine, but access denied
                // TODO: change LED to tell user access denied
            }
        }
        else
        {
            Serial.println(F("Token not found"));
            // TODO: log to server that user has tried to use machine, but token not found
            // TODO: change LED to tell user token not found
        }
    }

    // TODO: define how long the machine should stay on for
    // TODO: alert the user with audible timeout before their time expires
    //         i.e. via a steadily quickening buzz-buzz noise and flashing of LED
    // TODO: check that leaving a card on the reader means the machine stays on
    // TODO: work out how to do an early power-down of the machine - ie. user 
    //          wants to stop using the machine before the time is up.
    //          Maybe we need a button for this...?
    // TODO: log that machine has been powered down
    if (millis() - lastOn > ACTIVE_TIME_MS){
        relayOff();
    }
}