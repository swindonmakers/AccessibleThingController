/*
== About ==
    Machine controller code, used for access to control to
    an arbitrary "machine" (eg lathe, mill, 3d printer, etc)

== Hardware ==
    - NodeMCU
    - RC522 RFID card reader
    - 240V 10A Relay module
    - LED's (for status feedback)
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
    - In   -> D3

    LEDs -> NodeMCU pin 
    (Pin HIGH = LED on: Pin -> LED -> Resistor -> GND)
    - R -> D1
    - G -> D2

    Active Buzzer -> NodeMCU pin
    - +ve -> D8
    - -ve -> GND

*/

// Config
// TODO: maybe move this config into flash and add a (password protected) web UI to update it
#define WIFI_SSID "swindon-makerspace"
#define WIFI_PWD "makeallthethings"
#define THING_ID "1"
#define ACTIVE_TIME_MS 5000

// Pin Defines
#define PIN_RELAY D3
#define PIN_LED_R D1
#define PIN_LED_G D2
#define PIN_BUZZER D8
#define PIN_BUTTON 10 //?

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

// ActivityLED - blink red led if nothing happening
#define LED_BOUNCE_DELAY 500
unsigned long lastLedBounce = 0;

void relayOff()
{
    if (digitalRead(PIN_RELAY) == LOW) {
        Serial.println(F("Turning relay off"));
        digitalWrite(PIN_RELAY, HIGH);
    }
}

void relayOn()
{
    Serial.println(F("Turning relay on"));
    digitalWrite(PIN_RELAY, LOW);
}

bool isRelayOn()
{
    return digitalRead(PIN_RELAY)  == LOW;
}

bool isRelayOff()
{
    return !isRelayOn();
}

void redOn()
{
    digitalWrite(PIN_LED_R, HIGH);
}

void redOff()
{
    digitalWrite(PIN_LED_R, LOW);
}

void redAlternate()
{
    digitalWrite(PIN_LED_R, !digitalRead(PIN_LED_R));
}

void greenOn()
{
    digitalWrite(PIN_LED_G, HIGH);
}

void greenOff()
{
    digitalWrite(PIN_LED_G, LOW);
}


void buzzerOn()
{
    digitalWrite(PIN_BUZZER, HIGH);
}

void buzzerOff()
{
    digitalWrite(PIN_BUZZER, LOW);
}

void beep(int ms)
{
    buzzerOn();
    delay(ms);
    buzzerOff();
}

void setup()
{
    // Init Serial
    Serial.begin(115200);
    Serial.println(F("Machine controller v1"));
    Serial.println(F("Starting..."));
    
    // Set pin modes
    pinMode(PIN_RELAY, OUTPUT);
    pinMode(PIN_LED_R, OUTPUT);
    pinMode(PIN_LED_G, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);

    // Set initial pin states
    relayOff();
    redOff();
    greenOff();
    beep(500);

    // Init helpers & hardware
    cardReader.init();
    tokenCache.init();

    // Connect to wifi
    // TODO: move wifi connection to main loop to facilitate reconnect
    Serial.print(F("Connecting wifi"));
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PWD);
    while(WiFi.status() != WL_CONNECTED) {
        Serial.print(F("."));
        redOn();
        beep(10);
        redOff();
        delay(500);
    }
    Serial.println(F("Connected"));

    // TODO: log node startup to server
}

void loop()
{
    // TODO: make sure wifi stays connected (should be in a resuable library)

    if (cardReader.check()) {
        if (isRelayOff()) {
            beep(50);
            redOff();
        }

        item = tokenCache.fetch(&cardReader.lastUID, cardReader.lastLen, cardReader.lastToken);
        
        if (item != NULL) {
            if (item->flags && TOKEN_ACCESS) {
                if (isRelayOff()) {
                    // Permission granted, so open/power the machine
                    item->count++;

                    Serial.print(F("Permission granted: "));
                    Serial.println(item->count);
                    // TODO: log to sever that user has activated machine
                    greenOn();
                    relayOn();
                }
                // If the relay is already on, no need to beep / turn on, just extend time
                lastOn = millis();
            }
            else
            {
                Serial.println(F("Permission denied."));
                relayOff();
                redOn();
                beep(2000);
                redOff();
                // TODO: log to server that user has tried to use machine, but access denied
            }
        }
        else
        {
            Serial.println(F("Token not found"));
            relayOff();
            redOn();
            beep(500);
            redOff();
            delay(500);
            redOn();
            beep(1500);
            redOff();
            // TODO: log to server that user has tried to use machine, but token not found
        }
    }

    // TODO: see if two valid users can hand over the machine keeping it on
    // TODO: define how long the machine should stay on for
    // TODO: alert the user with audible timeout before their time expires
    //         i.e. via a steadily quickening buzz-buzz noise and flashing of LED
    // TODO: work out how to do an early power-down of the machine - ie. user 
    //          wants to stop using the machine before the time is up.
    //          Maybe we need a button for this...?
    if (millis() - lastOn > ACTIVE_TIME_MS){
        greenOff();
        relayOff();
        // TODO: log that machine has been powered down
    }

    // Gently blink RED led to indicate controller is alive
    if (millis() - lastLedBounce > LED_BOUNCE_DELAY) {
        redAlternate();
        lastLedBounce = millis();
    }
}