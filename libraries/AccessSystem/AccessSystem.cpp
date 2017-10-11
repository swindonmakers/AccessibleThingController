#include "AccessSystem.h"
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

AccessSystem::AccessSystem(String thingId) :
    thingId(thingId)
{ }

// send a log msg to the server, fire and forget
void AccessSystem::sendLogMsg(String msg) 
{
    Serial.print(F("sendLogMsg: "));
 
    // check if connected
    if ( WiFi.status() != WL_CONNECTED ) {
     Serial.println(F("Error: WiFi Not Connected"));
     return;
    }
 
    WiFiClient client;
    if (!client.connect(ACCESS_SYSTEM_HOST, ACCESS_SYSTEM_PORT)) {
      Serial.println(F("Error: Connection failed"));
      return;
    }
 
    String url = ACCESS_SYSTEM_URLPREFIX;
    url += "msglog?thing=";
    url += thingId;
    url += "&msg=";
    url += urlencode(msg);
 
    Serial.print(url);
 
    // This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + ACCESS_SYSTEM_HOST + "\r\n" +
                 "Connection: close\r\n\r\n");
 
    // don't care if it succeeds, so close connection and return
    client.flush();
    client.stop();

    Serial.println(F(":ok"));
    return;
 }

 // query server for token, and return flags
uint8_t AccessSystem::getAccess(String cardID) 
{
    Serial.print("getAccess:");
    Serial.print(ACCESS_SYSTEM_HOST);
    Serial.print(":");
    Serial.print(ACCESS_SYSTEM_PORT);
    uint8_t flags = 0;
 
    // check if connected
    if ( WiFi.status() != WL_CONNECTED ) {
      Serial.println("Error: WiFi Not Connected");
      return TOKEN_ERROR;
    }
  
    // Use WiFiClient class to create TCP connections
    WiFiClient client;
    if (!client.connect(ACCESS_SYSTEM_HOST, ACCESS_SYSTEM_PORT)) {
      Serial.println("Error: Connection failed");
      return TOKEN_ERROR;
    }
 
    // We now create a URI for the request
    String url = ACCESS_SYSTEM_URLPREFIX;
    url += "verify";
    url += "?token=";
    url += cardID;
    url += "&thing=";
    url += thingId;
 
    Serial.println(url);
 
    // This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + ACCESS_SYSTEM_HOST + "\r\n" +
                 "Connection: close\r\n\r\n");
    int checkCounter = 0;
    while (!client.available() && checkCounter < ACCESS_SYSTEM_TIMEOUT) {
      delay(10);
      checkCounter++;
    }
 
    // Read reply and decode json
    if (client.available()) {
      while(client.available()){

        String json;
        boolean endOfHeaders = false;
        while (!endOfHeaders && client.available()) {
          yield();

          json = client.readStringUntil('\n');
          if (json == "") endOfHeaders = true;
        }
   
        Serial.println(json);

        StaticJsonBuffer<200> jsonBuffer;
   
        JsonObject& root = jsonBuffer.parseObject(json);
   
        // Test if parsing succeeds.
        if (!root.success()) {
          Serial.println("Error: Couldn't parse JSON");
          return TOKEN_ERROR;
        }
 
        if (!root.containsKey("access")) {
          Serial.println("Error: No access info");
          return TOKEN_ERROR;
        }
        
        // Check json response for access permission
        if (root["access"] == 1)
          flags |= TOKEN_ACCESS;
   
        if (root["trainer"] == 1)
          flags |= TOKEN_TRAINER;
      }
      
    } else {
     flags = TOKEN_ERROR;
    }
 
    // close connection
    client.stop();
 
    return flags;
 }