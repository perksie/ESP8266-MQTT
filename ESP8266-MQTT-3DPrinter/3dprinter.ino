// Visual Micro is in vMicro>General>Tutorial Mode
// 
/*
    Name:       3dprinter.ino
    Created:	11/06/2019 1:54:43 PM
    Author:     INTERNAL\BPerks
*/



#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "FastLED.h"
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>.
#include <WiFiUdp.h>



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
#define DEVICE_NAME				"3dprinter"
#define DEVICE_FRIENDLY_NAME	"3dSensor"
#define OTApassword				"OTA_PASSWORD"


/*********************************** FastLED Defintions ********************************/
#define NUM_LEDS    60

#define DATA_PIN    4
//#define CLOCK_PIN 5
#define CHIPSET     WS2812
#define COLOR_ORDER GRB

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

String setColor = "0,0,150";
String setPower;
String setEffect = "Solid";
String setBrightness = "150";
int brightness = 150;
String setAnimationSpeed;
int animationspeed = 240;
String setColorTemp;
int Rcolor = 0;
int Gcolor = 0;
int Bcolor = 0;
CRGB leds[NUM_LEDS];



#define colorstatuspub "homeassistant/3dprinter/colorstatus"
#define setcolorsub "homeassistant/3dprinter/setcolor"
#define setpowersub "homeassistant/3dprinter/setpower"
#define seteffectsub "homeassistant/3dprinter/seteffect"
#define setbrightness "homeassistant/3dprinter/setbrightness"

#define setcolorpub "homeassistant/3dprinter/setcolorpub"
#define setpowerpub "homeassistant/3dprinter/setpowerpub"
#define seteffectpub "homeassistant/3dprinter/seteffectpub"
#define setbrightnesspub "homeassistant/3dprinter/setbrightnesspub"
#define setanimationspeed "homeassistant/3dprinter/setanimationspeed"



/****************FOR CANDY CANE***************/
CRGBPalette16 currentPalettestriped; //for Candy Cane
CRGBPalette16 gPal; //for fire

/****************FOR NOISE***************/
static uint16_t dist;         // A random number for our noise generator.
uint16_t scale = 30;          // Wouldn't recommend changing this on the fly, or the animation will be really blocky.
uint8_t maxChanges = 48;      // Value for blending between palettes.
CRGBPalette16 targetPalette(OceanColors_p);
CRGBPalette16 currentPalette(CRGB::Black);

/*****************For TWINKER********/
#define DENSITY     80
int twinklecounter = 0;

/*********FOR RIPPLE***********/
uint8_t colour;                                               // Ripple colour is randomized.
int center = 0;                                               // Center of the current ripple.
int step = -1;                                                // -1 is the initializing step.
uint8_t myfade = 255;                                         // Starting brightness.
#define maxsteps 16                                           // Case statement wouldn't allow a variable.
uint8_t bgcol = 0;                                            // Background colour rotates.
int thisdelay = 20;                                           // Standard delay value.

/**************FOR RAINBOW***********/
uint8_t thishue = 0;                                          // Starting hue value.
uint8_t deltahue = 10;

/**************FOR DOTS**************/
uint8_t   count = 0;                                        // Count up to 255 and then reverts to 0
uint8_t fadeval = 224;                                        // Trail behind the LED's. Lower => faster fade.
uint8_t bpm = 30;

/**************FOR LIGHTNING**************/
uint8_t frequency = 50;                                       // controls the interval between strikes
uint8_t flashes = 8;                                          //the upper limit of flashes per strike
unsigned int dimmer = 1;
uint8_t ledstart;                                             // Starting location of a flash
uint8_t ledlen;
int lightningcounter = 0;

/********FOR FUNKBOX EFFECTS**********/
int idex = 0;                //-LED INDEX (0 to NUM_LEDS-1
int TOP_INDEX = int(NUM_LEDS / 2);
int thissat = 255;           //-FX LOOPS DELAY VAR
uint8_t thishuepolice = 0;
int antipodal_index(int i) {
	int iN = i + TOP_INDEX;
	if (i >= TOP_INDEX) {
		iN = (i + TOP_INDEX) % NUM_LEDS;
	}
	return iN;
}

