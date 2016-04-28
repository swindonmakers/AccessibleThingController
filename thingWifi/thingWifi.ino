// Enable WIFI debugging
#define DEBUG_ESP_PORT Serial
#define DDEBUG_ESP_CORE
#define DDEBUG_ESP_WIFI

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <TaskScheduler.h>

/* ========================================================================== *
 *  Pinout
 * ========================================================================== */
// NB: Do not use D0 or D2, these need to be pulled high by 10k resistors to ensure correct
//     boot sequence following watchdog/internal reset


#define DEBUG_CAPSENSE

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
#define CONTROL_MODE      MODE_DOOR
#define NETWORK_SSID      "swindon-makerspace"
#define NETWORK_PASSWORD  "makeallthethings"
#define SERVER_HOST       "192.168.1.70"
#define SERVER_PORT       3000
#define SERVER_URLPREFIX  "/"
// http://www.swindon-makerspace.org/api/telegram/?msg=i+do+not+spellz+good&groupid=-20679102
#define TELEGRAM_HOST     "www.swindon-makerspace.org"
#define WIFI_CONNECTION_TASK_INTERVAL   60000 // milliseconds
#define RFID_CONNECTION_TASK_INTERVAL   10000 // milliseconds
#define LOOKFORCARD_TASK_INTERVAL       100   // milliseconds
#define SYNC_CACHE_TASK_INTERVAL        600000 // milliseconds
#define MONITORDOORSENSOR_TASK_INTERVAL 500   // milliseconds
#define MONITOROUTPUT_TASK_INTERVAL     500   // milliseconds
#define CARD_DEBOUNCE_DELAY             2000  // milliseconds
#define PN532_READ_TIMEOUT              50   // milliseconds
#define CACHE_SIZE        32
#define CACHE_SYNC        240   // resync cache after <value> x 10 minutes

#define OUTPUT_ENABLE_DURATION          10000 // milliseconds

IPAddress ip(192,168,1,253);  //Node static IP
IPAddress gateway(192,168,1,254);
IPAddress subnet(255,255,255,0);

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
typedef uint8_t TOKEN[7];

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
// stored size = 9 bytes


/* ========================================================================== *
 *  Prototypes for task callbacks, etc
 * ========================================================================== */

// tasks
void keepWifiConnected();
void displayUptime();

/* ========================================================================== *
 *  Global Variables / Objects
 * ========================================================================== */

const char* ssid     = NETWORK_SSID;
const char* password = NETWORK_PASSWORD;
const char* host = SERVER_HOST;
const int hostPort = SERVER_PORT;

String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete


// tasks
Task WifiConnectionTask(WIFI_CONNECTION_TASK_INTERVAL, TASK_FOREVER, &keepWifiConnected);
Task displayUptimeTask(60000, TASK_FOREVER, &displayUptime);

// scheduler
Scheduler runner;



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

inline int max(int a,int b) {return ((a)>(b)?(a):(b)); }
inline int min(int a,int b) {return ((a)<(b)?(a):(b)); }

