/*
    Name:       blindmotor.ino
    Created:	24/01/2019 9:24:37 AM
    Author:     INTERNAL\BPerks
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Stepper_28BYJ_48.h>
#include <ArduinoJson/ArduinoJson.h>
#include <NidayandHelper.h>
#include <FS.h>


/***************************** PARAMETERS *****************************/

// Version number for checking if there are new code releases and notifying the user
String version = "1.0";

/*Used for debugging purposes. Only use if needed*/
bool DEBUG = true;		//Change to true to enable debugging

/*Wifi*/
#define wifi_ssid				"WiFi_NAME"
#define wifi_password			"WIFI_PASSWORD"
#define mqtt_server				"MQTT_SERVER_ADDRESS"
#define mqtt_port				1883
#define mqtt_user				"MQTT_USERNAME"
#define mqtt_password			"MQTT_PASSWORD"

/*Would be best to use a unique device name as to not conflict with MQTT topics*/

#define DEVICE_NAME				"MQTT_Blind_Motorxxx"
#define DEVICE_FRIENDLY_NAME	"Motorxxx"

// Sensor name is used for OTA and WiFi hostnames, can be different from DEVICE_NAME if you want
// to have a difference between your MQT topic names and the OTA/WiFi hostnames
#define SENSORNAME DEVICE_NAME

#define OTApassword "OTA_PASSWORD"

// Define topics for MQTT
#define MQTT_RESET_CMD			"reset"  //sending this command will reset the device
#define MQTT_FACTORY_CMD		"factoryreset"  //sending this command will wipe saved config

#define DEVICE_COMMAND_TOPIC	"homeassistant/" DEVICE_NAME "/set"
#define DEVICE_LISTENER_TOPIC	"homeassistant/" DEVICE_NAME "/listener"
#define DEVICE_MONITOR_TOPIC	"homeassistant/" DEVICE_NAME "/monitor"


//Stored data
long currentPosition = 0;           //Current position of the blind
long maxPosition = 20000;         //Max position of the blind
//boolean loadDataSuccess = false;
boolean saveItNow = false;          //If true will store positions to SPIFFS

int path = 0;                       //Direction of blind (1 = down, 0 = stop, -1 = up)
int setPos = 0;                     //The set position 0-100% by the client
bool shouldSaveConfig = false;      //Used for WIFI Manager callback to save parameters
boolean initLoop = true;            //To enable actions first time the loop is run
boolean ccw = true;                 //Turns counter clockwise to lower the curtain
String action;



WiFiClient espClient;
PubSubClient psclient(espClient);
Stepper_28BYJ_48 small_stepper(D1, D3, D2, D4);
NidayandHelper helper = NidayandHelper();

// The setup() function runs once each time the micro-controller starts
void setup()
{
	Serial.begin(115200);
	Serial.println("Starting Node: " SENSORNAME);

	setup_wifi();
	psclient.setServer(mqtt_server, mqtt_port);
	psclient.setCallback(callback);
	//Load config upon start
	if (!SPIFFS.begin()) {
		Serial.println("Failed to mount file system");
		return;
	}



}

// Add the main program code into the continuous loop() function
void loop() {
	ArduinoOTA.handle();

	if (!psclient.connected()) {
		reconnect();
	}
	psclient.loop();

	//reset action
	action = "";

	/**
	  Manage actions. Steering of the blind
	*/
	if (action == "auto") {
		/*
		   Automatically open or close blind
		*/
		if (currentPosition > path) {
			small_stepper.step(ccw ? -1 : 1);
			currentPosition = currentPosition - 1;
		}
		else if (currentPosition < path) {
			small_stepper.step(ccw ? 1 : -1);
			currentPosition = currentPosition + 1;
		}
		else {
			path = 0;
			action = "";
			int set = (setPos * 100) / maxPosition;
			int pos = (currentPosition * 100) / maxPosition;
			sendmsg(DEVICE_MONITOR_TOPIC, String(pos) );
			Serial.println("Stopped. Reached wanted position");
			saveItNow = true;
		}

	}
	else if (action == "manual" && path != 0) {
		/*
		   Manually running the blind
		*/
		small_stepper.step(ccw ? path : -path);
		currentPosition = currentPosition + path;
	}
	delay(100);
}



