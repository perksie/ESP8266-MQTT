#ifndef NidayandHelper_h
#define NidayandHelper_h

#include "Arduino.h"
#include <ArduinoJson.h>
#include "FS.h"
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <list>

class NidayandHelper {
public:
	NidayandHelper();
	boolean loadconfig();
	JsonVariant getconfig();
	boolean saveconfig(JsonVariant json);

	void mqtt_reconnect(PubSubClient& psclient, std::list<const char*> topics);
	void mqtt_reconnect(PubSubClient& psclient, String uid, String pwd);
	void mqtt_reconnect(PubSubClient& psclient, String uid, String pwd, std::list<const char*> topics);

	void mqtt_publish(PubSubClient& psclient, String topic, String payload);


private:
	JsonVariant _config;
	String _configfile;
	String _mqttclientid;
};

#endif