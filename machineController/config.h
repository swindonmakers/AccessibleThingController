/*
    Configurable stuff for the Machine Controller

    Intention is to duplicate this file for different machines so we can store config in git.
    i.e. have
     - config-3dPrinters.h
     - config-metalLathe.h
     - config-laserCutter.h
    and then copy the appropriate one to config.h when flashing the ESP
*/

// Wifi config
#define WIFI_SSID "swindon-makerspace"
#define WIFI_PWD "makeallthethings"

// Machines thingId in the access system.
#define THING_ID "1"

// Time the machine stays on for once the card is removed from the reader
#define ACTIVE_TIME_MS 10000

// Time at which machine starts a stage 1 timeout alert (slow short beep)
// Should be < ACTIVE_TIME_MS
#define PREWARN1_TIME_MS 5000
#define PREWARN1_BEEP_INTERVAL 1000
#define PREWARN1_BEEP_DURATION 25

// Time at which machine starts a stage 2 timeout alert (fast middling beep)
// Should be > PREWARN1_TIME_MS and < ACTIVE_TIME_MS
#define PREWARN2_TIME_MS 7500
#define PREWARN2_BEEP_INTERVAL 500
#define PREWARN2_BEEP_DURATION 250

// Time at which machine starts a stage 3 timeout alert (continuous beep)
// Should be > PREWARN2_TIME_MS and < ACTIVE_TIME_MS
#define PREWARN3_TIME_MS 9000
