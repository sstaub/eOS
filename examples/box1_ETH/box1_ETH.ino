/*
Copyright (c) 2017 Electronic Theatre Controls, Inc., http://www.etcconnect.com
Copyright (c) 2020 Stefan Staub

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

// put 100nF ceramic capitors between ground and the input of the buttons and encoders

// libraries included

#include "Arduino.h"
#include <LiquidCrystal.h>
#include <OSCMessage.h>
#include "eOS.h"
#include "Ethernet3.h"
#include "EthernetUdp3.h"


// Hardware pins
#define LCD_RS				2
#define LCD_ENABLE		3
#define LCD_D4				4
#define LCD_D5				5
#define LCD_D6				6
#define LCD_D7				7

#define ENC_1_A				A0
#define ENC_1_B				A1
#define ENC_2_A				A2
#define ENC_2_B				A3

#define NEXT_BTN			A4
#define LAST_BTN			A5
#define SHIFT_BTN			8

int LED_Pin=13

// constants and macros
#define LCD_CHARS			16
#define LCD_LINES			2

#define SIG_DIGITS		3 // Number of significant digits displayed

const String HANDSHAKE_QUERY = "ETCOSC?";
const String HANDSHAKE_REPLY = "OK";
const String PING_QUERY = "box1_eth_hello";
const String PARAMETER_QUERY = "/eos/out/param/";

// See displayScreen() below - limited to 10 chars (after 6 prefix chars)
const String VERSION_STRING = "1.0.0.0";

// Change these values to alter how long we wait before sending an OSC ping
// to see if Eos is still there, and then finally how long before we
// disconnect and show the splash screen
// Values are in milliseconds
#define PING_AFTER_IDLE_INTERVAL		2500
#define TIMEOUT_AFTER_IDLE_INTERVAL	5000

// Definition of the parameter you want to use
const String ENCODER_1_PARAMETER = "Pan";
const String ENCODER_2_PARAMETER = "Tilt";

// Network config
uint8_t mac[] = {0x90, 0xA2, 0xDA, 0x10, 0x14, 0x48};
IPAddress localIP(10, 101, 1, 201);
IPAddress subnet(255, 255, 0, 0);
IPAddress eosIP(10, 101, 1, 100);
uint16_t eosPort = 8000;
uint16_t localPort = 8001;

// Global variables
bool updateDisplay = false;
bool connectedToEos = false;
unsigned long lastMessageRxTime = 0;
bool timeoutPingSent = false;

struct Parameter {
	String name;
	String displayName;
	float value;
	} enc1, enc2;

// Hardware constructors
EthernetUDP udp;
EOS eos(udp, eosIP, eosPort);

LiquidCrystal lcd(LCD_RS, LCD_ENABLE, LCD_D4, LCD_D5, LCD_D6, LCD_D7); // rs, enable, d4, d5, d6, d7

Key next(NEXT_BTN, "NEXT");
Key last(LAST_BTN, "LAST");
Encoder encoder1(ENC_1_A, ENC_1_B, FORWARD);
Encoder encoder2(ENC_2_A, ENC_2_B, FORWARD);

// Local functions

/**
 * @brief Init the console, gives back a handshake reply
 * and send the filters and subscribtions.
 *
 */
void initEOS() {
	filter("/eos/out/param/*");
	filter("/eos/out/ping");
	subscribe(ENCODER_1_PARAMETER);
	subscribe(ENCODER_2_PARAMETER);
	}

/**
 * @brief 
 * Given a valid OSCMessage (relevant to Pan/Tilt), we update our Encoder struct
 * with the new valueition information.
 * 
 * @param msg - the OSC message we will use to update our internal data
 * @param addressOffset - unused (allows for multiple nested roots)
 */
void parseEnc1Update(OSCMessage& msg, int addressOffset) {
	enc1.value = msg.getOSCData(0)->getFloat();
	updateDisplay = true; 
	}

void parseEnc2Update(OSCMessage& msg, int addressOffset) {
	enc2.value = msg.getOSCData(0)->getFloat();
	updateDisplay = true;
	}

/**
 * @brief 
 * Given an unknown OSC message we check to see if it's a handshake message.
 * If it's a handshake we issue a subscribe, otherwise we begin route the OSC
 * message to the appropriate function.
 * 
 * @param msg - the OSC message of unknown importance
 *
 */
