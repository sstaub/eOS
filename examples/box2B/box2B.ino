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

#ifdef BOARD_HAS_USB_SERIAL
	#include <SLIPEncodedUSBSerial.h>
	SLIPEncodedUSBSerial SLIPSerial(thisBoardsSerialUSB);
#else
	#include <SLIPEncodedSerial.h>
	SLIPEncodedSerial SLIPSerial(Serial);
#endif

// constants and macros
#define LCD_CHARS				16
#define LCD_LINES				2

#define LCD_RS					2
#define LCD_ENABLE			3
#define LCD_D4					4
#define LCD_D5					5
#define LCD_D6					6
#define LCD_D7					7

#define ENC_1_A					A0
#define ENC_1_B					A1
#define ENC_2_A					A2
#define ENC_2_B					A3
#define ENC_1_BTN				A4
#define ENC_2_BTN				A5

#define PARAM_UP_BTN		8
#define PARAM_DOWN_BTN	9
#define SHIFT_BTN				10
#define LAST_BTN				11
#define NEXT_BTN				12
#define SELECT_LAST_BTN	13

#define SIG_DIGITS			3 // Number of significant digits displayed

// number of encoders you use
#define ENCODERS				2
#define PARAMETER_MAX		14 // number of parameters must even

#define UP							0
#define DOWN						1

const String HANDSHAKE_QUERY = "ETCOSC?";
const String HANDSHAKE_REPLY = "OK";
const String PING_QUERY = "box2b_hello";
const String PARAMETER_QUERY = "/eos/out/param/";
const String NO_PARAMETER = "none"; // none is a keyword used when there is no parameter

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
int8_t idx = 0; // start with parameter index 2 must even

/**
 * @brief Parameter structure
 * 
 */
struct Parameter {
	String name;
	String displayName;
	float value;
	};

/**
 * @brief Array of the Parameter structure, the number of possible parameters
 * depends on the used mikrocontroller and its RAM size.
 * 
 */
struct Parameter parameter[PARAMETER_MAX] = {
	{NO_PARAMETER, "------"},
	{"Intens"},
	{"Pan"},
	{"Tilt"},
	{"Zoom"},
	{"Edge"},
	{"Iris"},
	{"Diffusn"},
	//{"Hue"},
	//{"Saturatn"},
	{"Red"},
	{"Green"},
	{"Blue"},
	{"Cyan"},
	{"Magenta"},
	{"Yellow"},
	//{"cto", "CTO"},
	//{"frame_assembly", "Assembly"},
	//{"thrust_A", "ThrustA"},
	//{"angle_A", "AngleA"},
	//{"thrust_B", "ThrustB"},
	//{"thrust_B", "AngleB"},
	//{"thrust_C", "ThrustC"},
	//{"angle_C", "AngleC"},
	//{"thrust_D", "ThrustD"},
	//{"angle_D", "AngleD"}
	};

struct ControlButton {
	uint8_t function;
	uint8_t pin;
	uint8_t last;
	} parameterUp, parameterDown;

// Hardware constructors
EOS eos;
LiquidCrystal lcd(LCD_RS, LCD_ENABLE, LCD_D4, LCD_D5, LCD_D6, LCD_D7); // rs, enable, d4, d5, d6, d7

Encoder encoder1(ENC_1_A, ENC_1_B, FORWARD);
Encoder encoder2(ENC_2_A, ENC_2_B, FORWARD);
Key last(LAST_BTN, "LAST");
Key next(NEXT_BTN, "NEXT");
Key selectLast(SELECT_LAST_BTN, "SELECT_LAST");


// Local functions

/**
 * @brief Init the console, gives back a handshake reply and send the filters and subscribtions.
 *
 */
void initEOS() {
	SLIPSerial.print(HANDSHAKE_REPLY);
	filter("/eos/out/param/*");
	filter("/eos/out/ping");
	subscribe(parameter[idx].name);
	subscribe(parameter[idx + 1].name);
	}

/**
 * @brief This helper first unsubribe all parameters and subscribe the new ones
 * 
 */
void parameterUpdate() {
	unSubscribe("*");
	subscribe(parameter[idx].name);
	subscribe(parameter[idx + 1].name);
	}

/**
 * @brief Updates the display with the latest parameter values.
 * 
 */
