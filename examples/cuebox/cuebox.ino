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

/*
Note: Updated once per second.
“/eos/out/active/cue/<cue list number>/<cue number>”, <float argument with percent complete (0.0-1.0)>
• “/eos/out/active/cue”, <float argument with percent complete (0.0-1.0)>
• “/eos/out/active/cue/text”, <string argument with descriptive text about the active cue, ex: “1/ 2.3 Label 0:05 75%”>
• “/eos/out/pending/cue/<cue list number>/<cue number>”
• “/eos/out/pending/cue/text”, <string argument with descriptive text about the pending cue, ex: “1/2.4 Label 0:30”>
*/

// libraries included
#include "Arduino.h"
#include <LiquidCrystal.h>
#include <OSCMessage.h>
#include "eOS.h"

#ifdef BOARD_HAS_USB_SERIAL
	#include <SLIPEncodedUSBSerial.h>
	SLIPEncodedUSBSerial SLIPSerial(thisBoardsSerialUSB);
#else
	#include <SLIPEncodedSerial.h>
	SLIPEncodedSerial SLIPSerial(Serial);
#endif

// constants and macros
#define LCD_CHARS			16
#define LCD_LINES			2

#define LCD_RS				2
#define LCD_ENABLE		3
#define LCD_D4				4
#define LCD_D5				5
#define LCD_D6				6
#define LCD_D7				7

#define GO_BTN				8
#define BACK_BTN			9

const String HANDSHAKE_QUERY = "ETCOSC?";
const String HANDSHAKE_REPLY = "OK";
const String PING_QUERY = "cuebox2_hello";
const String CUE_ACTIVE_QUERY = "/eos/out/active/cue/text";
const String CUE_PENDING_QUERY = "/eos/out/pending/cue/text";

// See displayScreen() below - limited to 10 chars (after 6 prefix chars)
const String VERSION_STRING = "2.0.0.0";

// Change these values to alter how long we wait before sending an OSC ping
// to see if Eos is still there, and then finally how long before we
// disconnect and show the splash screen
// Values are in milliseconds
#define PING_AFTER_IDLE_INTERVAL		2500
#define TIMEOUT_AFTER_IDLE_INTERVAL	5000

// Global variables
bool updateDisplay = false;
bool connectedToEos = false;
unsigned long lastMessageRxTime = 0;
bool timeoutPingSent = false;

struct CueData {
  String cuelist;
  String cue;
  String label;
  String duration;
  String progress;
  }activeCue, pendingCue;

// special chars
uint8_t upArrow[8] = {  
  B00100,
  B01010,
  B10001,
  B00100,
  B00100,
  B00100,
	B00000,
	};

uint8_t downArrow[8] = {
	B00000,
  B00100,
  B00100,
  B00100,
  B10001,
  B01010,
  B00100,
	};


// Hardware constructors
EOS eos;
LiquidCrystal lcd(LCD_RS, LCD_ENABLE, LCD_D4, LCD_D5, LCD_D6, LCD_D7); // rs, enable, d4, d5, d6, d7

Key go(GO_BTN, "GO_0");
Key back(BACK_BTN, "STOP");

// Local functions

/**
 * @brief Parser for cue text
 * 
 * @param data 
 * @param msg 
 */
void parseCueMessage(struct CueData *data, String msg) {
  uint8_t size = msg.length();
  uint8_t firstSpace = msg.indexOf(" ");
  uint8_t cueSeparator = msg.indexOf("/");
  uint8_t lastSpace = msg.lastIndexOf(" ");
  
  if (cueSeparator < firstSpace && cueSeparator != 0) { // there is an cue separator
    data->cuelist = msg.substring(0, cueSeparator);
    data->cue = msg.substring(cueSeparator + 1, firstSpace);
    }
  else {
    data->cuelist = String();
    data->cue = msg.substring(0, firstSpace);
    }
  
  if (msg[size - 1] == '%') { // check if active or peding cue, only active cues have a progress counter in %
    data->progress = msg.substring(lastSpace + 1);
    uint8_t durSpace = msg.lastIndexOf(" ", lastSpace - 1);
    data->duration = msg.substring(durSpace + 1, lastSpace);
    data->label = msg.substring(firstSpace + 1, durSpace);
    }
  else {
    data->progress = String();
    data->duration = msg.substring(lastSpace + 1);
    data->label = msg.substring(firstSpace + 1, lastSpace);
    }
  }