void parseOSCMessage(String& msg) {
	// Prepare the message for routing by filling an OSCMessage object with our message string
	OSCMessage oscmsg;
	oscmsg.fill((uint8_t*)msg.c_str(), (int)msg.length());
	// Route parameter messages to the relevant update function
	oscmsg.route((PARAMETER_QUERY + ENCODER_1_PARAMETER).c_str(), parseEnc1Update);
	oscmsg.route((PARAMETER_QUERY + ENCODER_2_PARAMETER).c_str(), parseEnc2Update);
	connectedToEos = true;
	}

/**
 * @brief 
 * Updates the display with the latest parameter values.
 * 
 */
void displayStatus() {
	lcd.clear();

	if (!connectedToEos) {
		// display a splash message before the Eos connection is open
		lcd.setCursor(0, 0);
		lcd.print(String("Box1ETH v" + VERSION_STRING).c_str());
		lcd.setCursor(0, 1);
		lcd.print("waiting for EOS");
		digitalWrite(LED_Pin, LOW);
		}
	else {
		lcd.setCursor(0, 0);
		lcd.print(ENCODER_1_PARAMETER);
		lcd.setCursor(8, 0);
		lcd.print(ENCODER_2_PARAMETER);
		lcd.setCursor(0, 1);
		lcd.print(enc1.value, SIG_DIGITS);
		lcd.setCursor(8, 1);
		lcd.print(enc2.value, SIG_DIGITS);
		digitalWrite(LED_Pin, HIGH)
		}
	updateDisplay = false;
	}

/**
 * @brief 
 * Here we setup our encoder, lcd, and various input devices. We also prepare
 * to communicate OSC with Eos by setting up SLIPSerial. Once we are done with
 * setup() we pass control over to loop() and never call setup() again.
 *
 * NOTE: This function is the entry function. This is where control over the
 * Arduino is passed to us (the end user).
 * 
 */
void setup() {
	Serial.begin(9600);
	// Ethernet init
	Ethernet.begin(mac, localIP, subnet);
  udp.begin(localPort);
	// LCD init
	lcd.begin(LCD_CHARS, LCD_LINES);
	lcd.clear();
	// eOS init
	initEOS(); // for hotplug with Arduinos without native USB like UNO
	shiftButton(SHIFT_BTN);
	encoder1.parameter(ENCODER_1_PARAMETER);
	encoder2.parameter(ENCODER_2_PARAMETER);

	pinMode(LED_Pin, OUTPUT)

	displayStatus();
	}

/**
 * @brief 
 * Here we service, monitor, and otherwise control all our peripheral devices.
 * First, we retrieve the status of our encoders and buttons and update Eos.
 * Next, we check if there are any OSC messages for us.
 * Finally, we update our display (if an update is necessary)
 *
 * NOTE: This function is our main loop and thus this function will be called
 * repeatedly forever
 * 
 */
void loop() {
	static String curMsg;
	int size;

	// Check for hardware updates
	next.update();
	last.update();
	encoder1.update();
	encoder2.update();

	// Then we check to see if any OSC commands have come from Eos
	// and update the display accordingly.
	size = udp.parsePacket();
	if (size > 0) {
		// Fill the msg with all of the available bytes
		while (size--) curMsg += (char)(udp.read());
		//}
		parseOSCMessage(curMsg);
		lastMessageRxTime = millis();
		// We only care about the ping if we haven't heard recently
		// Clear flag when we get any traffic
		timeoutPingSent = false;
		curMsg = String();
		}

	if (lastMessageRxTime > 0) {
		unsigned long diff = millis() - lastMessageRxTime;
		//We first check if it's been too long and we need to time out
		if (diff > TIMEOUT_AFTER_IDLE_INTERVAL) {
			connectedToEos = false;
			lastMessageRxTime = 0;
			updateDisplay = true;
			timeoutPingSent = false;
			}
		//It could be the console is sitting idle. Send a ping once to
		// double check that it's still there, but only once after 2.5s have passed
		if (!timeoutPingSent && diff > PING_AFTER_IDLE_INTERVAL) {
			ping(PING_QUERY);
			timeoutPingSent = true;	
			}
		}
	if (updateDisplay) displayStatus();
	}
