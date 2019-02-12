
/*
    Name:       ESP_MQTT_GarageDoor.ino
    Created:	12/02/2019 11:33:06 AM
    Author:     INTERNAL\BPerks
*/

// Define User Types below here or use a .h file
//
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>


/*Used for debugging purposes. Only use if needed*/
bool DEBUG = true; //Change to true to enable debugging

/************ WIFI and MQTT INFORMATION (CHANGE THESE FOR YOUR SETUP) ******************/
#define wifi_ssid            "WIFI_NAME"
#define wifi_password        "WIFI_PASSWORD"
#define mqtt_server          "MQTT_SERVER_ADDRESS"
#define mqtt_user            "MQTT_USERNAME"
#define mqtt_password        "MQTT_PASSWORD"
#define mqtt_port            1883

/*Would be best to use a unique device name as to not conflict with MQTT topics.*/
#define DEVICE_NAME				"RoomSensor123"
#define DEVICE_FRIENDLY_NAME	"Sensor"
#define OTApassword				"OTA_PASSWORD"


#define DEVICE_COMMAND_TOPIC "homeassistant/" DEVICE_NAME "/set"
#define DEVICE_DOOR_TOPIC "homeassistant/sensor/" DEVICE_NAME "/status"
#define DEVICE_RELAY_TOPIC "homeassistant/relay/" DEVICE_NAME "/set"


char* door_state = "UNDEFINED";
char* last_state = "";
long lastmsg = 0;


//define pins
#define DOOR_PIN
#define RELAY_PIN

WiFiClient espClient;
PubSubClient client(espClient);


// The setup() function runs once each time the micro-controller starts
void setup()
{
	Serial.begin(115200);
	Serial.print("Starting Node: " DEVICE_FRIENDLY_NAME);

	pinMode(RELAY_PIN, OUTPUT);
	pinMode(RELAY_PIN, LOW);
	pinMode(DOOR_PIN, INPUT);

	setup_wifi();

	client.setServer(mqtt_server, mqtt_port);
	client.setCallback(callback);

	ArduinoOTA.setPort(8266);
	ArduinoOTA.setHostname(DEVICE_FRIENDLY_NAME);
	ArduinoOTA.setPassword((const char *)OTApassword);

	ArduinoOTA.onStart([]() {
		Serial.println("Starting");
	});
	ArduinoOTA.onEnd([]() {
		Serial.println("\nEnd");
	});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
	});
	ArduinoOTA.onError([](ota_error_t error) {
		Serial.printf("Error[%u]: ", error);
		switch (error) {
		case OTA_AUTH_ERROR: Serial.println("Auth Failed"); break;
		case OTA_BEGIN_ERROR: Serial.println("Begin Failed"); break;
		case OTA_CONNECT_ERROR: Serial.println("Connect Failed"); break;
		case OTA_RECEIVE_ERROR: Serial.println("Receive Failed"); break;
		case OTA_END_ERROR: Serial.println("End Failed"); break;
		default: Serial.println("Unknown Error"); break;
		}
	});
	ArduinoOTA.begin();
	Serial.println("Ready");


}



// Add the main program code into the continuous loop() function
void loop()
{
	ArduinoOTA.handle();

	if (!client.connected()) {
		reconnect();
	}
	client.loop();

}

void setup_wifi() {
	delay(10);
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(wifi_ssid);

	WiFi.mode(WIFI_STA);
	WiFi.hostname(DEVICE_FRIENDLY_NAME);
	WiFi.begin(wifi_ssid, wifi_password);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}

	Serial.println("");
	Serial.println("WiFi connected");
	Serial.println("IP address: ");
	Serial.println(WiFi.localIP());
}

void reconnect() {
	// Loop until we're reconnected
	int failedCnt = 0;
	while (!client.connected()) {
		Serial.print("Attempting MQTT connection...");
		// Attempt to connect
		if (client.connect(DEVICE_FRIENDLY_NAME, mqtt_user, mqtt_password)) {
			Serial.println("connected");
			//client.subscribe(light_set_topic);
			client.subscribe(DEVICE_COMMAND_TOPIC);
			client.subscribe(DEVICE_RELAY_TOPIC);
		}
		else {
			Serial.print("failed, rc=");
			Serial.print(client.state());
			Serial.println(" try again in 5 seconds");
			failedCnt++;
			if (failedCnt > 20) { // if failed to connect more than 20 times reset device to try and fix
				Serial.println("Restarting device");
				ESP.restart();
			}
			// Wait 5 seconds before retrying
			delay(5000);
		}
	}
}

void checkDoorState() {
	//Checks if the door state has changed, and MQTT pub the change
	last_state = door_state; //get previous state of door
	if (digitalRead(DOOR_PIN) == 0) // get new state of door
		door_state = "OPENED";
	else if (digitalRead(DOOR_PIN) == 1)
		door_state = "CLOSED";

	if (last_state != door_state) { // if the state has changed then publish the change
		client.publish(DEVICE_DOOR_TOPIC, door_state);
		Serial.println(door_state);
	}
	//pub every minute, regardless of a change.
	long now = millis();
	if (now - lastmsg > 10000) {
		lastmsg = now;
		client.publish(DEVICE_DOOR_TOPIC, door_state);
	}
}

void callback(char* topic, byte* payload, unsigned int length) {
	//if the 'garage/button' topic has a payload "OPEN", then 'click' the relay
	payload[length] = '\0';
	strTopic = String((char*)topic);
	if (strTopic == button_topic)
	{
		switch1 = String((char*)payload);
		if (switch1 == "OPEN")
		{
			//'click' the relay
			Serial.println("ON");
			pinMode(RELAY_PIN, HIGH);
			delay(600);
			pinMode(RELAY_PIN, LOW);
		}
	}
}