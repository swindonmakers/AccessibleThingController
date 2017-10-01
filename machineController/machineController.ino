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

// Time the machine stays on for once the card is removed from the reader
#define ACTIVE_TIME_MS 10000
// Time at which machine starts a stage 1 timeout alert (slow short beep)
#define PREWARN1_TIME_MS 5000
#define PREWARN1_BEEP_INTERVAL 1000
#define PREWARN1_BEEP_DURATION 25
// Time at which machine starts a stage 2 timeout alert (fast middling beep)
#define PREWARN2_TIME_MS 7500
#define PREWARN2_BEEP_INTERVAL 500
#define PREWARN2_BEEP_DURATION 250
// Time at which machine starts a stage 3 timeout alert (continuous beep)
#define PREWARN3_TIME_MS 9000

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

unsigned long lastTimeoutBeep = 0;

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
            Serial.println(F("Machine off, new card presented"));
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
                    relayOn();
                }
                // If the relay is already on, no need to beep / turn on, 
                // just extend time, make sure buzzer is off and led is on
                greenOn();
                buzzerOff();
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
    // TODO: work out how to do an early power-down of the machine - ie. user 
    //          wants to stop using the machine before the time is up.
    //          Maybe we need a button for this...?
    if (isRelayOn()) {
        if (millis() - lastOn > ACTIVE_TIME_MS){
            Serial.println(F("Time up, turning machine off"));
            greenOff();
            buzzerOff();
            relayOff();
            // TODO: log that machine has been powered down

        } else if (millis() - lastOn > PREWARN3_TIME_MS && millis()) {
            // Make sure we only trigger prewarn3 once by checking lastTimeoutBeep != 0
            // Note we do this in a sub-if, rather than in the else-if to stop 2 triggering.
            if (lastTimeoutBeep != 0) {
                Serial.println(F("Prewarn 3 triggered"));
                greenOff();
                buzzerOn();
                lastTimeoutBeep = 0;
            }

        } else if (millis() - lastOn > PREWARN2_TIME_MS && millis() - lastTimeoutBeep > PREWARN2_BEEP_INTERVAL) {
            Serial.println(F("Prewarn 2 triggered"));
            greenOff();
            beep(PREWARN2_BEEP_DURATION);
            greenOn();
            lastTimeoutBeep = millis();

        } else if (millis() - lastOn > PREWARN1_TIME_MS && millis() - lastTimeoutBeep > PREWARN1_BEEP_INTERVAL) {
            Serial.println(F("Prewarn 1 triggered"));
            greenOff();
            beep(PREWARN1_BEEP_DURATION);
            greenOn();
            lastTimeoutBeep = millis();
        }
    }


    // Gently blink RED led to indicate controller is alive
    if (millis() - lastLedBounce > LED_BOUNCE_DELAY) {
        redAlternate();
        lastLedBounce = millis();
    }
}