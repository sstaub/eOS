/*
eOS library for USB and Ethernet UDPis placed under the MIT license
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
fader is a linear 10kOhm, from Bourns or ALPS and can be 45/60/100mm long
put 10nF ceramic capitors between ground and fader levelers to prevent analog noise
Arduino UNO, MEGA:
use IOREF instead +5V to the top (single pin) of the fader (100%)
GND to the center button pin (2 pins, the outer pin is normaly for the leveler) of the fader (0%)
TEENSY:
+3.3V to the top (single pin) of the fader (100%)
use ANALOG GND instead the normal GND to the center button pin (2 pins, the outer pin is normaly for the leveler) of the fader (0%)

put 100nF ceramic capitors between ground and the input of the buttons
*/

#ifndef EOS_H
	#define EOS_H

#include "Arduino.h"
#include "OSCMessage.h"
#include "Udp.h"

#define SUBSCRIBE		1
#define UNSUBSCRIBE	0

#define EDGE_DOWN		1
#define EDGE_UP			0

#define FORWARD			0
#define REVERSE			1

#define INT_ACC			4 // only used for intens
#define WHEEL_ACC		1

#define FADER_UPDATE_RATE_MS	40 // update each 40ms
#define THRESHOLD		4 // Jitter threshold of the faders

enum interface_t {EOSUSB, EOSUDP};

/**
 * @brief Class definitions for a general interface
 * 
 */
class EOS {

	public:

		/**
		 * @brief Construct a new EOS object for Etnernet communication over udp
		 * 
		 * @param udp UPD instance
		 * @param ip destination IP
		 * @param port destination Port
		 * @param interface Interface type EOSUDP
		 */
		EOS(UDP &udp, IPAddress ip, uint16_t port, interface_t interface = EOSUDP);

		/**
		 * @brief Construct a new EOS object for Serial communication
		 * 
		 * @param interface Interface type EOSUSB
		 */
		EOS(interface_t interface = EOSUSB);

		/**
		 * @brief send OSC message
		 * 
		 * @param msg OSC message
		 * @param ip optional destination IP address
		 * @param port optional destination port
		 */
		void sendOSC(OSCMessage& msg, IPAddress ip, uint16_t port);

		/**
		 * @brief send OSC message
		 * 
		 * @param msg 
		 */
		void sendOSC(OSCMessage& msg);

	private:

		UDP *udp;
		HardwareSerial *s;
		IPAddress ip;
		uint16_t port;
		interface_t interface;

	};

/**
 * @brief send an OSC message
 * 
 * @param msg OSC message
 */
//void sendOSC(OSCMessage& msg);

/**
 * @brief Filter for messages you want receive
 * 
 * 
 * @param message you want receive
 */
void filter(String pattern);

/**
 * @brief Subscribe a parameter you want receive
 * 
 * @param parameter  for subscription
 */
void subscribe(String parameter);

/**
 * @brief Unsubscribe a parameter
 * 
 * @param parameter for unsubscription
 */
void unSubscribe(String parameter);


/**
 * @brief send a ping without a message
 * 
 */
void ping();

/**
 * @brief send a ping with a meassge
 * 
 * @param message 
 */
void ping(String message);

/**
 * @brief send a string to the command line
 * 
 * @param cmd command line String
 */
void command(String cmd);

/**
 * @brief send a new command line string
 * 
 * @param cmd command line String
 */
void newCommand(String newCmd);

/**
 * @brief set the user
 * 
 * @param userID user
 */
void user(int16_t userID);


/**
 * @brief modes for the encoder button if available
 * 
 */
enum ButtonMode {
	HOME, FINE
	};

/**
 * @brief initialise the Shift button for encoders and wheels
 * 
 * @param pin of the shift button 
 */
void shiftButton(uint8_t pin);

/**
 * @brief Class definitions for Encoder controlling parameters by their name
 * 
 */
class Encoder {

	public:

