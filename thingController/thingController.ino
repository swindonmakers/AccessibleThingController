#define DEBUG_ESP_PORT Serial
#define DDEBUG_ESP_CORE 
#define DDEBUG_ESP_WIFI

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <TaskScheduler.h>
#include <EEPROM.h>

/* ========================================================================== *
 *  Pinout
 * ========================================================================== */
// NB: Do not use D0 or D2, these need to be pulled high by 10k resistors to ensure correct
//     boot sequence following watchdog/internal reset
#define I2C_DATA_PIN      D3
#define I2C_CLOCK_PIN     D1
#define PN532_RESET_PIN   D4
#define DOOR_SENSOR_PIN   D5
#define OUTPUT_PIN        D6

/* ========================================================================== *
 *  Enums
 * ========================================================================== */

/* ========================================================================== *
 *  Configuration
 * ========================================================================== */

#define VERSION           "0.1"
#define EEPROM_MAGIC      2  // update to clear EEPROM on restart
#define THING_ID          "1A9E3D66-E90F-11E5-83C1-1E346D398B53"
#define THING_NAME        "DOOR"
#define CONTROL_ALLOWED_MODE      OUTPUT
#define CONTROL_ALLOWED_VALUE     LOW
#define CONTROL_DISALLOWED_MODE   OUTPUT
#define CONTROL_DISALLOWED_VALUE  HIGH
//#define NETWORK_SSID      "PI_Guest"
//#define NETWORK_PASSWORD  "@Trop1cana@"
#define NETWORK_SSID      "desert-island-2g"
#define NETWORK_PASSWORD  "messageinabottle"
#define SERVER_HOST       "desert-island.me.uk"
#define SERVER_PORT       80
#define SERVER_URLPREFIX  "/accesssystem/"
#define TELEGRAM_HOST     "149.154.167.198"  // api.telegram.org
#define WIFI_CONNECTION_TASK_INTERVAL   60000 // milliseconds
#define RFID_CONNECTION_TASK_INTERVAL   10000 // milliseconds
#define LOOKFORCARD_TASK_INTERVAL       100   // milliseconds
#define SYNC_CACHE_TASK_INTERVAL        600000 // milliseconds
#define MONITORDOORSENSOR_TASK_INTERVAL 500   // milliseconds
#define MONITOROUTPUT_TASK_INTERVAL     500   // milliseconds
#define CARD_DEBOUNCE_DELAY             2000  // milliseconds
#define PN532_READ_TIMEOUT              100   // milliseconds
#define CACHE_SIZE        32
#define CACHE_SYNC        240   // resync cache after <value> x 10 minutes

#define OUTPUT_ENABLE_DURATION          10000 // milliseconds


/* ========================================================================== *
 *  Types / Structures
 * ========================================================================== */

 /*
  * struct for token cache
  * token - 7 bytes
  * length - unsigned byte - length of token
  * flags - 1 byte, encodes permission and trainer status
  * scan count - 2 bytes (unsigned int) - number of scans
  *
  * cache is fixed sized array
  * selection sorted on token
  * if cache size exceeded then another item is removed,
  * starting with badges that have no access, followed by smallest scan count
  *
  * cache can be debugged over serial, but not wifi, to prevent snooping of valid token numbers
  */

// tokens are 4 or 7-byte values, held in a fixed 7-byte array
typedef  uint8_t TOKEN[7];

// flags for TOKEN_CACHE_ITEM
#define TOKEN_ACCESS    0x01
#define TOKEN_TRAINER   0x02
#define TOKEN_ERROR     0x04

// struct for items in the token cache
struct TOKEN_CACHE_ITEM {
    TOKEN token;      // the token uid
    uint8_t length;   // length of token in bytes
    uint8_t flags;    // permission bits
    uint16_t count;   // scan count
    uint8_t sync;     // countdown to resync with cache with server
};
// stored size = 12 bytes


/* ========================================================================== *
 *  Prototypes for task callbacks, etc
 * ========================================================================== */

// tasks
void keepWifiConnected();
void keepRFIDConnected();
void lookForCard();
void displayUptime();
void syncCache();
void monitorDoorSensor();
void monitorOutput();

// other prototypes
uint8_t queryServer(String cardID);

void syncEEPROM();

/* ========================================================================== *
 *  Global Variables / Objects
 * ========================================================================== */

