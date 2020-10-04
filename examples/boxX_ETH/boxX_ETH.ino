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

//LCD Pins
#define LCD_RS				2
#define LCD_ENABLE		3
#define LCD_D4				4
#define LCD_D5				5
#define LCD_D6				6
#define LCD_D7				7

//Encoder Pins
#define ENC_1_A					A0
#define ENC_1_B					A1
#define ENC_2_A					A2
#define ENC_2_B					A3
#define ENC_1_BTN				A4
#define ENC_2_BTN				A5

//BTN Asignments
#define PARAM_UP_BTN		A8
#define PARAM_DOWN_BTN	A9
#define SEL_LAST_BTN		A10

#define GO_BTN					A11
#define BACK_BTN				A12
#define SHIFT_BTN				A13

//LED Pin Asignment
int LED_Pin= 13

// constants and macros
#define LCD_CHARS				20
#define LCD_LINES				4

#define UP							0
#define DOWN						1

#define SIG_DIGITS			3 // Number of significant digits displayed

#define ENCODERS				2 // number of encoders you use
#define PARAMETER_MAX		26 // 14 for UNO, number of parameters must even

const String HANDSHAKE_QUERY = "ETCOSC?";
const String HANDSHAKE_REPLY = "OK";
const String PING_QUERY = "box_x_eth_hello";
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
const String NO_PARAMETER = "none"; // none is a keyword used when there is no parameter
int8_t idx = 0; // start with parameter index 2 must even

// Network config
uint8_t mac[] = {0x90, 0xA2, 0xDA, 0x10, 0x14, 0x48};
IPAddress localIP(10, 101, 1, 201);
IPAddress subnet(255, 255, 0, 0);
uint16_t localPort = 8001;
IPAddress eosIP(10, 101, 1, 100);
uint16_t eosPort = 8000;

// Global variables
bool updateDisplay = false;
bool connectedToEos = false;
unsigned long lastMessageRxTime = 0;
bool timeoutPingSent = false;

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

struct CueData {
  String cuelist;
  String cue;
  String label;
  String duration;
  String progress;
  }activeCue, pendingCue;

struct ControlButton {
	uint8_t function;
	uint8_t pin;
	uint8_t last;
	} parameterUp, parameterDown;

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
	{"Red"},
	{"Green"},
	{"Blue"},
	{"Cyan"},
	{"Magenta"},
	{"Yellow"},
	// foloowing parameters only for Arduino MEGA
	{"Hue"},
	{"Saturatn"},
	{"cto", "CTO"},
	{"Frame Assembly", "Assembly"},
	{"Thrust A"},
	{"Angle A"},
	{"Thrust B"},
	{"Angle B"},
	{"Thrust C"},
	{"Angle C"},
	{"Thrust D"},
	{"Angle D"},
	};

// Hardware constructors
EthernetUDP udp;
EOS eos(udp, eosIP, eosPort);

LiquidCrystal lcd(LCD_RS, LCD_ENABLE, LCD_D4, LCD_D5, LCD_D6, LCD_D7); // rs, enable, d4, d5, d6, d7

Key selectLast(SEL_LAST_BTN, "SELECT_LAST");
Key go(GO_BTN, "GO_0");
Key back(BACK_BTN, "STOP");
Encoder encoder1(ENC_1_A, ENC_1_B, FORWARD);
Encoder encoder2(ENC_2_A, ENC_2_B, FORWARD);

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
	filter("/eos/out/param/*");
	filter("/eos/out/ping");
	filter("/eos/out/active/cue/text");
	filter("/eos/out/pending/cue/text");
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
	// Prepare the message for routing by filling an OSCMessage object with our message string
	static String parseMsg;
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
	parseMsg = String();
	parseMsg = PARAMETER_QUERY + parameter[idx + 1].name;
	if (msg.indexOf(parseMsg) != -1) {
		parameter[idx + 1].value = oscmsg.getFloat(0);
		connectedToEos = true;
		updateDisplay = true;
		parseMsg = String();
		return;
		}
	// Route cue messages to the relevant update function
	oscmsg.route("/eos/out/active/cue/text", activeCueUpdate);
	oscmsg.route("/eos/out/pending/cue/text", pendingCueUpdate);
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
 * @brief 
 * Updates the display with the latest parameter values.
 * 
 */
void displayStatus() {
	lcd.clear();

	if (!connectedToEos) {
		// display a splash message before the Eos connection is open
		lcd.setCursor(0, 0);
		lcd.print(String("Box X v" + VERSION_STRING).c_str());
		lcd.setCursor(0, 1);
		lcd.print("waiting for EOS...");
		// LED disable
		digitalWrite(LED_Pin, LOW)
		}
	else {
		// cue data
		lcd.setCursor(0, 0);
		lcd.write(uint8_t(0)); // special char up arrow
		lcd.setCursor(0, 1);
		lcd.write(uint8_t(1)); // special char down arrow
		lcd.setCursor(2, 0);
		lcd.print(activeCue.cue);
		lcd.setCursor(6, 0);
		lcd.print(activeCue.label.substring(0, 8)); // max 6 chars
		lcd.setCursor(15, 0);
		lcd.print(activeCue.progress);
		lcd.setCursor(2, 1);
		lcd.print(pendingCue.cue);
		lcd.setCursor(6, 1);
		lcd.print(pendingCue.label.substring(0, 8)); // max 6 chars
		lcd.setCursor(15, 1);
		lcd.print(pendingCue.duration);
		// encoder data
		lcd.setCursor(0, 2);
		if (parameter[idx].displayName == 0) lcd.print(parameter[idx].name);
		else lcd.print(parameter[idx].displayName);
		lcd.setCursor(10, 2);
		if (parameter[idx + 1].displayName == 0) lcd.print(parameter[idx + 1].name);
		else lcd.print(parameter[idx + 1].displayName);
		lcd.setCursor(0, 3);
		lcd.print(parameter[idx].value, SIG_DIGITS);
		lcd.setCursor(10, 3);
		lcd.print(parameter[idx + 1].value, SIG_DIGITS);
		// LED enable
		digitalWrite(LED_Pin,HIGH);
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
	// Ethernet init
	Ethernet.begin(mac, localIP, subnet);
  udp.begin(localPort);
	// LCD init
	lcd.createChar(0, upArrow);
	lcd.createChar(1, downArrow);
	lcd.begin(LCD_CHARS, LCD_LINES);
	lcd.clear();
	// eOS init
	initEOS(); // for hotplug with Arduinos without native USB like UNO
	shiftButton(SHIFT_BTN);
	encoder1.button(ENC_1_BTN);
	encoder2.button(ENC_2_BTN);
	encoder1.parameter(parameter[idx].name);
	encoder2.parameter(parameter[idx + 1].name);
	initControlButton(&parameterUp, PARAM_UP_BTN, UP);
	initControlButton(&parameterDown, PARAM_DOWN_BTN, DOWN);
	// LED init
	pinMode(LED_Pin, OUTPUT);

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
	updateControlButton(&parameterUp);
	updateControlButton(&parameterDown);
	go.update();
	back.update();
	selectLast.update();
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