		/**
		 * @brief Construct a new Encoder object
		 * 
		 * @param pinA pin A of the encoder
		 * @param pinB pin B of the encoder
		 * @param direction the direction for the wheel can be FORWARD or REVERSE
		 */
		Encoder(uint8_t pinA, uint8_t pinB, uint8_t direction = FORWARD);

		/**
		 * @brief add an optional encoder button
		 * 
		 * @param buttonPin pin of the button
		 * @param buttonMode the parameter function of the button, in the moment Home or FINE
		 */
		void button(uint8_t buttonPin, ButtonMode buttonMode = HOME);

		/**
		 * @brief set the parameter which should controlled by the encoder
		 * 
		 * @param param parameter name as a String
		 */
		void parameter(String param);

		/**
		 * @brief get the parameter which is controlled by the encoder
		 * 
		 * @return String 
		 */
		String parameter();

		/**
		 * @brief update the output of the encoder, must be in the loop
		 * 
		 */
		void update();
	
	private:
		String param;
		uint8_t pinA;
		uint8_t pinB;
		uint8_t buttonPin;
		ButtonMode buttonMode;
		uint8_t pinALast;
		uint8_t pinACurrent;
		uint8_t buttonPinLast;
		uint8_t direction;
		int8_t encoderMotion;

	};

/**
 * @brief Class definition for Wheels, controlling parameters by their index
 * 
 */
class Wheel {

	public:

		/**
		 * @brief Construct a new Wheel object
		 * 
		 * @param pinA pin A of the encoder
		 * @param pinB pinB of the encoder
		 * @param direction the direction for the wheel can be FORWARD or REVERSE
		 */
		Wheel(uint8_t pinA, uint8_t pinB, uint8_t direction = FORWARD);

		/**
		 * @brief add an optional encoder button
		 * 
		 * @param buttonPin pin of the button
		 * @param buttonMode the parameter function of the button, in the moment only FINE
		 */
		void button(uint8_t buttonPin, ButtonMode buttonMode = FINE);

		/**
		 * @brief set the index number of the wheel
		 * 
		 * @param idx index
		 */
		void index(uint8_t idx);

		/**
		 * @brief get the parameter which is controlled by the encoder
		 * 
		 * @return uint8_t the wheel idx 
		 */
		uint8_t index();

		/**
		 * @brief update the output of the Wheel, must be in the loop
		 * 
		 */
		void update();
	
	private:
		uint8_t idx;
		uint8_t pinA;
		uint8_t pinB;
		uint8_t buttonPin;
		uint8_t pinALast;
		uint8_t pinACurrent;
		uint8_t buttonPinLast;
		uint8_t buttonMode;
		uint8_t direction;
		int8_t encoderMotion;

	};

/**
 * @brief Class definitions for Key commands
 * 
 */
class Key {

	public:

		/**
		 * @brief Construct a new Key object
		 * 
		 * @param pin button pin
		 * @param key EOS Key command
		 */
		Key(uint8_t pin, String key);

		/**
		 * @brief update the state of the Key button, must in while() loop
		 * 
		 */
		void update();

	private:

		String keyPattern;
  	uint8_t pin;
  	uint8_t last;

	};

/**
 * @brief Class definitions for a Submaster
 * 
 */
class Submaster {

	public:

		/**
		 * @brief Construct a new Submaster object
		 * 
		 * @param analogPin pin for fader leveler
		 * @param firePin pin for the fire button
		 * @param sub submaster number
		 */
		Submaster(uint8_t analogPin, uint8_t firePin, uint8_t sub);

		/**
		 * @brief update the state of the Submaster and fire button, must in while() loop
		 * 
		 */
		void update();

	private:

		String subPattern;
		uint8_t analogPin;
		uint8_t firePin;
		int16_t analogLast;
		uint8_t fireLast;
		uint32_t updateTime;

	};

/**
 * @brief initialise a fader bank
 * 
 * @param page fader page of the console, standard is 1
 * @param faders number of faders, standard is 10
 * @param bank number of the OSC fader bank, standard is 1
 */
void initFaders(uint8_t page = 1, uint8_t faders = 10, uint8_t bank = 1);

/**
 * @brief Fader object with stop and fire buttons
 * 
 */
