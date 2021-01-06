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
#define FADER_1_LEVELER			A0
#define FADER_1_FIRE_BUTTON	2
#define FADER_1_STOP_BUTTON	8
#define FADER_2_LEVELER			A1
#define FADER_2_FIRE_BUTTON	3
#define FADER_2_STOP_BUTTON	9
#define FADER_3_LEVELER			A2
#define FADER_3_FIRE_BUTTON	4
#define FADER_3_STOP_BUTTON	10
#define FADER_4_LEVELER			A3
#define FADER_4_FIRE_BUTTON	5
#define FADER_4_STOP_BUTTON	11
#define FADER_5_LEVELER			A4
#define FADER_5_FIRE_BUTTON	6
#define FADER_5_STOP_BUTTON	12
#define FADER_6_LEVELER			A5
#define FADER_6_FIRE_BUTTON	7
#define FADER_6_STOP_BUTTON	13

// constants and macros
#define FADER_PAGE				1 // fader page on EOS / Nomad
#define NUMBER_OF_FADERS	10 // size of the faders per page on EOS / Nomad
#define FADER_BANK				1	// virtuell OSC bank

const String HANDSHAKE_QUERY = "ETCOSC?";
const String HANDSHAKE_REPLY = "OK";
const String PING_QUERY = "faderwingX_hello";

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
Fader fader1(FADER_1_LEVELER, FADER_1_FIRE_BUTTON, FADER_1_STOP_BUTTON, 1, FADER_BANK);
Fader fader2(FADER_2_LEVELER, FADER_2_FIRE_BUTTON, FADER_2_STOP_BUTTON, 2, FADER_BANK);
Fader fader3(FADER_3_LEVELER, FADER_3_FIRE_BUTTON, FADER_3_STOP_BUTTON, 3, FADER_BANK);
Fader fader4(FADER_4_LEVELER, FADER_4_FIRE_BUTTON, FADER_4_STOP_BUTTON, 4, FADER_BANK);
Fader fader5(FADER_5_LEVELER, FADER_5_FIRE_BUTTON, FADER_5_STOP_BUTTON, 5, FADER_BANK);
Fader fader6(FADER_6_LEVELER, FADER_6_FIRE_BUTTON, FADER_6_STOP_BUTTON, 6, FADER_BANK);

// Local functions

/**
 * @brief Init the console, gives back a handshake reply
 * and send the filters and subscribtions.
 *
 */
void initEOS() {
	SLIPSerial.print(HANDSHAKE_REPLY);
	filter("/eos/out/ping");
	initFaders(FADER_PAGE, NUMBER_OF_FADERS, FADER_BANK);
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
	fader1.update();
	fader2.update();
	fader3.update();
	fader4.update();
	fader5.update();
	fader6.update();

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