inline int clamp(int v, int minV, int maxV) {
  return min(maxV, max(v, minV));
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
     //Serial.println("Error: WiFi Not Connected");
     return TOKEN_ERROR;
   }

   //Serial.print("Querying server: ");
   //Serial.println(host);

   // Use WiFiClient class to create TCP connections
   WiFiClient client;
   if (!client.connect(host, hostPort)) {
     //Serial.println("Error: Connection failed");
     return TOKEN_ERROR;
   }

   // We now create a URI for the request
   String url = SERVER_URLPREFIX;
   url += "verify";
   url += "?token=";
   url += cardID;
   url += "&thing=";
   url += THING_ID;

   //Serial.print("Requesting URL: ");
   //Serial.println(url);

   // This will send the request to the server
   client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                "Host: " + host + "\r\n" +
                "Connection: close\r\n\r\n");
   int checkCounter = 0;
   while (!client.available() && checkCounter < 3000) {
     delay(10);
     checkCounter++;
   }

   // Read reply and decode json
   if (client.available()) {
     while(client.available()){
       // TODO: Replace with better/more robust streaming HTTP header parser
       String json;
       boolean endOfHeaders = false;
       while (!endOfHeaders && client.available()) {
         // feed the watchdog
         yield();
         json = client.readStringUntil('\n');
         //Serial.print("_");
         //Serial.print(json);
         if (json == "") endOfHeaders = true;
       }
       //Serial.println("json:");
       //Serial.println(json);
  
       StaticJsonBuffer<200> jsonBuffer;
  
       JsonObject& root = jsonBuffer.parseObject(json);
  
       // Test if parsing succeeds.
       if (!root.success()) {
         //Serial.println("Error: Couldn't parse JSON");
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
     
   } else {
    flags = TOKEN_ERROR;
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


// TODO: Fix this, has stopped working?!?
void sendTelegramMsg(String msg) {
   Serial.print("Sending msg to telegram");

   // check if connected
   if ( WiFi.status() != WL_CONNECTED ) {
    Serial.println("Error: WiFi Not Connected");
    return;
   }

   WiFiClient client;
   if (!client.connect(TELEGRAM_HOST, 80)) {
     Serial.println("Error: Connection failed");
     return;
   }

   String url = "/api/telegram/?groupid=-20679102&msg=";
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
 *  WiFi Client
 * ========================================================================== */

// Task to keep wifi connected
void keepWifiConnected() {

   if (WiFi.status() != WL_CONNECTED) {
      Serial.print("WiFi -> Connecting to: ");  Serial.println(ssid);

      WiFi.disconnect(true); // abandon any previous connection attempt
      WiFi.mode(WIFI_STA);  // force mode to STA only
      WiFi.begin(ssid, password);  // try again
      WiFi.config(ip, gateway, subnet);  // use static IP as more stable/faster than DHCP

      // check again to see if connection established...
      WifiConnectionTask.setInterval(WIFI_CONNECTION_TASK_INTERVAL);
   } else {
      //Serial.print("WiFi -> OK, IP: ");
      //Serial.println(WiFi.localIP());

      // sorted, shouldn't need to check again for a while
      WifiConnectionTask.setInterval(WIFI_CONNECTION_TASK_INTERVAL);
   }
}


/* ========================================================================== *
 *  Serial handling
 * ========================================================================== */

void handleSerial() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      stringComplete = true;
    } else {
      inputString += inChar;
    }
    yield();
  }
  
  if (stringComplete) {

    // validate query?
    if (inputString[0] == '?') {
      uint8_t v = queryServer(inputString.substring(1));
      Serial.print('?');
      Serial.println(v);
    } else if (inputString[0] == '!') {
      // log message
      sendLogMsg(inputString.substring(1));
    } else if (inputString[0] == '~') {
      // heartbeat request
      Serial.println('~');
    }
    
    // clear the string:
    inputString = "";
    stringComplete = false;
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

  // start serial
  Serial.begin(9600);

  Serial.setDebugOutput(true);

  Serial.println("");
  Serial.println("Door Controller WiFi");
  Serial.print("V:");  Serial.println(VERSION);

  // reserve 200 bytes for the inputString:
  inputString.reserve(200);

  // Setup scheduler
  Serial.println("Configuring tasks...");
  runner.init();
  runner.addTask(WifiConnectionTask);
  runner.addTask(displayUptimeTask);

  // Enable tasks
  WifiConnectionTask.enableDelayed(2000);
  displayUptimeTask.enable();

  Serial.println("Ready");
  Serial.println();
}


/* ========================================================================== *
 *  Main Loop
 * ========================================================================== */

void loop(void) {
  // execute tasks
  runner.execute();

  // keep track of uptime
  uptime(false);

  handleSerial();
  

  // TODO:
  // I've rebooted log msg
  // send heartbeat to server - 10min?
}
