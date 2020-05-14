# eOS for USB and Ethernet UDP
An object orientated library for Arduino to control ETCs EOS Family Consoles with OSC over USB or Ethernet UDP. The goal of the library is to have a smart toolbox to create your own boxes which covers your needing by endless combinations of hardware elements.

The library depends on:
- the OSC library from CNMAT https://github.com/CNMAT/OSC

For use with Ethernet
- an Arduino compatible Ethernet library like Ethernet3 https://github.com/sstaub/Ethernet3
- optional for Teensy MAC address https://github.com/sstaub/TeensyID

This library and examples are a replacment for the #lighthack project, it uses Ethernet instead of USB so the library does not depend on the board type.
The in the examples used Ethernet library only supports the Wiznet 5500 chip, used on Ethernet Shield 2 or the popular USR-ES1 module which you can buy for a small pice at aliexpress.com

Future:
- Parsers for extracting implicit OSC outputs, like Wheel, Softkey ...
- Adding new control elements for Softkey and Parameter Selection.
- TCP is not possible in the moment, because there is no further development of the original CNMAT library. Maybe I use my own OSC library for sending data.

The library support hardware elements like encoders, fader, buttons with some helper functions. The library allows you to use hardware elements as an object and with the use of the helper functions, code becomes much easier to write and read and to understand. 
Please refer to the EOS manual for more information about OSC.

For use with PlatformIO https://platformio.org, as a recommanded IDE with MS VSCode, there is an extra start example folder called **#lighthack** and **#lighthack_ETH**.

If you have whishes for other functions or classes make an issue. If you find bugs also, nobody is perfect.

## USB configuration and initialization
Before using USB one things that must be extra done. You must add a constructor for init the library. This must done befor setup(), don't change the name of the constructor!
```
EOS eos;
```

## Ethernet configuration and initialization
The Ethernet functionality is now independent from the hardware port (e.g. WIFI or other Ethernet hardware than WizNet W5500) and libraries. Behind the scenes it uses the virtual Arduino UDP class.

Before using Ethernet there a some things that must be done. It can be different between the diverse libraries.
1. Import the necessary #defines e.g.
```
#include "Ethernet3.h"
#include "EthernetUdp3.h"
```
2. You need to define IP addresses and ports 

- **mac** - You need a unique MAC address, for Teensy you can use the TeensyID library on this GitHub site
- **localIP** - You need a static IP address for your Arduino in the subnet range of network system
- **subnet** - A subnet range is necessary
- **localPort** - This is the destinitaion port of your Arduino
- **eosIP** - This is the console IP address
- **eosPort** - This is the destination port of the EOS console
The EOS constructor name **eos** is fixed, don't change them!

```
// configuration example, must done before setup()
uint8_t mac[] = {0x90, 0xA2, 0xDA, 0x10, 0x14, 0x48};
IPAddress localIP(10, 101, 1, 201);
IPAddress subnet(255, 255, 0, 0);
uint16_t localPort = 8001; // on this port Arduino listen for data
// in EOS Setup > System > Showcontrol > OSC > OSC UDP TX Port
IPAddress eosIP(10, 101, 1, 100);
uint16_t eosPort = 8000; // on this port EOS listen for data
// in EOS Setup > System > Showcontrol > OSC > OSC UDP RX Port
```
3. You need an EOS and UDP constructor, must done befor setup(), don't change the name of the constructor
```
EthernetUDP udp;
EOS eos(udp, eosIP, eosPort);
```
4. In the beginning of setup() you must start network services.
```
Ethernet.begin(mac, localIP, subnet);
while(!udp.begin(localPort)); // wait for a connection
```

## Installation
1. Download from Releases
2. Follow the instruction on the Arduino website https://www.arduino.cc/en/Guide/Libraries
You can import the .zip file from the IDE with *Sketch / Include Library / Add .ZIP Library...*
3. For PlatformIO Unzip and move the folder to the lib folder of your project.

## Examples
### There are some examples for USB and Ethernet version

## box1
It is based on the original box1, no extras

## box1 ETH
It is based on the original box1, no extras. On an Arduino UNO you can't use the Pins 10 to 13 because they are needed for communication with the Ethernet Shield / Module.