void setup_wifi() {
	delay(10);
	Serial.println();
	Serial.print("Connecting to ");
	Serial.println(wifi_ssid);

	WiFi.mode(WIFI_STA);
	WiFi.hostname(SENSORNAME);
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
	while (!psclient.connected()) {
		Serial.print("Attempting MQTT connection...");
		// Attempt to connect
		if (psclient.connect(SENSORNAME, mqtt_user, mqtt_password)) {
			Serial.println("connected");
			psclient.subscribe(DEVICE_LISTENER_TOPIC);
			psclient.subscribe(DEVICE_COMMAND_TOPIC);			
		}
		else {
			Serial.print("failed, rc=");
			Serial.print(psclient.state());
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

void stopPowerToCoils() {
	digitalWrite(D1, LOW);
	digitalWrite(D2, LOW);
	digitalWrite(D3, LOW);
	digitalWrite(D4, LOW);
}

bool loadConfig() {
	if (!helper.loadconfig()) {
		return false;
	}
	JsonVariant json = helper.getconfig();

	//Store variables locally
	currentPosition = long(json["currentPosition"]);
	maxPosition = long(json["maxPosition"]);

	return true;
}

/**
   Save configuration data to a JSON file
   on SPIFFS
*/
bool saveConfig() {
	StaticJsonBuffer<200> jsonBuffer;
	JsonObject& json = jsonBuffer.createObject();
	json["currentPosition"] = currentPosition;
	json["maxPosition"] = maxPosition;

	return helper.saveconfig(json);
}


void callback(char* topic, byte* payload, unsigned int length) {
	Serial.print("Message arrived [");
	Serial.print(topic);
	Serial.print("] ");
	String msg = "";

	char message[length + 1];
	for (int i = 0; i < length; i++) {
		message[i] = (char)payload[i];
		msg += String((char)payload[i]);
	}
	message[length] = '\0';
	Serial.println(message);


	if (strcmp(topic, DEVICE_COMMAND_TOPIC) == 0) {
		if (strcmp(message, MQTT_RESET_CMD) == 0) {
			Serial.println("Restarting device");
			ESP.restart();
		}
		else if (strcmp(message, MQTT_FACTORY_CMD) == 0) {
			Serial.println("Wipeing storage and restarting");
			SPIFFS.format();
			ESP.restart();
		}
		else if (strcmp(message, DEVICE_LISTENER_TOPIC) == 0) {
			processMsg(msg, NULL);

		}


	}
}



void processMsg(String res, uint8_t clientnum) {

	if (res == "(0)") {
		/*
		   Stop
		*/
		path = 0;
		saveItNow = true;
		action = "manual";
	}
	else if (res == "(1)") {
		/*
		   Move down without limit to max position
		*/
		path = 1;
		action = "manual";
	}
	else if (res == "(-1)") {
		/*
		   Move up without limit to top position
		*/
		path = -1;
		action = "manual";

	}
	else if (res == "(update)") {
		//Send position details to client
		int set = (setPos * 100) / maxPosition;
		int pos = (currentPosition * 100) / maxPosition;
		sendmsg(DEVICE_MONITOR_TOPIC, String(pos));

	}
	else if (res == "(start)") {
		/*
	   Store the current position as the start position
	*/
		currentPosition = 0;
		path = 0;
		saveItNow = true;
		action = "manual";
	}
	else {
		/*
		   Any other message will take the blind to a position
		   Incoming value = 0-100
		   path is now the position
		*/
		path = maxPosition * res.toInt() / 100;
		setPos = path; //Copy path for responding to updates
		action = "auto";

		int set = (setPos * 100) / maxPosition;
		int pos = (currentPosition * 100) / maxPosition;

		//Send the instruction to all connected devices
		sendmsg(DEVICE_MONITOR_TOPIC, String(pos));
	}
}

/*
   Connect to MQTT server and publish a message on the bus.
   Finally, close down the connection and radio
*/
void sendmsg(String topic, String payload) {

	helper.mqtt_publish(psclient, topic, payload);
}