const char* ssid     = NETWORK_SSID;
const char* password = NETWORK_PASSWORD;
const char* host = SERVER_HOST;
const int hostPort = SERVER_PORT;

PN532_I2C pn532i2c(Wire, I2C_DATA_PIN, I2C_CLOCK_PIN);  // data, clock
PN532 nfc(pn532i2c);

// the cache
TOKEN_CACHE_ITEM cache[CACHE_SIZE];

// number of items in cache
uint8_t cacheSize = 0;

// tasks
Task WifiConnectionTask(WIFI_CONNECTION_TASK_INTERVAL, TASK_FOREVER, &keepWifiConnected);
Task RFIDConnectionTask(RFID_CONNECTION_TASK_INTERVAL, TASK_FOREVER, &keepRFIDConnected);
Task lookForCardTask(LOOKFORCARD_TASK_INTERVAL, TASK_FOREVER, &lookForCard);
Task displayUptimeTask(60000, TASK_FOREVER, &displayUptime);
Task syncCacheTask(SYNC_CACHE_TASK_INTERVAL, TASK_FOREVER, &syncCache);
Task monitorDoorSensorTask(MONITORDOORSENSOR_TASK_INTERVAL, TASK_FOREVER, &monitorDoorSensor);
Task monitorOutputTask(MONITOROUTPUT_TASK_INTERVAL, TASK_FOREVER, &monitorOutput);

// scheduler
Scheduler runner;

// output
unsigned long outputEnableTimer;

/* ========================================================================== *
 *  Utility Functions
 * ========================================================================== */

String byteArrayToHexString(const uint8_t *data, const uint32_t numBytes) {
  String res = "";
  for (uint8_t i = 0; i < numBytes; i++) {
        res += String(data[i], HEX);
    }
  return res;
}

void uptime(boolean display = true)
{
  static uint16_t rollover = 0;  // to count timer rollovers
  static boolean highMillis = false;
  long days=0;
  long hours=0;
  long mins=0;
  long secs=0;
  unsigned long ms = millis();

  // track rollover
  if (ms > 100000) {
    highMillis = true;
  }
  if (ms < 100000 && highMillis) {
    rollover++;
    highMillis = false;
  }
  
  secs = millis()/1000;
  mins=secs/60; 
  hours=mins/60;
  days=(rollover * 50) + hours/24; 
  secs=secs-(mins*60); //subtract the coverted seconds to minutes in order to display 59 secs max 
  mins=mins-(hours*60); //subtract the coverted minutes to hours in order to display 59 minutes max
  hours=hours-(days*24); //subtract the coverted hours to days in order to display 23 hours max
  
  //Display results
  if (display) {
    Serial.print("Uptime: ");
    if (days>0) // days will displayed only if value is greater than zero
    {
        Serial.print(days);
        Serial.print("d ");
    }
    Serial.print(hours);
    Serial.print(":");
    Serial.print(mins);
    Serial.print(":");
    Serial.println(secs);
  }
}

void displayUptime() {
  uptime(true);
}


/* ========================================================================== *
 *  Output
 * ========================================================================== */

// in door mode:  true = unlocked, false = locked
// in machine mode: true = powered, false = unpowered

// duration in milliseconds
void allowedOutput(unsigned long duration) {
  Serial.println("+++ Output allowed");
  pinMode(OUTPUT_PIN, CONTROL_ALLOWED_MODE);
  digitalWrite(OUTPUT_PIN, CONTROL_ALLOWED_VALUE);
  outputEnableTimer = millis() + duration;
}

void disallowedOutput() {
  Serial.println("--- Output disallowed");
  pinMode(OUTPUT_PIN, CONTROL_DISALLOWED_MODE);
  digitalWrite(OUTPUT_PIN, CONTROL_DISALLOWED_VALUE);
}

// returns true if in allowed mode/value.
boolean getOutputStatus() {
  // TODO: sort inversion for diff modes
  //return digitalRead(OUTPUT_PIN);
  return false;
}

// task to monitor output status, disable when necessary
void monitorOutput() {
  if (millis() > outputEnableTimer) {
    disallowedOutput();
  }
}

/* ========================================================================== *
 *  Token Cache
 * ========================================================================== */