## box2A
Is a modifcation of the original box1 and uses the former Next and Last buttons to step thru a parameter list.
Because of the limited RAM of the Arduino UNO, the parameter list is limited and depends on the size of the parameter names.
On an Arduino UNO the maximum is 14 parameter names.
On an Arduino MEGA more than 14 parameters can be used, also on other boards like Teensy 3.x or 4.
For Teensy you need to fake the PID/VID of the USB connection to work with a console, have a look to the forum.
Added a keyword „none“ for gaps in the parameter list, former titeled as „empty“.

## box2B
This box uses 6 buttons for Next, Last, SelectLast, Shift and Parameter Up/Down. It also uses the buttons of the encoder for posting the Home position. So, all Pins of an Arduino UNO are used.

## boxX ETH
This box is an example for Arduino MEGA with Ethernet Shield 2 and a large 20x4 LCD. It uses 6 buttons for Go, Back, SelectLast, Shift and Parameter Up/Down for stepping to a parameter list. Additional you have a Go/Back button and the LCD displays also cue informations. It also uses the buttons of the encoder for posting the Home position. It is a mixture between #lighthack box2B and cuebox for USB. But you can use more parameters and you have also endless pins on the Arduino MEGA

## faderwing
Is a small faderwing with 6 faders and 12 buttons for Stop / Go

## macrobox
Is a box with 12 buttons to fire Macros, actual 101 thru 112 but this can easily changed.

## cuebox
Is a simple box which shows you the active and pending cue and have buttons for GO and BACK/STOP. There is a parser for the cue text who gives you access to the different parts of the message.

## #lighthack
Is a project folder for use with PlatformIO and includes the box1 code as a starting point.

## #lighthack ETH
Is a project folder for use with PlatformIO and includes the box1 code as a starting point with the box X code.

## Helper functions

### **Filters**
```
void filter(String pattern);
```
With a Filter you can get messages from the console which you can use for proceeding informations.

### **Parameter Subscriptions**
```
void subscribe(String parameter);
void unSubscribe(String parameter); // unsubscribe a parameter
```
With subscribtions you can get special informations about dedicated parameters.

### **Ping**
```
void ping(); // send a ping without a message
void ping(String message); // send a ping with a message 
```

With a ping you can get a reaction from the console which helps you to indentify your box and if is alive. You should send a ping regulary with message to identify your box on the console.


### **Command Line**
```
void command(String cmd); // send a command
void newCommand(String newCmd); // clears cmd line before applyieng
```
You can send a string to the command line.

### **Users**
```
void user(int16_t userID);
```

This function allows you to change the user ID e.g. 
- **0** is a background user
- **-1** is the current user
- or any other user

### **Shift button**
```
void shiftButton(uint8_t pin);
```
This function allows you to assign a hardware button as a **Shift** button. **Shift** set the encoder and wheel messages to the **Fine** mode or for opposite the acceleration of the **Intens** parameter. It can replaced by using the optional encoder buttons as a **Shift** function.

### **Init faders**
```
void initFaders(uint8_t page = 1, uint8_t faders = 10, uint8_t bank = 1);
```
The **initFaders()** function is basic configuration and must use before you can use your Fader objects.
- **page** the fader page on your console
- **fader** is the number of fader on you console page
- **bank** is virtuell OSC fader bank

**initFaders();** without a parameter gives you a configuration for use your faders on page 1 of your console 

## Classes

### **EOS**
Eos is the main class and must init before you can use this library. The constructor name is fixed! You can use any UDP communication you want including WIFI. Beware of the different methods of initializing the interfaces.

**!!!** The class name **eos** is fixed and can’t changed in the moment **!!!**
```
EOS(UDP &udp, IPAddress ip, uint16_t port, interface_t interface = EOSUDP); // used for Ethernet
EOS(interface_t interface = EOSUSB); // used for USB
```
Example for Ethernet UDP, this should done before the setup()
```
EthernetUDP udp;
EOS eos(udp, eosIP, eosPort);
```
Example for USB
```
EOS eos;
```