void displayStatus() {
	lcd.clear();
	if (!connectedToEos) { // display a splash message before the Eos connection is open
		lcd.setCursor(0, 0);
		lcd.print(String("Box2B v" + VERSION_STRING).c_str());
		lcd.setCursor(0, 1);
		lcd.print("waiting for EOS");
		}
	else {
		lcd.setCursor(0, 0);
		if (parameter[idx].displayName == 0) lcd.print(parameter[idx].name);
		else lcd.print(parameter[idx].displayName);
		lcd.setCursor(8, 0);
		if (parameter[idx + 1].displayName == 0) lcd.print(parameter[idx + 1].name);
		else lcd.print(parameter[idx + 1].displayName);
		lcd.setCursor(0, 1);
		lcd.print(parameter[idx].value, SIG_DIGITS);
		lcd.setCursor(8, 1);
		lcd.print(parameter[idx + 1].value, SIG_DIGITS);
		}
	updateDisplay = false;
	}

/**
 * @brief Given an unknown OSC message we check to see if it's a handshake message.
 * If it's a handshake we issue the init, otherwise we update the parameters values.
 * 
 * @param msg - the OSC message of unknown importance
 *
 */
void parseOSCMessage(String& msg) {
	static String parseMsg;
	if (msg.indexOf(HANDSHAKE_QUERY) != -1) { // Check to see if this is the handshake string
		initEOS();
		connectedToEos = true;
		updateDisplay = true;
		return;
		}
	else {
		OSCMessage oscmsg;
		oscmsg.fill((uint8_t*)msg.c_str(), (int)msg.length());
		parseMsg = PARAMETER_QUERY + parameter[idx].name;
		if (msg.indexOf(parseMsg) != -1) {
				parameter[idx].value = oscmsg.getFloat(0);
				connectedToEos = true;
				updateDisplay = true;
				parseMsg = String();
				return;
				}
		parseMsg = PARAMETER_QUERY + parameter[idx + 1].name;
		if (msg.indexOf(parseMsg) != -1) {
			parameter[idx + 1].value = oscmsg.getFloat(0);
			connectedToEos = true;
			updateDisplay = true;
			parseMsg = String();
			return;
			}
		}
	}

/**
 * @brief initalizes a given control button struct
 *
 * @param button name of the button structure
 * @param pin arduino pin of the control button
 * @param function can UP or DOWN
 */
void initControlButton(struct ControlButton* button, uint8_t pin, uint8_t function) {
	button->function = function;
	button->pin = pin;
	pinMode(pin, INPUT_PULLUP);
	button->last = digitalRead(pin);
	}

/**
 * @brief Calculates the new index and update the parameters
 *
 * @param button
 */
void updateControlButton(struct ControlButton* button) {
	if ((digitalRead(button->pin)) != button->last) {
		if (button->last == LOW) {
			button->last = HIGH;
			// calc index
			if (button->function == UP) {
				idx += ENCODERS;
				if (idx > PARAMETER_MAX - ENCODERS) {
					idx = 0;
					} 
				encoder1.parameter(parameter[idx].name);
				encoder2.parameter(parameter[idx + 1].name);
				parameterUpdate(); // this is necessary to get actual data for the index
				updateDisplay = true;
				return;
				}
			if (button->function == DOWN) {
				idx -= ENCODERS;
				if (idx < 0) {
					idx = PARAMETER_MAX - ENCODERS;
					} 
				encoder1.parameter(parameter[idx].name);
				encoder2.parameter(parameter[idx + 1].name);
				parameterUpdate(); // this is necessary to get actual data for the index
				updateDisplay = true;
				return;
				}      
			}
		else {
			button->last = LOW;
			}
		}
	}

/**
 * @brief Here we setup our encoder, lcd, and various input devices. We also prepare
 * to communicate OSC with Eos by setting up SLIPSerial. Once we are done with
 * setup() we pass control over to loop() and never call setup() again.
 *
 * NOTE: This function is the entry function. This is where control over the
 * Arduino is passed to us (the end user).
 * 
 */
void setup() {
	SLIPSerial.begin(115200);
	// This is a hack around an Arduino bug. It was taken from the OSC library examples
	#ifdef BOARD_HAS_USB_SERIAL
		#ifndef TEENSYDUINO
			while (!SerialUSB);
		#endif
	#else
		while (!Serial);
	#endif

	lcd.begin(LCD_CHARS, LCD_LINES);
	lcd.clear();

	initEOS(); // for hotplug with Arduinos without native USB like UNO
	shiftButton(SHIFT_BTN);
	encoder1.button(ENC_1_BTN);
	encoder2.button(ENC_2_BTN);
	encoder1.parameter(parameter[idx].name);
	encoder2.parameter(parameter[idx + 1].name);
	initControlButton(&parameterUp, PARAM_UP_BTN, UP);
	initControlButton(&parameterDown, PARAM_DOWN_BTN, DOWN);
	displayStatus();
	}

/**
 * @brief Here we service, monitor, and otherwise control all our peripheral devices.
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
	updateControlButton(&parameterUp);
	updateControlButton(&parameterDown);
	encoder1.update();
	encoder2.update();
	last.update();
	next.update();
	selectLast.update();

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
