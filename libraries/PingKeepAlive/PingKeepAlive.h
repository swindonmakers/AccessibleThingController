#ifndef PingKeepAlive_h
#define PingKeepAlive_h

#include <Arduino.h>
#include <ESP8266Ping.h>

#ifndef TOO_MANY_FAILED_PINGS
#define TOO_MANY_FAILED_PINGS 5
#endif

#ifndef PING_CHECK_FREQ
#define PING_CHECK_FREQ 10000
#endif

#ifndef RECONNECT_TIMEOUT
#define RECONNECT_TIMEOUT 60000
#endif

class PingKeepAlive
{
public:
	typedef void(*Callback) ();
	void loop();

	int disconnectCount = 0; // number of times a disconnect fired
	int reconnectCount = 0; // number of times we successfully reconnected
	bool isConnected = false;

	void onDisconnect(Callback fn);
	void onReconnect(Callback fn);

private:
	unsigned long lastPingTime = 0; // last time we pinged the gateway
	unsigned int pingFailCount = 0; // how many times in a row the ping has failed
	bool reconnecting = false; // will be true if the WiFi connection has dropped and we are trying to reconnect
	unsigned long lastReconnectAttempt = 0; // last time we tried to reconnect

	Callback disconnectFunction = NULL;
	Callback reconnectFunction = NULL;

  void reconnect();
};

#endif