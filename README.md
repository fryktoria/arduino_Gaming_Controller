# arduino_Gaming_Controller
A project to create a gaming joystick and a trainee RC controller, salvaging parts from an old RC transmitter

# Summary
The purpose of this project is to create a gaming joystick and a trainee RC controller, using a minimum number of parts, coming from an old RC controller. We want to use the joystick to play flight simulator games on PC. We also want to be able to connect to a working RC controller in the trainee mode, so that our joystick can be used to train new RC pilots, without having to use a fully functional RC controller with RF parts.

The device can be used as a Joystick connected to a PC via USB, transmitting an iBus encoded data stream, which is subsequently interpreted by [vJoySerialFeeder](https://github.com/Cleric-K/vJoySerialFeeder), which in turn feeds the [vJoy](https://github.com/jshafer817/vJoy) virtual joystick driver. It has been tested successfully with [GeoFS](https://geo-fs.com) (with Firefox browser) and  [FlightGear](https://www.flightgear.org/) flight simulators.

The device transmits a PPM signal with 8 channels onto the TRAINER_PORT pin of the trainer port socket. This signal can be routed to an RC controller operating at Master mode with a two-wire cable.

This article does not intend to be a step-by-step instructable, since you may have to take decisions depending on various parameters, such as the resources you intend to re-use, the capacity of the box to include the new components etc.

# History - boring but explains purpose and design decisions
My original purpose was to buy some joystick pads and a few buttons and build a joystick from scratch. But who can argue that any such custom solution cannot even come close to the perfection and ease of use of the sticks of an existing RC controller. Therefore, I decided to get an old, broken RC controller and utilize its  parts, along with a new MCU.

In this project, I happened to gain access to an ECLIPSE7 RC controller from [hitec ](https://www.hitecrcd.com). This is a 7-channel RC controller with several buttons and LCD display, where everything is controlled and operated by an MCU. It arrived to me with the information that something inside has blown out. The owner had purchased another RC controller and did not bother even to attempt its repair. Since my first target was to build a joystick to be used with flight simulator programs, I did not even consider to reverse engineer the RC controller and perhaps reuse the MCU and display, especially when it came with very bad credentials. Therefore, I stripped it from everything, keeping only the potentiometers and buttons. Using only the parts saves me from any claim of the manufacturer for intellectual property rights :-).

The methods described here, as well as the software could easily be modified to handle other types of RC controllers.

Originally, I intended to use an Arduino Pro Micro, so the joystick appears directly as an HID Joystick device. Unfortunately, my purchase was delayed, so this part of the project is still missing. In the future, I intend to feed the iBus channel stream via I2C to an Arduino Pro Micro, which in turn will appear as HID Joystick. This way, there will be no need to use the vJoy device driver and the vJoySerialFeeder.

## What we need
In order to fly virtual airplanes of flight simulators, we need at minimum:
 - Four (4) sticks to operate as controls of elevators/pitch, ailerons/roll, throttle, rudder/yaw
 - One joystick button. In order for a joystick to be recognized by the game software, usually a joystick button must be presses.
 - Software to make our PC think that it speaks to a joystick.
 In order to operate at a trainee mode, there are also some mandatory requirements:
 - The ability to trim the four sticks in order to set the center zone of the controls identical to the Master RC controller
 - The ability to invert the signal produced by the four sticks, so that e.g.  up/down movement of the stick can be translated by the controlled unit (i.e. the servo) to a down/up movement of the ailerons.
 - The electrical specifications of the trainee output signal to comply with the input of the RC controller which will operate as Master and will have the RF unit to operate the model airplane.

## What we have
 - Four (4) sticks, each working a potentiometer of approximately 5K.
 - Two potentiometers operated by knobs, each working a potentiometer of approximately 8.7K
 - 8 buttons which, in normal operation, where used for trimming each of the four sticks, i.e. two buttons per stick
 - Six (6) switches, each one working as SPDT (one center terminal, one NC normally-closed terminal and a NO normally-open terminal )
 - One switch with a common and two NO terminals. This switch has a center position where nothing is connected, one position where the common terminal is connected to one of the NO terminals and a second position where the common is connected to the other NO terminal
 - One switch, marked as Trainer. This is an SPDT switch which is spring loaded, can be drawn to one position but returns to the other position when released.
 - 9 push buttons, which were used to configure the RC controller.
 - An on-off slide switch
 - A 6-pin DIN45322-IEC-DIN EN 60130-9, 240 degree socket, which was used as trainer port.
 - A display based on NEC uPD72225, that we do not know if it works!
 - An Arduino Nano in the drawer.

## Mapping of requirements to what we have at hand
This is a mentally exhausting procedure, since we need to think (and re-think over and over) on possible ways we can utilize our assets to meet our design goals. The major drawback was that I had absolutely no previous experience with the operation of RC controllers, since I have never been an RC fan. Second drawback was that I could possibly implement more than I had planned, having all those switches and buttons at my disposal, considering that my short experience with simulator programs led me to appreciate the need for a multitude of buttons. I even found out that there is software to consolidate several joysticks, so that users can make use of more sticks and buttons than a single joystick can provide.

Let us examine each element, one by one:

### The four sticks
 - Although they are constructed with a gimbal mechanism, electrically they are plain potentiometers, so the task will be to connect the two edge terminals to Vcc and GND respectively, while the center pin will provide an output that can be read and interpreted by the Analog-to-Digital ports of an Arduino.
 - Those four potentiometers will be connected to the first four analog ports A0, A1, A2 and A3.
 - When I wired for the first time, I did not pay any particular order to the association between the stick and the order that the sticks need to be wired. For gaming, you can make a mapping between each axis and its function, so the order is not of any importance. Yet, to operate in the trainee mode, the order is important and follows rules crafted several years ago. The proper order of the four axes is :  ailerons/roll, elevators/pitch, throttle, rudder/yaw. The Arduino software will cope with the proper order, so you can make the wiring in any order is most suitable to your particular case.
 - There is a peculiarity in the operation of those four potentiometers. There range is not from 0% to 100% of the full resistance between the edge terminals. Yet, they go from approximately 18% to 83%, i.e. from 900 Ohm to 4150 Ohm. Probably there is some reason for that. In any case, if this is considered a "problem", the code can fix this, so that the movement of the axis in the PC can go from 0% to 100%.

### The two knob potentiometers
 - They have a different value than the potentiometers of the sticks, but we do not mind. Since the input impedance of the ATMega328 input pins is much higher, we only care about the ratio between the two parts of the potentiometer resistance. Long story short, we do not need to do any particular wiring for those potentiometers. We shall wire the center terminal of each to analog ports A6 and A7 respectively. We do not use A4 and A5, because these two pins operate the I2C protocol, which we need for other functionality.
 - The two knob potentiometers go full scale, from 0% to 100%, so we do not need to manipulate their range.

### The trim buttons
 - The RC controller has two PCBs, one with 4 buttons to trim the two sticks of Aileron and Pitch, and another PCB with 4 buttons to trim the sticks of Throttle and Rudder.
 - Each of the two boards is wired in a form that the four buttons present a 2x2 matrix.
 - To save our digital ports, we can wire the two boards by connecting the rows of each board, having finally a 2x4 matrix keyboard.

### The 9 push buttons
 - Since we will be forced to write the software to read a matrix keyboard for the mandatory trim buttons, why not expand our keyboard to include the 9 push buttons? These buttons are wired in a 3x5 matrix. One button occupies by itself one row and one column. This is something that we will use in our advantage in order to connect the Trainer switch.
 - We can think of combining the 2x2 matrix of the trim buttons with this 3x5 matrix and create a 5x5 matrix keyboard, that will handle all the push buttons of our controller.
 - The current design of the trim and push-button PCBs does not contain a diode in series with each button, that would avoid keyboard "ghosting". This means that if several buttons are pressed simultaneously, some button combinations may lead to the recognition of a different button. But in our case, we do not want to make extensive modifications to the existing PCB. In addition, we can assume that in contrast to switches, push buttons are only pressed for a short amount of time to perform a function and the possibility of having pressed several buttons at the same time is small.

### The six plus one switches
 - We may use those switches as additional channels (such as Channel 7 and 8), or to create the mandatory functionality of inverting a channel. Those switches can be positioned to one or the other place and remain there for  a considerable amount of time. Therefore, they cannot be mixed with the 5x5 matrix keyboard. We will read each switch by expanding the digital ports of the Arduino with a PCF8574 port extender.
 - Remember that one of the switches (SW5 in the schematic) has three states, an inactive center position, and two others. We will wire those two terminals at two different digital ports, covering fully all 8 ports of the PCF8574.

### The Trainer switch
 - We cannot use this switch for any "training" function, since our new device can only operate at Slave mode. Therefore, we can use this switch as a push button, due to its spring which returns it to the original position. Electrically, this gives us the opportunity to wire it in the 5x5 matrix keyboard.
 - We make use of Row 4, Column 3. Remember that Row 4, Column 4 is wired to one single button. So, by using Row 4 and a diode in the Trainer switch, we will not have the ghosting problem. Also notice that Row 4 is the only one of the row ports that is not connected to a diode, before going to the keyboard PCB. This is on purpose, because we do not want to ask a digital input port of ATMega328 to recognize the added forward voltage of two diodes as LOW, while at the same time there is no possibility of short-circuiting two output ports of our ATMega328, because the scanning of our keyboard will keep only one row at logical LOW, while all others will be HIGH.
 - Since we can use this as a push button, it can be used perfectly well as Brakes, if mapped to the proper button of your flight simulator. It is perfectly located in the left hand side, operated by the same hand the operates the Rudder during the landing procedure.

### The On-Off switch
 - Our device can be powered, either by the USB port when connected to  PC, or from a battery, when operating on field flying airplanes. We shall wire it in series with the battery, leading to the Vin port of Arduino Nano.

### The Trainer port
 - Since our device will operate only on Slave made, the PPM signal will be always be present on the DIN socket.
 - I tried to find a standard regarding the voltage of the PPM signal, with no success. I measured a peak-to-peak voltage of 3.3V at the output of one modern RC controller, whereas the same waveform had  a peak-to-peak voltage of 12.5V at the output of an older RC controller. Please make sure that you comply with the limits of the Master controller that you will connect this device.
 - In the schematic, I use a logic level converter to set the voltage to 3.3V. If your Master does not recognize this signal, you may have to use a higher voltage. You may use a zener diode to clip the voltage to your required level. PLEASE CHECK THE SPECIFICATIONS OF YOUR DEVICES. I CANNOT BE HELD RESPONSIBLE FOR ANY DAMAGE. Proceed at your own risk!
 - The signal is routed to a pin of a DIN45322-IEC-DIN EN 60130-9, 6-pin, 240degree socket. Again, I did not find a standard for wiring. Some controllers having this type of socket use this pin as output and the mirror symmetrical on the other side as input. Other controllers use one pin, which they configure as input or output in their settings. There is also a variety of sockets. Some controllers use a 3.5mm mono jack, others a stereo jack. As before,  I CANNOT BE HELD RESPONSIBLE FOR ANY DAMAGE. Proceed at your own risk!
 - The trainer port was tested at the risk of the owner of a FrSky Taranis QX7 RC controller. And it worked...

### The LCD display
 - The original LCD display was found to work fine. Although I did not consider it mandatory for the new design, we need to be able to invert some of the sticks. If we intend to use the joystick for a limited amount of uses or the software that we intend to use the joystick provides it own functionality for inversion, hardwiring could be an acceptable solution. But for using it as a trainee joystick, you need to have more flexibility in configuring the functionality. To use the original display, I had to write my own library. You can find this library in https://github.com/fryktoria/upd7225. You may enable or disable the display code using the pre-processor directive USE_DISPLAY.
### The bluetooth module
 - Before I found that the LCD display was working, I had the idea of setting the configuration parameters using an HC-06 bluetooth module, and to communicate with the Arduino board using a bluetooth terminal application running on a mobile phone. I used the "Serial Bluetooth Terminal" app. You may enable or disable the bluetooth code using the pre-processor directive USE_BLUETOOTH_MODULE.


### Software

#### Use as Joystick
I originally planned to use an Arduino Pro Micro, having an ATMega 32u4 MCU, which can be recognized by a PC as an HID device. My purchase order took quite some time and I did not have the patience to wait, so I used an Arduino Nano. Unfortunately, the USB drivers recognize the Nano as a COM port. This means that I need two additional pieces of software:
 - vJoy device driver. This is a driver for a virtual joystick. It can be found in several locations on the internet, some being legitimate forks of an original code, others being a bit suspicious. I used version 2.1.9.1 from https://github.com/jshafer817/vJoy
 - vJoySerialFeeder. This is a magnificent piece of software by Cleric-K, available from  https://github.com/Cleric-K/vJoySerialFeeder. It receives data from a serial COM port and feeds the vJoy device driver. It supports several encodings of data. It also offers functionality to assign virtual channels to signal channels and buttons, invert axes, calibrate axes etc. I used version 2.1.7.1, which is compatible with vJoy 2.1.9.1 on Windows 11. I found that 2.1.7.1 is not compatible with later versions of vJoy. The vJoySerialFeeder source code contains also an example of Arduino code, that I used in order to create an iBus encoded data stream.

#### Use as Slave trainee RC transmitter
I used as basis the PPMEncoder library by Christopher Schirner, version 0.4.0, available at https://github.com/schinken/PPMEncoder. I had to make several modifications, therefore the altered code is included in the final, not as a library but as independent source files.

# Schematics
After all the explanations above, the schematics should be self-explainable.
	- Potentiometers are wired to the Analog ports of Arduino Nano.
	- Switches are wired to the ports of the PCF8574 extender and read by Arduino through the I2C channel, using ports SCL and SDA. Pull up resistors are wired to each of the 8 PCF8574 ports. Also, two 4.7K resistors pull-up the SCL and SDA pins.
	- The functions of switch SW5 appears in the schematic as two switches, SW5-1 and SW5-2, because I could find in the drawing libraries a similar function.
	- The Interrupt port of PCF8574 is wired to port D2 of the Nano. Yet, the code does not currently make use of the interrupt feature. Perhaps it will be used in the future.

There are not enough pins to operate the LCD display. I used another PCF8574A, setting I2C device address to 0x21. The library https://github.com/fryktoria/upd7225 provides the option to control the display via I2C.

# Implementation
 - I attached the Arduino Nano on a perforated general purpose board, along with the rest of the components. I cut the board to the dimensions of the original LCD display, which I removed, leaving the real estate available for the board. _Update_: After I used the LCD display, I squeezed the board between the two sticks.
 - It would be nice to use the existing cables of the potentiometers, but unfortunately they were terminated on connectors with a pin distance of 2mm, which would make extremely difficult the attachment on the 0.1 inch/2.54mm board. Therefore, I de-soldered all wiring and added new wires. I used flat cables as much as possible.
 - I tried to follow as much as possible the natural flow of wiring inside the case. It is not important to attach any switch pin to a specific port, in respect to the pins of PCF8584. The software is perfectly capable to deal with these details.
 - In order to access the USB connector of the Nano inside the case, I used an extension cable, with a male USB mini connector going to the Nano, and a female USB mini socket at the other end. This socket was attached to the case, to be connected to a regular USB cable.
 
 ' - Try to remove as much as possible from unused items to make room for the new board. If it does not fit, you will have to use an external box, although things are becoming clumsy, due to the large number of cables. If possible, break down the schematic to the items that can be multiplexed via I2C, and carry only SDA, SCL and power out of the box to the main board.

# Instructions - with LCD
 - Load the sketch.
 - Configure according to your wishes the pre-processor options, as well as the user parameters.
 - Upload to the Arduino
T
he software version will appear briefly on the LCD, followed by a "Helo" message (well, only 4 characters are allowed...)
A countdown counter will appear. If you press the Up, Down or Clear buttons before the counter reaches zero, the software will enter configuration mode. Otherwise, it will enter normal operation.

## Configuration mode - LCD
After you enter configuration mode, you can navigate the menu options via the Up and Down keys. Press Clear to select any of the options.

 - RUN: Exits configuration mode and starts normal operation
 - INV: Starts the procedure to invert the operation of the channels. Use the Cursor Left and Right buttons to select a channel. The selected channel will start flashing. Press +Increase button to invert operation of the channel. If the arrow symbol is over the channel number, the operation is Normal. If the arrow is below the number, the operation is Reversed/inverted. Press Clear to end the channel selection process and go back to menu
 - CMID: Calibrate Middle point. Before selecting this option, set sticks to the middle position. Pay particulara attention to the Throttle stick, as this stick does not have a spring to place it automatically in the middle position.
 - CAL: Calibrate edges. A countdown timer will appear. You will have this time to move all sticks to the extreme positions (up-down-left-right). The software will use these positions as the minimum and maximu for each axis.
 - STOR: Stores the inversion status and calibration data to the internal non-volatile memory of the Arduino. If the READ_FROM_EEPROM pre-processor directive is active, the software will read the values from the non-volatile memory during the next start-up.
 
# Instructions - with Bluetooth module
 - Power-up the Arduino and Bluetooth.
 - Pair the Bluetooth module with your mobile. The password is usually 1234 or 0000.
 
 - Load the sketch.
 - Configure according to your wishes the pre-processor options, as well as the user parameters.
 - Upload to the Arduino

 - Start the Serial Bluetooth Terminal in your mobile and "Connect" to the bluetooth module
 - Send any character within the timeout period to enter configuration mode, or wait until normal operation starts
 
## Configuration mode - bluetooth terminal
This is an old-style, terminal based configuration, where you have to press a character on your terminal to initiate an operation. The available menu options are shown below:

  - P: Show active configuration
  - S: Show channel inversion flags
  - I: Invert channel 
  - M: Set stick middle
  - C: Calibrate stick edges
  - E: Show EEPROM
  - F: Show EEPROM formatted
  - W: Save to EEPROM
  - Q: Exit configuration mode
 

# Notes
 - [GeoFS](https://geo-fs.com) flight simulator does not work with vJoy when GeoFS runs on Chrome or Edge. Use Firefox, instead.
 - vJoySerialFeeder is supported under Linux, without vJoy which is Windows specific. Yet, vJoySerialFeeder requires 'mono' to run on Linux. It works, but mono consumes so many CPU resources that is practically unusable, at least in my case when run in a virtual machine.

# Attributions
The code that may be required to support the operation of this device is included in folder `attributions`, in its original form, both as sources as well as in binaries. A list of this software appears in section [[Software]] above. Please follow the license specifications of each of this software.