/**
 * @brief Init the console, gives back a handshake reply
 * and send the filters and subscribtions.
 *
 */
void initEOS() {
	SLIPSerial.print(HANDSHAKE_REPLY);
	filter("/eos/out/active/cue/text");
	filter("/eos/out/pending/cue/text");
	filter("/eos/out/ping");
	}

/**
 * @brief Get the text message of the active cue
 * 
 * @param msg OSC message
 * @param addressOffset 
 */
void activeCueUpdate(OSCMessage& msg, int addressOffset) {
	char text[msg.getDataLength(0)];
	msg.getString(0, text);
	parseCueMessage(&activeCue, text);
	connectedToEos = true;
	updateDisplay = true; 
	}

/**
 * @brief Get the text message of the pending cue
 * 
 * @param msg OSC message
 * @param addressOffset 
 */
void pendingCueUpdate(OSCMessage& msg, int addressOffset) {
	char text[msg.getDataLength(0)];
	msg.getString(0, text);
	parseCueMessage(&pendingCue, text);
	connectedToEos = true;
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
	// Check to see if this is the handshake string
	if (msg.indexOf(HANDSHAKE_QUERY) != -1) {
		initEOS();
		// Make our splash screen go away
		connectedToEos = true;
		updateDisplay = true;
		}

	else {
		// Prepare the message for routing by filling an OSCMessage object with our message string
		OSCMessage oscmsg;
		oscmsg.fill((uint8_t*)msg.c_str(), (int)msg.length());
		// Route cue messages to the relevant update function
		oscmsg.route("/eos/out/active/cue/text", activeCueUpdate);
		oscmsg.route("/eos/out/pending/cue/text", pendingCueUpdate);
		}
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
		lcd.print(String("Cuebox v" + VERSION_STRING).c_str());
		lcd.setCursor(0, 1);
		lcd.print("waiting for EOS");
		}
	else {
		lcd.setCursor(0, 0);
		lcd.write(uint8_t(0)); // special char up arrow
		lcd.setCursor(0, 1);
		lcd.write(uint8_t(1)); // special char down arrow
		lcd.setCursor(2, 0);
		lcd.print(activeCue.cue);
		lcd.setCursor(5, 0);
		lcd.print(activeCue.label.substring(0, 6)); // max 6 chars
		lcd.setCursor(12, 0);
		lcd.print(activeCue.progress);
		lcd.setCursor(2, 1);
		lcd.print(pendingCue.cue);
		lcd.setCursor(5, 1);
		lcd.print(pendingCue.label.substring(0, 6)); // max 6 chars
		lcd.setCursor(12, 1);
		lcd.print(pendingCue.duration);
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
	SLIPSerial.begin(115200);
	// This is a hack around an Arduino bug. It was taken from the OSC library
	//examples
	#ifdef BOARD_HAS_USB_SERIAL
		#ifndef TEENSYDUINO
			while (!SerialUSB);
		#endif
	#else
		while (!Serial);
	#endif

	lcd.createChar(0, upArrow);
	lcd.createChar(1, downArrow);
	lcd.begin(LCD_CHARS, LCD_LINES);
	lcd.clear();

	initEOS(); // for hotplug with Arduinos without native USB like UNO

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
	go.update();
	back.update();

	// Then we check to see if any OSC commands have come from Eos
	// and update the display accordingly.
	size = SLIPSerial.available();
	if (size > 0) {
		// Fill the msg with all of the available bytes
		while (size--) curMsg += (char)(SLIPSerial.read());
		}
	if (SLIPSerial.endofPacket()) {
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