/********FIRE**********/
#define COOLING  55
#define SPARKING 120
bool gReverseDirection = false;

/********BPM**********/
uint8_t gHue = 0;
char message_buff[100];


WiFiClient espClient;
PubSubClient client(espClient);

void setup() {

	Serial.begin(115200);
	Serial.print("Starting Node: " DEVICE_FRIENDLY_NAME);
	Serial.print("Initialising LEDs...");

	FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
	FastLED.setMaxPowerInVoltsAndMilliamps(12, 10000); //experimental for power management. Feel free to try in your own setup.
	FastLED.setBrightness(brightness);

	setupStripedPalette(CRGB::Red, CRGB::Red, CRGB::White, CRGB::White); //for CANDY CANE

	gPal = HeatColors_p; //for FIRE

	fill_solid(leds, NUM_LEDS, CRGB(255, 0, 0)); //Startup LED Lights
	FastLED.show();

	setup_wifi();

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

void loop() {

	ArduinoOTA.handle();

	if (!client.connected()) {
		reconnect();
	}
	client.loop();


	int Rcolor = setColor.substring(0, setColor.indexOf(',')).toInt();
	int Gcolor = setColor.substring(setColor.indexOf(',') + 1, setColor.lastIndexOf(',')).toInt();
	int Bcolor = setColor.substring(setColor.lastIndexOf(',') + 1).toInt();

	if (setPower == "OFF") {
		setEffect = "Solid";
		for (int i = 0; i < NUM_LEDS; i++) {
			leds[i].fadeToBlackBy(8);   //FADE OFF LEDS
		}
	}

	if (setEffect == "Sinelon") {
		fadeToBlackBy(leds, NUM_LEDS, 20);
		int pos = beatsin16(13, 0, NUM_LEDS);
		leds[pos] += CRGB(Rcolor, Gcolor, Bcolor);
	}

	if (setEffect == "Juggle") {                           // eight colored dots, weaving in and out of sync with each other
		fadeToBlackBy(leds, NUM_LEDS, 20);
		byte dothue = 0;
		for (int i = 0; i < 8; i++) {
			leds[beatsin16(i + 7, 0, NUM_LEDS)] |= CRGB(Rcolor, Gcolor, Bcolor);
			dothue += 32;
		}
	}

	if (setEffect == "Confetti") {                       // random colored speckles that blink in and fade smoothly
		fadeToBlackBy(leds, NUM_LEDS, 10);
		int pos = random16(NUM_LEDS);
		leds[pos] += CRGB(Rcolor + random8(64), Gcolor, Bcolor);
	}


	if (setEffect == "Rainbow") {
		// FastLED's built-in rainbow generator
		static uint8_t starthue = 0;    thishue++;
		fill_rainbow(leds, NUM_LEDS, thishue, deltahue);
	}


	if (setEffect == "Rainbow with Glitter") {               // FastLED's built-in rainbow generator with Glitter
		static uint8_t starthue = 0;
		thishue++;
		fill_rainbow(leds, NUM_LEDS, thishue, deltahue);
		addGlitter(80);
	}


	if (setEffect == "Glitter") {
		fadeToBlackBy(leds, NUM_LEDS, 20);
		addGlitterColor(80, Rcolor, Gcolor, Bcolor);
	}


	if (setEffect == "BPM") {                                  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
		uint8_t BeatsPerMinute = 62;
		CRGBPalette16 palette = PartyColors_p;
		uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
		for (int i = 0; i < NUM_LEDS; i++) { //9948
			leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
		}
	}

	if (setEffect == "Solid" & setPower == "ON") {          //Fill entire strand with solid color
		fill_solid(leds, NUM_LEDS, CRGB(Rcolor, Gcolor, Bcolor));
	}



	if (setEffect == "Twinkle") {
		twinklecounter = twinklecounter + 1;
		if (twinklecounter < 2) {                               //Resets strip if previous animation was running
			FastLED.clear();
			FastLED.show();
		}
		const CRGB lightcolor(8, 7, 1);
		for (int i = 0; i < NUM_LEDS; i++) {
			if (!leds[i]) continue; // skip black pixels
			if (leds[i].r & 1) { // is red odd?
				leds[i] -= lightcolor; // darken if red is odd
			}
			else {
				leds[i] += lightcolor; // brighten if red is even
			}
		}
		if (random8() < DENSITY) {
			int j = random16(NUM_LEDS);
			if (!leds[j]) leds[j] = lightcolor;
		}
	}

	if (setEffect == "Dots") {
		uint8_t inner = beatsin8(bpm, NUM_LEDS / 4, NUM_LEDS / 4 * 3);
		uint8_t outer = beatsin8(bpm, 0, NUM_LEDS - 1);
		uint8_t middle = beatsin8(bpm, NUM_LEDS / 3, NUM_LEDS / 3 * 2);
		leds[middle] = CRGB::Purple;
		leds[inner] = CRGB::Blue;
		leds[outer] = CRGB::Aqua;
		nscale8(leds, NUM_LEDS, fadeval);
	}

	if (setEffect == "Lightning") {
		twinklecounter = twinklecounter + 1;                     //Resets strip if previous animation was running
		Serial.println(twinklecounter);
		if (twinklecounter < 2) {
			FastLED.clear();
			FastLED.show();
		}
		ledstart = random8(NUM_LEDS);           // Determine starting location of flash
		ledlen = random8(NUM_LEDS - ledstart);  // Determine length of flash (not to go beyond NUM_LEDS-1)
		for (int flashCounter = 0; flashCounter < random8(3, flashes); flashCounter++) {
			if (flashCounter == 0) dimmer = 5;    // the brightness of the leader is scaled down by a factor of 5
			else dimmer = random8(1, 3);          // return strokes are brighter than the leader
			fill_solid(leds + ledstart, ledlen, CHSV(255, 0, 255 / dimmer));
			FastLED.show();                       // Show a section of LED's
			delay(random8(4, 10));                // each flash only lasts 4-10 milliseconds
			fill_solid(leds + ledstart, ledlen, CHSV(255, 0, 0)); // Clear the section of LED's
			FastLED.show();
			if (flashCounter == 0) delay(150);   // longer delay until next flash after the leader
			delay(50 + random8(100));             // shorter delay between strokes
		}
		delay(random8(frequency) * 100);        // delay between strikes
	}



	if (setEffect == "Police One") {                    //POLICE LIGHTS (TWO COLOR SINGLE LED)
		idex++;
		if (idex >= NUM_LEDS) {
			idex = 0;
		}
		int idexR = idex;
		int idexB = antipodal_index(idexR);
		int thathue = (thishuepolice + 160) % 255;
		for (int i = 0; i < NUM_LEDS; i++) {
			if (i == idexR) {
				leds[i] = CHSV(thishuepolice, thissat, 255);
			}
			else if (i == idexB) {
				leds[i] = CHSV(thathue, thissat, 255);
			}
			else {
				leds[i] = CHSV(0, 0, 0);
			}
		}

	}

	if (setEffect == "Police All") {                 //POLICE LIGHTS (TWO COLOR SOLID)
		idex++;
		if (idex >= NUM_LEDS) {
			idex = 0;
		}
		int idexR = idex;
		int idexB = antipodal_index(idexR);
		int thathue = (thishuepolice + 160) % 255;
		leds[idexR] = CHSV(thishuepolice, thissat, 255);
		leds[idexB] = CHSV(thathue, thissat, 255);
	}


	if (setEffect == "Candy Cane") {
		static uint8_t startIndex = 0;
		startIndex = startIndex + 1; /* higher = faster motion */

		fill_palette(leds, NUM_LEDS,
			startIndex, 16, /* higher = narrower stripes */
			currentPalettestriped, 255, LINEARBLEND);
	}


	if (setEffect == "Cyclon Rainbow") {                    //Single Dot Down
		static uint8_t hue = 0;
		Serial.print("x");
		// First slide the led in one direction
		for (int i = 0; i < NUM_LEDS; i++) {
			// Set the i'th led to red 
			leds[i] = CHSV(hue++, 255, 255);
			// Show the leds
			FastLED.show();
			// now that we've shown the leds, reset the i'th led to black
			// leds[i] = CRGB::Black;
			fadeall();
			// Wait a little bit before we loop around and do it again
			delay(10);
		}
		for (int i = (NUM_LEDS)-1; i >= 0; i--) {
			// Set the i'th led to red 
			leds[i] = CHSV(hue++, 255, 255);
			// Show the leds
			FastLED.show();
			// now that we've shown the leds, reset the i'th led to black
			// leds[i] = CRGB::Black;
			fadeall();
			// Wait a little bit before we loop around and do it again
			delay(10);
		}
	}

	if (setEffect == "Fire") {
		Fire2012WithPalette();
	}
	random16_add_entropy(random8());




	EVERY_N_MILLISECONDS(10) {
		nblendPaletteTowardPalette(currentPalette, targetPalette, maxChanges);  // FOR NOISE ANIMATION
		{ gHue++; }


		if (setEffect == "Noise") {
			setPower = "ON";
			for (int i = 0; i < NUM_LEDS; i++) {                                     // Just ONE loop to fill up the LED array as all of the pixels change.
				uint8_t index = inoise8(i * scale, dist + i * scale) % 255;            // Get a value from the noise function. I'm using both x and y axis.
				leds[i] = ColorFromPalette(currentPalette, index, 255, LINEARBLEND);   // With that value, look up the 8 bit colour palette value and assign it to the current LED.
			}
			dist += beatsin8(10, 1, 4);                                              // Moving along the distance (that random number we started out with). Vary it a bit with a sine wave.
			// In some sketches, I've used millis() instead of an incremented counter. Works a treat.
		}


		if (setEffect == "Ripple") {
			for (int i = 0; i < NUM_LEDS; i++) leds[i] = CHSV(bgcol++, 255, 15);  // Rotate background colour.
			switch (step) {
			case -1:                                                          // Initialize ripple variables.
				center = random(NUM_LEDS);
				colour = random8();
				step = 0;
				break;
			case 0:
				leds[center] = CHSV(colour, 255, 255);                          // Display the first pixel of the ripple.
				step++;
				break;
			case maxsteps:                                                    // At the end of the ripples.
				step = -1;
				break;
			default:                                                             // Middle of the ripples.
				leds[(center + step + NUM_LEDS) % NUM_LEDS] += CHSV(colour, 255, myfade / step * 2);   // Simple wrap from Marc Miller
				leds[(center - step + NUM_LEDS) % NUM_LEDS] += CHSV(colour, 255, myfade / step * 2);
				step++;                                                         // Next step.
				break;
			}
		}


	}

	EVERY_N_SECONDS(5) {
		targetPalette = CRGBPalette16(CHSV(random8(), 255, random8(128, 255)), CHSV(random8(), 255, random8(128, 255)), CHSV(random8(), 192, random8(128, 255)), CHSV(random8(), 255, random8(128, 255)));
	}

	FastLED.setBrightness(brightness);  //EXECUTE EFFECT COLOR
	FastLED.show();

	if (animationspeed > 0 && animationspeed < 150) {  //Sets animation speed based on receieved value
		FastLED.delay(1000 / animationspeed);
	}
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
	while (!client.connected()) {
		Serial.print("Attempting MQTT connection...");
		// Attempt to connect
		if (client.connect("3D Printer LEDs", mqtt_user, mqtt_password)) {
			Serial.println("connected");

			FastLED.clear(); //Turns off startup LEDs after connection is made
			FastLED.show();

			client.subscribe(setcolorsub);
			client.subscribe(setbrightness);
			//client.subscribe(setcolortemp);
			client.subscribe(setpowersub);
			client.subscribe(seteffectsub);
			client.subscribe(setanimationspeed);
			client.publish(setpowerpub, "OFF");
		}
		else {
			Serial.print("failed, rc=");
			Serial.print(client.state());
			Serial.println(" try again in 5 seconds");
			// Wait 5 seconds before retrying
			delay(5000);
		}
	}
}

void addGlitter(fract8 chanceOfGlitter)
{
	if (random8() < chanceOfGlitter) {
		leds[random16(NUM_LEDS)] += CRGB::White;
	}
}

void addGlitterColor(fract8 chanceOfGlitter, int Rcolor, int Gcolor, int Bcolor)
{
	if (random8() < chanceOfGlitter) {
		leds[random16(NUM_LEDS)] += CRGB(Rcolor, Gcolor, Bcolor);
	}
}