// Get cache item by token, returns null if not in cache
TOKEN_CACHE_ITEM* getTokenFromCache(TOKEN* token, uint8_t length) {
  // search through items to find a match to token
  // TODO: binary search?

  TOKEN_CACHE_ITEM* item = NULL;

  uint8_t i;
  for (i=0; i<cacheSize; i++) {
    if ((length == cache[i].length) && (memcmp(token, &cache[i], length)==0)) {
      item = &cache[i];
      break;
    }
  }
  
  return item;
}


// add a token to the cache, returns pointer to new cache item
TOKEN_CACHE_ITEM* addTokenToCache(TOKEN* token, uint8_t length, uint8_t flags) {
  // if cache not full, then add a new item to end of array
  uint8_t pos = cacheSize;
  uint8_t i;
  
  // else, find item with least scans 
  if (cacheSize == CACHE_SIZE) {
    cacheSize = 0;  // start at the beginning
    
    for (i=0; i < cacheSize; i++) {
      if (cache[i].count < cache[pos].count) {
        pos = i;
      }
    }
  } else {
    cacheSize++;
  }

  // write new token into the cache
  memcpy(&cache[pos].token, token, length);
  cache[pos].length = length;
  cache[pos].flags = flags;
  cache[pos].count = 1;
  cache[pos].sync = CACHE_SYNC;

  // TODO: selection sort all items

  // write new info to EEPROM
  syncEEPROM();

  // return new item
  return &cache[pos];
}

void initCache() {
  Serial.println("Loading cache from EEPROM...");
  
  EEPROM.begin(2 + 8 * CACHE_SIZE);

  // read magic
  if (EEPROM.read(0) != EEPROM_MAGIC) {
    Serial.println("Magic changed, resetting cache");
    // reset stored cache size
    EEPROM.write(0, EEPROM_MAGIC);
    EEPROM.write(1, 0);
    EEPROM.commit();

    cacheSize = 0;
  } else {
    cacheSize = EEPROM.read(1);
    Serial.print(cacheSize);
    Serial.println(" items");
  }

  // read token items from cache
  uint8_t i = 0, j = 0;
  uint16_t addr = 2;
  for (i=0; i<cacheSize; i++) {
    // token
    for (j=0; j<7; j++) {
      cache[i].token[j] = EEPROM.read(addr + j);
    }
    
    // length
    cache[i].length = EEPROM.read(addr + 7);
    
    // flags
    cache[i].flags = EEPROM.read(addr + 8);

    Serial.print("  ");
    Serial.print(byteArrayToHexString(cache[i].token, cache[i].length));
    Serial.print(" : ");
    Serial.println(cache[i].flags);

    cache[i].count = 0;
    cache[i].sync = 2 + i;  // resync everything soon-ish

    addr += 9;
  }
}

// task to update permission flags in cache, e.g. if someones access has changed
void syncCache() {
  // give up if we don't have a valid wifi connection
  if ( WiFi.status() != WL_CONNECTED ) {
     return;
   }
  
  // for each item in cache
  uint8_t i;
  for (i=0; i<cacheSize; i++) {
    // dec sync counter
    cache[i].sync--;

    // if reached zero
    if (cache[i].sync == 0) {
      Serial.print("Syncing cached flags for: ");
      Serial.println(byteArrayToHexString(cache[i].token, cache[i].length));
      
      // query permission flags from server
      uint8_t flags = queryServer(byteArrayToHexString(cache[i].token, cache[i].length));

      if (flags != TOKEN_ERROR) {
         // if successful, update flags and reset sync counter
         cache[i].flags = flags;
         cache[i].sync = CACHE_SYNC;
      } else {
        // else try again next cycle
        cache[i].sync = 1;
      }
      
    }
  }

  // sync changes to EEPROM
  syncEEPROM();
}


// update a byte of EEPROM memory, return true if changed
boolean updateEEPROM(int address, uint8_t value) {
  boolean changed = EEPROM.read(address) != value;
  if (changed) {
    EEPROM.write(address, value);
  }
  return changed;
}