### **sendOSC**
```
void EOS::sendOSC(OSCMessage& msg);
void EOS::sendOSC(OSCMessage& msg, IPAddress ip, uint16_t port);
```
It is a part of the EOS main class and send an OSC Message, depending on the choosen interface.
Example, this function can called inside setup() or loop() after init the interface.
```
eos.sendOSC(message);
eos.sendOSC(message, ip, port);
```

### **OscButton**
With this new class you can create generic buttons which allows you to control other OSC compatible software in the network like QLab. The class initializer is overloaded to allow sending different OSC data types: Interger 32 bit, Float, Strings or no data.

For USB
```
OscButton(uint8_t pin, String pattern, int32_t integer32);
OscButton(uint8_t pin, String pattern, float float32);
OscButton(uint8_t pin, String pattern, String message);
OscButton(uint8_t pin, String pattern);
```

For Ethernet UDP
```
OscButton(uint8_t pin, String pattern, int32_t integer32, IPAddress ip, uint16_t port);
OscButton(uint8_t pin, String pattern, float float32, IPAddress ip, uint16_t port);
OscButton(uint8_t pin, String pattern, String message, IPAddress ip, uint16_t port);
OscButton(uint8_t pin, String pattern, IPAddress ip, uint16_t port);
```
- **pin** the connection Pin for the button hardware
- **pattern** the OSC address pattern
- **integer32** optional Integer data to send
- **float32** optional Float data to send
- **message** optional String to send
- **ip** optional destination IP address
- **port** optional destination port address

Example for Ethernet UDP, this should done before the setup()
```
OscButton pingButton(8 , "/eos/ping", "hello EOS");
```
Example for Ethernet UDP, this should done before the setup()
```
IPAddress qlabIP(10, 101, 1, 101); // IP of QLab
uint16_t qlabPort = 53000; // QLab receive port
OscButton qlabGo(8 , "/go", qlabIP, qlabPort);
```
To get the actual button state you must call inside the loop():
```
void update();
```
Example, this must happen in the loop() 
```
qlabGo.update();
```

### **Encoder**
The Encoder class creates an encoder object which allows to control parameters:
```
Encoder(uint8_t pinA, uint8_t pinB, uint8_t direction = FORWARD);
```
- **pinA** and **pinB** are the connection Pins for the encoder hardware
- **direction** is used for changing the direction of the encoder to clockwise if pinA and pinB are swapped. The directions are FORWARD (standard) or REVERSE

Example, this should done before the setup()
```
Encoder encoder1(A0, A1, REVERSE);
```
If the Encoder have an extro button build in, you can add it with following class member:
```
void button(uint8_t buttonPin, ButtonMode buttonMode = HOME);
```
- **buttonPin** is the pin for the extra button
- **buttonMode** is the function you can assign, following functions are available

	- HOME will post a home command of the parameter
	- FINE allows you to use the button as a **Shift** function

Example, this must happen in the setup() before assigning a Parameter
```
encoder1.button(A3, HOME);
```
Before using you must assign a Parameter:
```
void parameter(String param); // set a parameter
String parameter(); // get the parameter name
```
- **param** is the Parameter which you want assign

Example, this must happen in the setup() before assigning an encoder button
```
encoder1.parameter("Pan");
```
To get the actual encoder state you must call inside the loop():
```
void update();
```
Example, this must happen in the loop() 
```
encoder1.update();
```

### **Wheel**
This class is similar to the Encoder class. It uses the wheel index instead a concrete Parameter. The Wheel index ist send by EOS with implicit OSC output “/eos/out/active/wheel/wheelIndex". You need a helper function to get the Wheel index and other information like parameter name and value. I will do a helper function later. With Wheel it is possible to handle dynamic parameter lists
```
Wheel(uint8_t pinA, uint8_t pinB, uint8_t direction  = FORWARD);
```
- **pinA** and **pinB** are the connection Pins for the encoder hardware
- **direction** is used for changing the direction of the encoder to clockwise if pinA and pinB are swapped. The directions are FORWARD or REVERSE