class Fader {

	public:

	/**
	 * @brief Construct a new Fader object
	 * 
	 * @param analogPin pin for the fader leveler
	 * @param firePin pin for the fire button
	 * @param stopPin pin for the stop button
	 * @param fader number of the fader inside the bank
	 * @param bank number of the OSC fader bank, standard is 1
	 */
	Fader(uint8_t analogPin, uint8_t firePin, uint8_t stopPin, uint8_t fader, uint8_t bank = 1);

	/**
	 * @brief update the state of the Fader and fire / stop buttons, must in while() loop
	 * 
	 */
	void update();

	/**
	 * @brief set the OSC fader bank of the fader object
	 * 
	 * @param bank OSC fader bank
	 */
	void faderBank(uint8_t bank);

	/**
	 * @brief get the OSC bank number of the fader object
	 * 
	 * @return uint8_t number of the OSC fader bank
	 */
	uint8_t faderBank();

	/**
	 * @brief set the fader number of the fader object
	 * 
	 * @param fader number of the fader
	 */
	void faderNumber(uint8_t fader);

	/**
	 * @brief get the fader number of the fader object
	 * 
	 * @return uint8_t fader number
	 */
	uint8_t faderNumber();

	private:

		String faderPattern;
		uint8_t bank; 
		uint8_t fader;
		uint8_t analogPin;
		uint8_t firePin;
		uint8_t stopPin;
		int16_t analogLast;
  	uint8_t fireLast;
  	uint8_t stopLast;
		uint32_t updateTime;

	};

/**
 * @brief Class definitions for a Macro button
 * 
 */
class Macro {

	public:

	/**
	 * @brief Construct a new Macro object
	 * 
	 * @param pin macro button pin
	 * @param macro number of the macro
	 */
	Macro(uint8_t pin, uint16_t macro);

	/**
	 * @brief update the state of the Macro button, must in while() loop
	 * 
	 */
	void update();

	private:

		String firePattern;
		uint8_t pin;
    uint8_t last;

	};

class OscButton {

	public:

	/**
	 * @brief Construct a new osc Button object for sending an integer value
	 * 
	 * @param pin button pin
	 * @param pattern OSC address
	 * @param integer32 value must cast (int32_t) when using with a non matching size
	 * @param ip optional destination IP address
	 * @param port optional destination port
	 */
	OscButton(uint8_t pin, String pattern, int32_t integer32);
	OscButton(uint8_t pin, String pattern, int32_t integer32, IPAddress ip, uint16_t port);
	
	/**
	 * @brief Construct a new osc Button object for sending a float value
	 * 
	 * @param pin button pin
	 * @param pattern OSC address
	 * @param float32 float value
	 * @param ip optional destination IP address
	 * @param port optional destination port
	 */
	OscButton(uint8_t pin, String pattern, float float32);
	OscButton(uint8_t pin, String pattern, float float32, IPAddress ip, uint16_t port);
	
	/**
	 * @brief Construct a new osc Button object for ssending a String
	 * 
	 * @param pin button pin
	 * @param pattern OSC address
	 * @param message String message
	 * @param ip optional destination IP address
	 * @param port optional destination port
	 */
	OscButton(uint8_t pin, String pattern, String message);
	OscButton(uint8_t pin, String pattern, String message, IPAddress ip, uint16_t portt);
	
	/**
	 * @brief Construct a new osc Button object with no value
	 * 
	 * @param pin button pin
	 * @param pattern OSC address
	 * @param ip optional destination IP address
	 * @param port optional destination port
	 */
	OscButton(uint8_t pin, String pattern);
	OscButton(uint8_t pin, String pattern, IPAddress ip, uint16_t port);
	
	/**
	 * @brief update the state of the Macro button, must in while() loop
	 * 
	 */
	void update();

	private:

		enum osc_t {NONE, INT32, FLOAT32, STRING};
		osc_t typ;
		String pattern;
		int32_t integer32;
		float float32;
		String message;
		IPAddress ip;
		uint16_t port;
		uint8_t pin;
    uint8_t last;

	};

#endif