// update EEPROM to match cache
void syncEEPROM() {
  boolean changed = false;
  
  changed |= updateEEPROM(1, cacheSize);

  // update items from cache
  uint8_t i = 0, j = 0;
  uint16_t addr = 2;
  for (i=0; i<cacheSize; i++) {
    // token
    for (j=0; j<7; j++) {
      changed |= updateEEPROM(addr + j, cache[i].token[j]);
    }
    
    // length
    changed |= updateEEPROM(addr + 7, cache[i].length);
    
    // flags
    changed |= updateEEPROM(addr + 8, cache[i].flags);
    
    addr += 9;
  }

  if (changed) {
    Serial.println("Updating EEPROM");
    EEPROM.commit();
  }
}


/* ========================================================================== *
 *  HTTP Client
 * ========================================================================== */

// query server for token, and return flags
uint8_t queryServer(String cardID) {
   // TODO: Rewrite/refactor
   uint8_t flags = 0;
   
   // check if connected
   if ( WiFi.status() != WL_CONNECTED ) {
     Serial.println("Error: WiFi Not Connected");
     return TOKEN_ERROR;
   }
  
   Serial.print("Querying server: ");
   Serial.println(host);

   // Use WiFiClient class to create TCP connections
   WiFiClient client;
   if (!client.connect(host, hostPort)) {
     Serial.println("Error: Connection failed");
     return TOKEN_ERROR;
   }

   // We now create a URI for the request
   String url = SERVER_URLPREFIX;
   url += "verify";
   url += "?token=";
   url += cardID;
   url += "&thing=";
   url += THING_ID;

   Serial.print("Requesting URL: ");
   Serial.println(url);

   // This will send the request to the server
   client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                "Host: " + host + "\r\n" +
                "Connection: close\r\n\r\n");
   int checkCounter = 0;
   while (!client.available() && checkCounter < 300) {
     delay(10);
     checkCounter++;
   }
   Serial.println("Bored of waiting for response, checkCounter=");
   Serial.println(checkCounter);

   // Read reply and decode json
   while(client.available()){
     // TODO: Replace with better/more robust streaming HTTP header parser
     String json;
     boolean endOfHeaders = false;
     while (!endOfHeaders && client.available()) {
       // feed the watchdog
       ESP.wdtFeed(); 
       json = client.readStringUntil('\n');
       Serial.print("_");
       Serial.print(json);
       if (json == "") endOfHeaders = true;
     }
     Serial.println("json:");
     Serial.println(json);

     const int BUFFER_SIZE = JSON_OBJECT_SIZE(3) + JSON_OBJECT_SIZE(1);
     Serial.print("BUFFER_SIZE=");
     Serial.println(BUFFER_SIZE);
     StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

     JsonObject& root = jsonBuffer.parseObject(json);

     // Test if parsing succeeds.
     if (!root.success()) {
       Serial.println("Error: Couldn't parse JSON");
       return TOKEN_ERROR;
     }

     // Check json response for access permission
     if (root["access"] == 1) {
       flags |= TOKEN_ACCESS;
     }

     if (root["trainer"] == 1) {
       flags |= TOKEN_TRAINER;
     }
   }
   
   // close connection
   client.stop();

   return flags;
}


// send a log msg to the server, fire and forget
void sendLogMsg(String msg) {
   Serial.print("Sending log to server: ");
   Serial.println(host);
   
   // check if connected
   if ( WiFi.status() != WL_CONNECTED ) {
    Serial.println("Error: WiFi Not Connected");
    return;
   }

   WiFiClient client;
   if (!client.connect(host, hostPort)) {
     Serial.println("Error: Connection failed");
     return;
   }

   String url = SERVER_URLPREFIX;
   url += "msglog?thing=";
   url += THING_ID;
   url += "&msg=";
   url += msg;

   Serial.println(url);

   // This will send the request to the server
   client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                "Host: " + host + "\r\n" +
                "Connection: close\r\n\r\n");

   // don't care if it succeeds, so close connection and return
   client.flush();
   client.stop();
   return;
}



void sendTelegramMsg(String msg) {
   Serial.print("Sending msg to telegram");
   
   // check if connected
   if ( WiFi.status() != WL_CONNECTED ) {
    Serial.println("Error: WiFi Not Connected");
    return;
   }

   WiFiClientSecure client;
   if (!client.connect(TELEGRAM_HOST, 443)) {
     Serial.println("Error: Connection failed");
     return;
   }

   String url = "/bot189740736:AAFaZX7TaqEMg8g9bjrAkzaSH3gbAdt7vpE/sendMessage?chat_id=-20679102&text=";
   url += msg;

   Serial.println(url);

   // This will send the request to the server
   client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                "Host: " + TELEGRAM_HOST + "\r\n" +
                "Connection: close\r\n\r\n");

   // don't care if it succeeds, so close connection and return
   client.flush();
   client.stop();
   return;
}