Example, this should done before the setup()
```
Wheel wheel1(A0, A1, REVERSE);
```
If the Encoder have an extro button build in, you can add it with following class member:
```
void button(uint8_t buttonPin, ButtonMode buttonMode = HOME);
```
- **buttonPin** is the pin for the extra button
- **buttonMode** is the function you can assign, following functions are available

	- only FINE is available, this allows you to use the button as a **Shift** function

Example, this must happen in the setup() before assigning a index
```
wheel1.button(A3, HOME);
wheel1.button(A3); // HOME is the standard mode
```
Before using you must assign a Wheel index:
```
void index(uint8_t idx); // set a Wheel index
uint8_t idx(); // get the actual index
```
- **param** is the Parameter which you want assign

Example, this must happen in the setup() before assigning an encoder button
```
wheel.index(1); // most time index 1 is the Intens parameter
```
To get the actual encoder state you must call inside the loop():
```
void update();
```
Example, this must happen in the loop()
``` 
wheel1.update();
```

### **Key**
With this class you can create Key objects which can be triggered with a button.
```
Key(uint8_t pin, String key);
```
- **pin** are the connection Pin for the button hardware
- **key** is the Key name, refer to the manual to get all possible Key words

Example, this should done before the setup()
```
Key next(8, "NEXT"); // "Next" button on pin 8
```
To get the actual button state you must call inside the loop():
```
void update();
```
Example, this must happen in the loop() 
```
next.update();
```

### **Macro**
With this class you can create Macro objects which can be triggered with a button.
```
Macro(uint8_t pin, uint16_t macro);
```
- **pin** are the connection Pin for the button hardware
- **macro** is the macro number you want to fire

Example, this should done before the setup()
```
Makro macro1(11, 101); // the button on pin 11 fires the macro
```
To get the actual button state you must call inside the loop():
```
void update();
```
Example, this must happen in the loop() 
```
macro1.update();
```

### **Submaster**
This class allows you to control a submaster with a hardware (slide) potentiometer as a fader.

The fader is a linear 10kOhm, from Bourns or ALPS and can be 45/60/100mm long. Put a 10nF ceramic capitor between ground and fader leveler to prevent analog noise.

**Additional Advices**

**Arduino UNO, MEGA**
use IOREF instead +5V to the top (single pin) of the fader (100%)
GND to the center button pin (2 pins, the outer pin is normaly for the leveler) of the fader (0%)

**TEENSY**
+3.3V to the top (single pin) of the fader (100%)
use ANALOG GND instead the normal GND to the center button pin (2 pins, the outer pin is normaly for the leveler) of the fader (0%)

```
Submaster(uint8_t analogPin, uint8_t firePin, uint8_t sub);
```
- **analogPin** are the connection Analog Pin for the fader leveler
- **firePin** is the Pin number for an additional bump button. Set to 0 if you don't need it.
- **sub** is the submaster number you want to control

Example, this should done before the setup()
```
Submaster submaster1(A5, 0, 1); // leveler is Analog Pin 5, no bump button, submaster 1 controlled
```
To get the actual button state you must call inside the loop():
```
void update();
```
Example, this must happen in the loop()
```
submaster1.update();
```

### **Fader**
This class allows you to control a fader containing two control buttons, all  functions configured in EOS Tab 36, with a hardware (slide) potentiometer as a fader and buttons. 
Before using Faders you must call **initFaders(page, faders, bank);**
For fader hardware see also Submaster.
```
Fader(uint8_t analogPin, uint8_t firePin, uint8_t stopPin, uint8_t fader, uint8_t bank = 1);
```
- **analogPin** are the connection Analog Pin for the fader leveler
- **firePin** is the Pin number for an additional fire button. Set to 0 if you don't need it.
- **stopPin** is the Pin number for an additional stop button. Set to 0 if you don't need it.
- **fader** is the fader number of the fader page you want to control
- **bank** is the internal OSC bank number (standard 1)

Example, this should done before the setup()
```
Fader fader1(A4, 12, 13, 3); // leveler is Analog Pin 4, fire button at Pin 12, stop button at Pin 13, fader number 3 is controlled
```
To get the actual button state you must call inside the loop():
```
void update();
```
Example, this must happen in the loop()
```
fader1.update();
```
