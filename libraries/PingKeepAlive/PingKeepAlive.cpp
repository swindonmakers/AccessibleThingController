#include "PingKeepAlive.h"

void PingKeepAlive::loop() {

	// Ping check
	if (WiFi.status() == WL_CONNECTED && millis() - lastPingTime > PING_CHECK_FREQ) {
		lastPingTime = millis();
		bool ret = Ping.ping(WiFi.gatewayIP(), 1);
		if (ret) {
			pingFailCount = 0;
			isConnected = true;
		}
		else {
			pingFailCount++;
		}
	}

	// Try to keep wifi connected.
	if (!reconnecting && WiFi.status() != WL_IDLE_STATUS) {
		if (pingFailCount > TOO_MANY_FAILED_PINGS || WiFi.status() != WL_CONNECTED) {
			// We've just disconnected or a reconnect attempt failed
			// so try to connect again
			reconnecting = true;
			isConnected = false;
			disconnectCount++;
			pingFailCount = 0;

			if (disconnectFunction != NULL)
				disconnectFunction();

      reconnect();
		}
	}
  else if (reconnecting && millis() - lastReconnectAttempt > RECONNECT_TIMEOUT) {
    // it's taking too long to reconnect, try again
    reconnect();
  }
	else if (WiFi.status() == WL_CONNECTED && reconnecting) {
		// We've just reconnected
		reconnectCount++;
		isConnected = true;
		if (reconnectFunction != NULL) reconnectFunction();
		reconnecting = false;
	}

  yield();
}

void PingKeepAlive::reconnect()
{
  WiFi.mode(WIFI_OFF);
  WiFi.mode(WIFI_STA);
  WiFi.begin();

  lastReconnectAttempt = millis();
}

void PingKeepAlive::onDisconnect(Callback fn)
{
	disconnectFunction = fn;
}

void PingKeepAlive::onReconnect(Callback fn)
{
	reconnectFunction = fn;
}