/* ========================================================================== *
 *  PN532
 * ========================================================================== */

void resetPN532() {
  Serial.println("resetting PN532");

  digitalWrite(PN532_RESET_PIN, HIGH);
  delay(15);
  digitalWrite(PN532_RESET_PIN, LOW);
  delay(15);
  digitalWrite(PN532_RESET_PIN, HIGH);
  delay(15);

  Serial.println("reset pin toggle done");
  
  nfc.begin();

  // added ref issue: https://github.com/Seeed-Studio/PN532/issues/44
  nfc.setPassiveActivationRetries(0x19);

  // PN532 manual 7.2.3, only the first two bytes:
  // err, field: last error code, external rf field detected?
  Serial.print("general status:       "); Serial.println(nfc.getGeneralStatus());
  // PN532 manual 7.2.2, expected 0x32010607.
  // - 0x32 chip = PN5*32*
  // - 0x01 firmware version = 1
  // - 0x06 firmware revision = 6
  // - 0x07 supported cards = iso18092 + iso/iec 14443 Type B + 14443-A
  Serial.print("get firmware version: "); Serial.println(nfc.getFirmwareVersion(), HEX);
  
  
  //Serial.println(nfc.getGeneralStatus());
  //Serial.println(nfc.getFirmwareVersion());

  nfc.SAMConfig();
}


// Called in main loop
void keepRFIDConnected() {
  
   // seem to need to call this before a getFirmwareVersion to get reliable response!?!
   uint16_t generalstatus = nfc.getGeneralStatus();
   Serial.print("General status (in keepRFIDConnected): ");
   Serial.println(generalstatus, HEX);

   uint32_t versiondata = nfc.getFirmwareVersion();

   if (! versiondata) {
      Serial.println("PN532 -> Error - Resetting");

      // Resetting the PN532 doesn't seem to resolve I2C lock-up, needs ESP reset
      // or... maybe this is fixed, now the clock stretching bit is patched
      resetPN532();

      // Reset the ESP !!!
      //ESP.reset();

      // Redundant, as ESP reboots and never gets here
      //RFIDConnectionTask.setInterval(1000);
   } else {
      Serial.println("PN532 -> OK");

      // configure board to read RFID tags - again and again
      nfc.SAMConfig();

      RFIDConnectionTask.setInterval(RFID_CONNECTION_TASK_INTERVAL);
   }
}

// Called from main loop to poll for card
void lookForCard() {
  uint8_t success;
  TOKEN uid;  // Buffer to store the returned UID
  uint8_t uidLength; // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

  static TOKEN luid;  // last scanned uid, for debounce
  static unsigned long lastChecked;
  
  TOKEN_CACHE_ITEM* item;

  //Serial.print(".");

  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, PN532_READ_TIMEOUT);

  if (success && (memcmp(luid, uid, 7)!=0 || millis() > lastChecked + CARD_DEBOUNCE_DELAY)) {

    // store the uid to permit debounce
    memcpy(luid, uid, 7);

    lastChecked = millis();

    Serial.print("Card found, token: ");  Serial.println(byteArrayToHexString(uid, uidLength));

    // check cache 
    item = getTokenFromCache(&uid, uidLength); 

    //if not found, then query server and add to cache if has permission
    if (item == NULL) {
      Serial.println("Not in cache");
      uint8_t flags = queryServer(byteArrayToHexString(uid, uidLength));
      Serial.println(flags);
      if ((flags > 0) && (flags != TOKEN_ERROR)) {
        item = addTokenToCache(&uid, uidLength, flags);
      }
    } else {
      Serial.println("In cache");
    }

    // if got valid details and permission given, then open/power the thing, if not, don't
    if (item != NULL) {

      // take action and send log to server
      if (item->flags && TOKEN_ACCESS) {
        // permission given, so open/power the thing
        item->count++;
        
        Serial.print("Permission granted: ");
        Serial.println(item->count);
        sendLogMsg("Permission%20granted%20to:%20" + byteArrayToHexString(uid, uidLength));
        //sendTelegramMsg("Door opened");

        allowedOutput(OUTPUT_ENABLE_DURATION);
      } else {
        // permission denied!
        Serial.println("Permission denied");
        sendLogMsg("Permission%20denied%20to:%20" + byteArrayToHexString(uid, uidLength));
      }

    } else {
      Serial.println("Error: Couldn't determine permissions");
    }
    
  }
}


