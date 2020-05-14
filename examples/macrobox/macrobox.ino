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
#include <OSCMessage.h>
#include "eOS.h"

#ifdef BOARD_HAS_USB_SERIAL
	#include <SLIPEncodedUSBSerial.h>
	SLIPEncodedUSBSerial SLIPSerial(thisBoardsSerialUSB);
#else
	#include <SLIPEncodedSerial.h>
	SLIPEncodedSerial SLIPSerial(Serial);
#endif

// define hardware pins
#define MACRO_BTN_1		2
#define MACRO_BTN_2		3
#define MACRO_BTN_3		4
#define MACRO_BTN_4		5
#define MACRO_BTN_5		6
#define MACRO_BTN_6		7
#define MACRO_BTN_7		8
#define MACRO_BTN_8		9
#define MACRO_BTN_9		10
#define MACRO_BTN_10	11
#define MACRO_BTN_11	12
#define MACRO_BTN_12	13

// macros
#define MACRO_1		101
#define MACRO_2		102
#define MACRO_3		103
#define MACRO_4		104
#define MACRO_5		105
#define MACRO_6		106
#define MACRO_7		107
#define MACRO_8		108
#define MACRO_9		109
#define MACRO_10	110
#define MACRO_11	112
#define MACRO_12	113

// constants and macros
#define FADER_PAGE				1 // fader page on EOS / Nomad
#define NUMBER_OF_FADERS	10 // size of the faders per page on EOS / Nomad
#define FADER_BANK				1	// virtuell OSC bank

const String HANDSHAKE_QUERY = "ETCOSC?";
const String HANDSHAKE_REPLY = "OK";
const String PING_QUERY = "macroboxX_hello";
const String PARAMETER_QUERY = "/eos/out/param/";

// Change these values to alter how long we wait before sending an OSC ping
// to see if Eos is still there, and then finally how long before we
// disconnect and show the splash screen
// Values are in milliseconds
#define PING_AFTER_IDLE_INTERVAL		2500
#define TIMEOUT_AFTER_IDLE_INTERVAL	5000

// Global variables
bool connectedToEos = false;
unsigned long lastMessageRxTime = 0;
bool timeoutPingSent = false;


// Hardware constructors
EOS eos;
Macro macro1(MACRO_BTN_1, MACRO_1);
Macro macro2(MACRO_BTN_2, MACRO_2);
Macro macro3(MACRO_BTN_3, MACRO_3);
Macro macro4(MACRO_BTN_4, MACRO_4);
Macro macro5(MACRO_BTN_5, MACRO_5);
Macro macro6(MACRO_BTN_6, MACRO_6);
Macro macro7(MACRO_BTN_7, MACRO_7);
Macro macro8(MACRO_BTN_8, MACRO_8);
Macro macro9(MACRO_BTN_9, MACRO_9);
Macro macro10(MACRO_BTN_10, MACRO_10);
Macro macro11(MACRO_BTN_11, MACRO_11);
Macro macro12(MACRO_BTN_12, MACRO_12);

// Local functions

/**
 * @brief Init the console, gives back a handshake reply
 * and send the filters and subscribtions.
 *
 */
void initEOS() {
	SLIPSerial.print(HANDSHAKE_REPLY);
	filter("/eos/out/ping");
	}

/**
 * @brief 
 * Given an unknown OSC message we check to see if it's a handshake message.
 * If it's a handshake we issue a subscribe.
 * 
 * @param msg - the OSC message of unknown importance
 *
 */
void parseOSCMessage(String& msg) {
	// check to see if this is the handshake string
	if (msg.indexOf(HANDSHAKE_QUERY) != -1) {
		// handshake string found!
		connectedToEos = true;
		initEOS();
		}

	if (msg.indexOf(PING_QUERY) != -1) {
		// handshake string found!
		connectedToEos = true;
		}
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

	initEOS(); // for hotplug with Arduinos without native USB like UNO
	initFaders(FADER_PAGE, NUMBER_OF_FADERS, FADER_BANK);
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
	macro1.update();
	macro2.update();
	macro3.update();
	macro4.update();
	macro5.update();
	macro6.update();
	macro7.update();
	macro8.update();
	macro9.update();
	macro10.update();
	macro11.update();
	macro12.update();

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
			timeoutPingSent = false;
			}
		//It could be the console is sitting idle. Send a ping once to
		// double check that it's still there, but only once after 2.5s have passed
		if (!timeoutPingSent && diff > PING_AFTER_IDLE_INTERVAL) {
			ping(PING_QUERY);
			timeoutPingSent = true;
			}
		}
	}