/* ========================================================================== *
 *  WiFi Client
 * ========================================================================== */

// Called in main loop
void keepWifiConnected() {

   if (WiFi.status() != WL_CONNECTED) {
      Serial.print("WiFi -> Connecting to: ");  Serial.println(ssid);

      WiFi.disconnect(true); // abandon any previous connection attempt
      WiFi.mode(WIFI_STA);  // force mode to STA only
      WiFi.begin(ssid, password);  // try again

      // check again to see if connection established...
      WifiConnectionTask.setInterval(WIFI_CONNECTION_TASK_INTERVAL);
   } else {
      Serial.print("WiFi -> OK, IP: ");
      Serial.println(WiFi.localIP());

      // sorted, shouldn't need to check again for a while
      WifiConnectionTask.setInterval(WIFI_CONNECTION_TASK_INTERVAL);
   }

}


/* ========================================================================== *
 *  Door Sensor
 * ========================================================================== */

void monitorDoorSensor() {
  static boolean doorOpen = false;  // true = open, false = closed
  
  //Serial.println(digitalRead(DOOR_SENSOR_PIN));  

  // sensor value: 0 = closed, 1 = open
  boolean newDoorOpen = digitalRead(DOOR_SENSOR_PIN);

  // see if state has changed
  if (newDoorOpen != doorOpen) {
    doorOpen = newDoorOpen;

    if (doorOpen) {
      Serial.println("Door opened"); 

      // Compare to lock status...  scream if not expected to be opened!!
      if (!getOutputStatus()) {
        Serial.println("AAARRGH: Door opened, but still locked!!!");
        sendTelegramMsg("AARGH someone has forced the door open");
      }
      
    } else {
      Serial.println("Door closed");
    }
  }
}


/* ========================================================================== *
 *  Setup
 * ========================================================================== */

void setup(void) {
  // clear any stored wifi config
  WiFi.disconnect(true);
  WiFi.softAPdisconnect();
  WiFi.setAutoReconnect(false);
  
  // configure pins
  pinMode(PN532_RESET_PIN, OUTPUT);
  /* Don't configure output pin, disallowedOutput() will do that. */

  // start serial
  Serial.begin(115200);

  Serial.setDebugOutput(true);

  // make sure door is locked / machine is off
  disallowedOutput();
  
  
  Serial.println("");
  Serial.println("AccessibleThingController");
  Serial.print("V:");  Serial.println(VERSION);


  // Start PN532
  Serial.println("Connecting to PN532...");
  resetPN532();

  // init EEPROM and load cache
  initCache();

  // Setup scheduler
  Serial.println("Configuring tasks...");
  runner.init();
  runner.addTask(WifiConnectionTask);
  runner.addTask(RFIDConnectionTask);
  runner.addTask(lookForCardTask);
  runner.addTask(displayUptimeTask);
  runner.addTask(syncCacheTask);
  runner.addTask(monitorOutputTask);
  
  // Enable tasks
  WifiConnectionTask.enableDelayed(30000);  // enough time to open the door before potential watchdog reset
  RFIDConnectionTask.enable();
  lookForCardTask.enableDelayed(2000);
  displayUptimeTask.enable();
  syncCacheTask.enable();
  monitorOutputTask.enable();

  // mode-specific tasks
/*  if (CONTROL_MODE == MODE_DOOR) {
    runner.addTask(monitorDoorSensorTask);
    monitorDoorSensorTask.enable();
  }
*/

  Serial.println("Ready");
}


/* ========================================================================== *
 *  Main Loop
 * ========================================================================== */

void loop(void) {  
  // execute tasks
  runner.execute();

  // keep track of uptime
  uptime(false);

  // TODO:
  // I've rebooted log msg
  // send heartbeat to server - 10min?
  // Manage display/LED
  // Manage buzzer
  // Monitor button(s) - exit button or timer buttons
}
