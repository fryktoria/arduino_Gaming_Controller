/*
ILIAS ILIOPOULOS 2025-05-13

Re-design of the ECLIPSE7 RC controller to provide 6 analog channels
and several buttons.
The analog and digital data are sent via iBus through the USB port. An application such as vJoySerialFeeder can decode
the data and forward to vJoy virtual joystick driver and subsequently to games.

A trim function is introduced to the analog joysticks, using the 8 trim buttons.
Note that those 8 trim buttons are also sent to iBus.
The application may ignore them or do whatever it likes

ibus code is based on https://github.com/Cleric-K/vJoySerialFeeder/tree/master/Arduino/Joystick.
I kept all of the comments in this example.
I modified the code to store channels in a static buffer and transmit all at the end.
This method has the disadvantage that it occupies a lot of precious RAM, but I plan
to re-use the ibus stream in the future, to transmit on other media.

PPM uses the PPMEncoder library https://github.com/schinken/PPMEncoder
I modified the code to fix a bug on the definition of int16_t channels[10], which 
needs to be volatile.

*/

#define VERSION "v1.0.1"

/*
  Reads configuration data from the EEPROM.
  If undefined, the code uses some pre-set boiler plate data 
*/
#define READ_FROM_EEPROM


/*
  The elevator, pitch and rudder D/R (dual role) buttons can be used 
  to invert the relevant axis. This feature is useful if there is no
  option to configure the device via display or bluetooth.

  If the inversion data are stored in the EEPROM, we may not need to use
  this feature. In that case, the buttons will be free to use for other purposes.
*/
//#define USE_DR_BUTTONS_TO_INVERT

/*
  A bluetooth module can be connected using SoftwareSerial
  to provide an interface to the user, to allow configuration 
  of the software, such as the inversion of axes.
*/
//#define USE_BLUETOOTH_MODULE

#ifdef USE_BLUETOOTH_MODULE
  #include <SoftwareSerial.h>
  // I set Arduino pin D11 as Tx and wire to HC-06 pin RX through the level converter.
  // Arduino pin D12 is set as Rx and is wired to HC-06 pin TX through the level converter.
  SoftwareSerial mySerial(12, 11); // RX, TX
  #define BLUETOOTH_BAUD_RATE 9600
#endif


/*
  If the upd7225 LCD display is available, there is an option to use it
*/
#define USE_DISPLAY


#include "ibus.h"
#include "matrix_keyboard.h"
#include <Wire.h>
#include "PPMEncoder.h" 
#include <EEPROM.h>


// //////////////////
// Edit here to customize

// 1. Analog channels. Data can be read with the Arduino's 10-bit ADC.
// This gives values from 0 to 1023.
// Specify below the analog pin numbers (as for analogRead()) you would like to use.
// Every analog input is sent as a single channel. The ADC 0-1023 range is mapped to the 1000-2000 range to make it more compatible with RC standards.

// example:
// byte analogPins[] = {A0, A1, A2, A3};
// This will send four analog channels for A0, A1, A2, A3 respectively

// Axis order is {Roll/ailerons, Pitch/elevators,  Throttle, Yaw/rudder,  left potentiometer, right potentiometer}
// The order matters both when sending to iBus and most important to PPM
byte analogPins[] = { A1, A0, A3, A2, A6, A7 };

// ////////////////
// Inversion mechanism

// If the value associated with the relevant channel is 1,
// the channel will be inverted, regardless of the state of any button
// Number of elements MUST be same as analogPins[]
// If READ_FROM_EEPROM has not been defined, the code
// will use this boiler-plate values.
// NOTE: Only 6 values are used for the analog channels, throughout the program 
// I just reserve space for 8 channels
byte forcedInvert[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

// In case forcedInvert is 0, 
// this defines a button which causes the inversion of the analog channel.
// If the setting is 0xFF, no inversion occurs
byte invertButton[] = { 27, 31, 0xFF, 32, 0xFF, 0xFF, 0xFF, 0xFF };


// NOTE: I do not use digital channels or Digital bit-mapped channels in the way
// it was implemented in the code of vJoySerialFeeder. In cases I need it,
// I use the state of the button reading the buttonState[] array. I keep the code for reference,
// or in case I decide to use a digital pin without being part of a keyboard

// 2. Digital channels. Data can be read from Arduino's digital pins.
// They could be either LOW or HIGH. The pins are set to use the internal pull-ups so your hardware buttons/switches should short them to GND.
// Specify below the digital pin numbers (as for digitalRead()) you would like to use.
// Every pin is sent as a single channel. LOW is the active state (when you short the pin to GND) and is sent as channel value 2000.
// HIGH is the pulled-up state when the button is open. It is sent as channel velue 1000.

// example:
// byte digitalPins[] = {2, 3};
// This will send two channels for pins D2 and D3 respectively

byte digitalPins[] = {};


// 3. Digital bit-mapped channels. Sending a single binary state as a 16-bit
// channel is pretty wasteful. Instead, we can encode one digital input
// in each of the 16 channel bits.
// Make sure you activate "use 16-bit channels" in the Setup dialog of the IBUS protocol in vJoySerialFeeder
// Specify below the digital pins (as for digitalRead()) you would like to send as
// bitmapped channel data. Data will be automatically organized in channels.
// The first 16 pins will go in one channel (the first pin goes into the LSB of the channel).
// The next 16 pins go in another channel and so on
// LOW pins are encoded as 1 bit, HIGH - as 0.

// example:
// byte digitalBitmappedPins[] = {4, 5, 6, 7, 8, 9};
// This will pack D4, D5, D6, D7, D8, D9 into one channel

byte digitalBitmappedPins[] = {};

#define ANALOG_INPUTS_COUNT sizeof(analogPins)
#define DIGITAL_INPUTS_COUNT sizeof(digitalPins)

/*
  WARNING: This functionality is specific to vJoySerialFeeder.
  It should NOT be used in any other implementation that uses iBus in its standard form.
  I do not use it at all.
*/
#define DIGITAL_BITMAPPED_INPUTS_COUNT sizeof(digitalBitmappedPins)

// /////////////////

// Define the appropriate analog reference source. See
// https://www.arduino.cc/reference/en/language/functions/analog-io/analogreference/
// Based on your device voltage, you may need to modify this definition
#define ANALOG_REFERENCE DEFAULT



// /////////////////
// Matrix keyboard definitions

// WARNING: Although the pins are defined below, writing and reading is performed through the
// PORTD and PORTB registers. DO NOT MODIFY.
// These definitions are used for setting the pins as Input/Output and to enable the input pullup resistors
byte rows[] = { D3, D4, D5, D6, D7 };        // Row pins of the matrix keyboard.
byte columns[] = { D8, D9, D10, D11, D12 };  // Column keys of the matrix keyboard

#define ROW_COUNT sizeof(rows)
#define COLUMN_COUNT sizeof(columns)
#define MATRIX_KEYBOARD_KEYS_COUNT (ROW_COUNT * COLUMN_COUNT)
// The number of rows associated with the trim keys
#define TRIM_ROW_COUNT 2
// The number of keyboard keys assigned to trim
#define TRIM_KEYS_COUNT (TRIM_ROW_COUNT * COLUMN_COUNT)
// We do not transmit the state of the trim keys which occupy the 10 first buttons (2 rows of the trim keyboard x 5 columns )
#define NO_TRIM_KEYBOARD_KEYS_COUNT (MATRIX_KEYBOARD_KEYS_COUNT - TRIM_KEYS_COUNT)

// /////////////////
// PCF8574 extender definitions - Reading of switches

#define REMOTE_EXTENDER_PIN_COUNT 8
#define PCF8574_ADDRESS 0x20

// I will map each of the extender pins as button, after the normal keyboard
byte buttonState[MATRIX_KEYBOARD_KEYS_COUNT + REMOTE_EXTENDER_PIN_COUNT];

// /////////////////
// Buttons mapped to PPM signal as analog axes
// In case I want a button to modify the relevant channel of the PPM output signal, 
// such as the Ch7 button of the ECLIPSE7 or the Clear button.
// See matrix_keyboard.h for the button codes
// ---------------------------------------------------------------------------------
// WARNING: THERE IS NO CHECK ON THE MAXIMUM NUMBER OF CHANNELS OF THE PPM STREAM
//          MAKE SURE THAT IT IS BELOW WHAT SUPPORTED BY THE PPMEncoder library (i.e. 8 channels)
// ---------------------------------------------------------------------------------
byte axisMappedButtons[] = { 25, 12 };
#define AXIS_MAPPED_BUTTONS_COUNT sizeof(axisMappedButtons)

// /////////////////
// IBUS
// How often to send data via iBus?
#define IBUS_UPDATE_INTERVAL 10  // milliseconds
// Define the baud rate for iBus
// in the communication via the USB port
#define IBUS_USB_BAUD_RATE 115200
// This definition is required for iBus
#define IBUS_NUM_CHANNELS ((ANALOG_INPUTS_COUNT) + (AXIS_MAPPED_BUTTONS_COUNT) + (DIGITAL_INPUTS_COUNT) + (15 + (DIGITAL_BITMAPPED_INPUTS_COUNT)) / 16) + NO_TRIM_KEYBOARD_KEYS_COUNT + REMOTE_EXTENDER_PIN_COUNT

// /////////////////
// Handling of axis trim

// An array to keep the offset per each axis
int axisOffset[ANALOG_INPUTS_COUNT];

// The step for each press of a trim button
#define TRIM_STEP 3  // 5 does a 1% at a press of the button

// The button numbers associated with an increase of the offset, per axis
// See matrix_keyboard.h for the assignment of buttons to button numbers
// Axis order is {Roll/ailerons, Pitch/elevetors,  Throttle, Yaw/rudder}
uint8_t trimButtonsIncrease[] = { BTN_TRIM_ROLL_RIGHT, BTN_TRIM_PITCH_UP, BTN_TRIM_THROTTLE_UP, BTN_TRIM_RUDDER_RIGHT };  // { 3, 2, 1, 0 };
uint8_t trimButtonsDecrease[] = { BTN_TRIM_ROLL_LEFT, BTN_TRIM_PITCH_DOWN, BTN_TRIM_THROTTLE_DOWN, BTN_TRIM_RUDDER_LEFT };  // { 8, 7, 6, 5 };
#define TRIMMED_AXES_COUNT sizeof(trimButtonsIncrease)

// /////////////////
// Keyboard debouncing

// Each axis is associated with a counter. Upon setting a trim button state to 1, the counter is initialized
// and decreased by one in every keyboard scan. Subsequent changes to the axis offset are allowed only after the counter goes to 0.
uint32_t axisDebounce[ANALOG_INPUTS_COUNT];
// Number of keyboard scans required to reset the debouncing counter
#define DEBOUNCE_LOOPS 20

// /////////////////
// PWM limits
#define PWM_MAX_WIDTH 2000
#define PWM_MIN_WIDTH 1000

// /////////////////
// ECLIPSE7 RC Controller
// The four main joysticks of Eclipse 7 do not reach 0 Ohm or maximum value of the potentiometer.
// Instead, the go from about 18% to 83% (values 160 to 850 instead of 0 to 1023 of the ADC)
// Setting JOYSTICK_FULL_RANGE, we map approximately to the full range 0 - 100%
#define JOYSTICK_FULL_RANGE

// When we want the range of the two joysticks (pitch, roll) not to be from 0% to 100%,
// but to a range e.g. from 20% to 80%, for the purpose of limiting the sensitivity,
// for example when flying in high speed, we can set the range
#define JOYSTICK_HIGH_SENSITIVITY_LOW 20 // in percentage
#define JOYSTICK_HIGH_SENSITIVITY_HIGH 80 // in percentage

// /////////////////
// Pin to be used as PPM output
#define TRAINER_OUTPUT_PIN 13

// /////////////////
// Calibration of the 4 axis sticks

// Number of axes subject to calibration
// We perform calibration only to the four sticks
#define AXIS_STICK_COUNT 4

// We store minimum, middle and maximum values for each axis
// We use this definition to avoid having a 3 all over the place!!!
#define CALIBRATION_VALUES_PER_AXIS 3
// Initial boiler-plate values of the ADC readings at min, middle and max point.
// I got this from a Calibration output. They are important only if the user
// does not perform calibration from the configuration menu.
uint16_t calibrationArray[] = {160, 512, 855, 174, 512, 838, 174, 512, 860, 166, 512, 865};

// /////////////////
// EEPROM

// The size of the EEPROM storage, in bytes.
// 6 bytes for the inversion flags, plus the 2-byte min,middle, max times 4 axes
#define EEPROM_STORAGE_SIZE (ANALOG_INPUTS_COUNT + AXIS_STICK_COUNT*CALIBRATION_VALUES_PER_AXIS*2*sizeof(uint8_t ))

// /////////////////
// Timeout values

// The time from power up until the hello message is sent to the Software serial
// of the bluetooth module.
// This time is necessary in order for the user to connect with a mobile app 
// to the bluetooth module
#define TIME_TO_HELLO 10 // seconds

// The time to wait for a user keypress after the hello, in order
// to enter configuration mode
#define TIME_TO_CONFIGURE 10 // seconds

// The time required to calibrate sticks to extents
#define TIME_FOR_CALIBRATION 10 // seconds

// /////////////////
// LCD 


#ifdef USE_DISPLAY

  #define LCD_EXTENDER_ADDRESS 0x21
  #include "upd7225.h"
  UPD7225 lcd(LCD_EXTENDER_ADDRESS, _EXT_SCK, _EXT_SI, _EXT_CD, _EXT_RESET, _EXT_CS, _EXT_BUSY);

// Menu variables
int menuIndex = 0;
const int menuItems = 5;
String menu[menuItems] = {"RUN", "INV", "CMID", "CAL", "STOR"};


const int menuButtons [] = {BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_INCREASE, BTN_DECREASE, BTN_CLEAR};

// The order of the buttons in the array above to use for debouncing each button, to reference by name instead of by index
#define BTN_UP_DBNC 0
#define BTN_DOWN_DBNC 1
#define BTN_LEFT_DBNC 2
#define BTN_RIGHT_DBNC 3
#define BTN_INCREASE_DBNC 4
#define BTN_DECREASE_DBNC 5
#define BTN_CLEAR_DBNC 6

uint32_t panelDebounce[sizeof(menuButtons)];
#define PANEL_DEBOUNCE_LOOPS 50

uint16_t channelNumber[] = {S_1, S_2, S_3, S_4, S_5, S_6, S_7, S_8};
uint16_t channelUpArrow[] = {S_ARROW_OVER_1, S_ARROW_OVER_2, S_ARROW_OVER_3, S_ARROW_OVER_4, S_ARROW_OVER_5, S_ARROW_OVER_6, S_ARROW_OVER_7, S_ARROW_OVER_8};
uint16_t channelDownArrow[] = {S_ARROW_UNDER_1, S_ARROW_UNDER_2, S_ARROW_UNDER_3, S_ARROW_UNDER_4, S_ARROW_UNDER_5, S_ARROW_UNDER_6, S_ARROW_UNDER_7, S_ARROW_UNDER_8};

#endif


// /////////////////
// Main code

IBus ibus(IBUS_NUM_CHANNELS);

void setup() {

  // Use the defined ADC reference voltage source
  // We want this very early because of the calibration
  analogReference(ANALOG_REFERENCE);

  // /////////////////
  // Configuration mode via bluetooth

#ifdef USE_BLUETOOTH_MODULE

  // Give the user some time from powering up, to connect
  // the bluetooth app
  delay(TIME_TO_HELLO * 1000);
  // Set the data rate for the SoftwareSerial port
  mySerial.begin(BLUETOOTH_BAUD_RATE);
  // Say hello via bluetooth module on SoftwareSerial
  btHello();

#endif

#ifdef READ_FROM_EEPROM

  // I place it after the SoftwareSerial initialization 
  // to print the values, for debugging purposes

  // Use the EEPROM to get any configuration parameters
  // and store to the relevant location
  populateForcedInvert();
  populateCalibrationData();

#endif  


#ifdef USE_BLUETOOTH_MODULE

  // Enter configuration mode
  btConfigurationMode();

  // Deactivate SoftwareSerial, to re-use pins for the keyboard
  mySerial.listen();

  // Finished configuration via Bluetooth

#endif

  // Activate pull-up resistors for input pins
  // Not used in ECLIPSE7 mod
  for (int i = 0; i < DIGITAL_INPUTS_COUNT; i++) {
    pinMode(digitalPins[i], INPUT_PULLUP);
  }

  // Activate pull-up resistors for bit-mapped pins
  // Not used in ECLIPSE7 mod
  for (int i = 0; i < DIGITAL_BITMAPPED_INPUTS_COUNT; i++) {
    pinMode(digitalBitmappedPins[i], INPUT_PULLUP);
  }

  /////// Keyboard settings

  // Make column input pins pullup
  for (uint8_t i = 0; i < sizeof(columns); i++) {
    pinMode(columns[i], INPUT_PULLUP);
  }

  // Make row pins output
  // Deactivate all row outputs by writing HIGH. Keyboard uses a negative logic.
  for (uint8_t i = 0; i < ROW_COUNT; i++) {
    pinMode(rows[i], OUTPUT);
    digitalWrite(rows[i], HIGH);
  }

  // Reset the variable holding the status of each button
  // The buttonState[] array uses a positive logic,
  //    0: not pressed
  //    1: button pressed
  for (uint8_t i = 0; i < MATRIX_KEYBOARD_KEYS_COUNT; i++) {
    buttonState[i] = 0;
  }

  // Set zero offset for every axis at the beginning
  for (uint8_t i = 0; i < ANALOG_INPUTS_COUNT; i++) {
    axisOffset[i] = 0;
  }

  // Start the I2C communication with the PCF8574 extender
  // We may have multiple extenders (e.g. for the display),
  // So we have to specify the address explicitly at the requests
  Wire.begin();

#ifdef USE_DISPLAY

  lcd.begin();
  lcd.clearDisplay();

  lcd.print14(VERSION);
  delay(2000);  
  lcd.print14("Helo");
  delay(3000);

  lcd.setSymbol(S_CH);
  lcd.setSymbol(S_N);
  lcd.setSymbol(S_R);

  // Turn on the channel numbers and display the current inversion state of each channel
  for (int i=0; i< ANALOG_INPUTS_COUNT; i++) {
    lcd.setSymbol(channelNumber[i]);
      forcedInvert[i] != 0 ? lcd.setSymbol(channelDownArrow[i]) : lcd.setSymbol(channelUpArrow[i]);
  }

  lcd.setBlinkSpeed(blinkSlow);

  unsigned long startCountdown = millis();

  while ((millis() - startCountdown) < (TIME_TO_CONFIGURE * 1000)) {
    lcd.print14("CONF");
    lcd.setSymbol(S_DELAY);
    scanKeyboardMatrix();
    if (buttonState[BTN_UP] == 1 || buttonState[BTN_DOWN] == 1 || buttonState[BTN_CLEAR] == 1) {     
      // Now that we can communicate with our environment and mainly the keyboard, it is time to 
      // configure via the LCD display
      lcd.print7("  "); // to delete the countdown value
      lcd.clearSymbol(S_DELAY);
      displayMenu();
      displayConfigure();
      // exit from while when we finish configuration
      break;
    }
    int previousRemainingSeconds = 9999;
    int remainingSeconds = ((TIME_TO_CONFIGURE * 1000) - (millis() - startCountdown)) / 1000;
    if (remainingSeconds != previousRemainingSeconds ) {
      lcd.printNumber7(remainingSeconds, false, false, false);
      previousRemainingSeconds = remainingSeconds;
    }
  } 
  // Continue with normal operation 
  lcd.clearDisplay();
  lcd.setSymbol(S_COLON);
  lcd.blinkSymbol(S_COLON);

#endif

  // Start the serial port to be used by iBus
  // WARNING: No Serial.print() FOR DEBUGGING IS ALLOWED IN THE CODE. THIS WILL MESS UP THE iBus STREAM
  Serial.begin(IBUS_USB_BAUD_RATE);

  // Start the PPM output on selected pin
  // We use the default number of channels
  ppmEncoder.begin(TRAINER_OUTPUT_PIN);
}


// /////////////////

void loop() {
  int i, bm_ch = 0;
  uint16_t adcValue;
  uint16_t xmitValue;
  unsigned long time = millis();
  int joystickSensitivityFactorLow;
  int joystickSensitivityFactorHigh;

  ibus.begin();

  // We read the remote extender first, because it contains a button that is assigned to PPM Channel 7.
  // We will send it as an axis signal, so we need to know its state together with the axes.
  // We map to buttons after the keyboard, hence the offset is the number of keyboard keys
  uint8_t remotePort = readExtender();
  assignExtenderPinsToButtons(remotePort, MATRIX_KEYBOARD_KEYS_COUNT);

  // read analog pins - one per channel
  for (i = 0; i < ANALOG_INPUTS_COUNT; i++) {

    adcValue = (uint16_t)analogRead(analogPins[i]) + axisOffset[i];

      // Adjustment with button Ftl Cond. to set the entire range of the pitch, roll and rudder joysticks between 20% and 80%.
      // This helps to do bigger movements.
      // It is essentialy the function dual-role D/R of the original ECLIPSE7 functionality, as 
      // activated by the three RC controller buttons. Here, I do it with a single button for the three axes, pitch, roll and yaw
      if ((buttonState[26] == 1) && ((i == 0) || (i == 1) || (i == 3))) {
        joystickSensitivityFactorLow = JOYSTICK_HIGH_SENSITIVITY_LOW;
        joystickSensitivityFactorHigh = JOYSTICK_HIGH_SENSITIVITY_HIGH;
      } else {
        joystickSensitivityFactorLow = 0;
        joystickSensitivityFactorHigh = 100;
      }

#ifdef JOYSTICK_FULL_RANGE
    // Only for the 4 joysticks. The two potentiometers go from 0 to 100% and no adjustment is required for them
    if (i < AXIS_STICK_COUNT) {
      
      // OBSOLETE. Instead of mapping the entire range, we will map the two segments, based on the calibrated middle point
      //xmitValue = PWM_MIN_WIDTH + map(adcValue, calibrationArray[i*CALIBRATION_VALUES_PER_AXIS + 0], calibrationArray[i*CALIBRATION_VALUES_PER_AXIS + 2], 0, PWM_MIN_WIDTH);  // map min-max to 1000-2000
      //xmitValue = PWM_MIN_WIDTH + map(adcValue, 0, 1023, 0, PWM_MIN_WIDTH);  // map min-max to 1000-2000

      if (adcValue < calibrationArray[i*CALIBRATION_VALUES_PER_AXIS + 1]) {
        xmitValue = PWM_MIN_WIDTH + map(adcValue, calibrationArray[i*CALIBRATION_VALUES_PER_AXIS + 0], calibrationArray[i*CALIBRATION_VALUES_PER_AXIS + 1], 0 + PWM_MIN_WIDTH * joystickSensitivityFactorLow/100, PWM_MIN_WIDTH/2); // Map lower half of stick to 1000-1500
      } else {
        xmitValue = PWM_MIN_WIDTH + map(adcValue, calibrationArray[i*CALIBRATION_VALUES_PER_AXIS + 1], calibrationArray[i*CALIBRATION_VALUES_PER_AXIS + 2], PWM_MIN_WIDTH/2, PWM_MIN_WIDTH * joystickSensitivityFactorHigh/100); // Map upper half of stick to 1500-2000
      }

    } else {
      // Non-sticks go all the way of the ADC dynamic range
      xmitValue = PWM_MIN_WIDTH + map(adcValue, 0, 1023, 0, PWM_MIN_WIDTH); // map 0-1023 to 1000-2000
    }
#else
    xmitValue = PWM_MIN_WIDTH + map(adcValue, 0, 1023, 0, PWM_MIN_WIDTH); // map 0-1023 to 1000-2000
#endif


    // Handle inverted channels
#ifdef USE_DR_BUTTONS_TO_INVERT

    if ((forcedInvert[i] != 0) ||  ((invertButton[i] < sizeof(buttonState) && (buttonState[invertButton[i]] == 1 ))))    {
        xmitValue = PWM_MIN_WIDTH + PWM_MAX_WIDTH - xmitValue;
    }

#else

    if (forcedInvert[i] != 0) {
        xmitValue = PWM_MIN_WIDTH + PWM_MAX_WIDTH - xmitValue;
    }

#endif

    // Make sure that regardless of the offset, we do not exceed the PWM limits
    xmitValue = constrain(xmitValue, PWM_MIN_WIDTH, PWM_MAX_WIDTH);

    // Set the value to ibus
    ibus.write(xmitValue);
    // ... and add into the PPM stream
    ppmEncoder.setChannel(i, xmitValue);
  }

  // Add into the PPM signal the axis-mapped buttons
  // NOTE: The buttons are sent into iBus, and also sent as regular buttons below.
  // Not really necessary but helps keep in mind a consistent model
  for (i = 0; i < AXIS_MAPPED_BUTTONS_COUNT; i++) {
    xmitValue = (buttonState[axisMappedButtons[i]] == 1) ? PWM_MAX_WIDTH : PWM_MIN_WIDTH;
    ibus.write(xmitValue);
    // Add into the PPM stream after the analog signals
    // WARNING: THERE IS NO CHECK ON THE MAXIMUM NUMBER OF CHANNELS OF THE PPM STREAM
    ppmEncoder.setChannel(i + ANALOG_INPUTS_COUNT, xmitValue);
  }

  // read digital pins - one per channel
  for (i = 0; i < DIGITAL_INPUTS_COUNT; i++) {
    ibus.write(digitalRead(digitalPins[i]) == LOW ? PWM_MAX_WIDTH : PWM_MIN_WIDTH);
  }


  // read digital bit-mapped pins - 16 pins go in one channel
  for (i = 0; i < DIGITAL_BITMAPPED_INPUTS_COUNT; i++) {
    int bit = i % 16;
    if (digitalRead(digitalBitmappedPins[i]) == LOW)
      bm_ch |= 1 << bit;

    if (bit == 15 || i == DIGITAL_BITMAPPED_INPUTS_COUNT - 1) {
      // data for one channel ready
      ibus.write(bm_ch);
      bm_ch = 0;
    }
  }


  /*
    From this point on, I will start transmit the values as collected in buttonState[] array.
    I start with the normal buttons of the matrix keyboard and then the 8 buttons of the extender
  */

  // Check the keyboard for pressed/released buttons
  scanKeyboardMatrix();
  // ... and send the data from the keyboard matrix
  // skipping the 8 trim keys which occupy the 10 first buttons (2 rows of the trim keyboard x 5 columns )
  int buttonIndex = TRIM_KEYS_COUNT;
  for (i = 0; i < NO_TRIM_KEYBOARD_KEYS_COUNT; i++) {
    ibus.write(buttonState[buttonIndex] == 1 ? PWM_MAX_WIDTH : PWM_MIN_WIDTH);
    ++buttonIndex;
  }

  // ... and send the data from the extender
  for (i = 0; i < REMOTE_EXTENDER_PIN_COUNT; i++) {
    ibus.write(buttonState[buttonIndex] == 0 ? PWM_MIN_WIDTH : PWM_MAX_WIDTH);
    ++buttonIndex;
  }

  // Handle analog axis trimming
  for (int axis = 0; axis < TRIMMED_AXES_COUNT; axis++) {
    if (buttonState[trimButtonsIncrease[axis]] == 1 && axisDebounce[axis] == 0) {
      axisOffset[axis] += TRIM_STEP;
      axisDebounce[axis] = DEBOUNCE_LOOPS;
    }
    if (buttonState[trimButtonsDecrease[axis]] == 1 && axisDebounce[axis] == 0) {
      axisOffset[axis] -= TRIM_STEP;
      axisDebounce[axis] = DEBOUNCE_LOOPS;
    }
  }

  // Update the debouncing loop counters
  handleTrimDebouncing();

  // and end the ibus packet
  ibus.end();

  time = millis() - time;  // time elapsed in reading the inputs
  if (time < IBUS_UPDATE_INTERVAL)
    // sleep till it is time for the next update
    delay(IBUS_UPDATE_INTERVAL - time);
}

/*
  Scans the output pins of the keyboard matrix by writing a 0 succesively in each 
  row pin and reads the column pins.
  Updates directly the buttonState[] array depending on the state of each button.
  Debouncing is not necessary for all pins, as we go continuously into a loop 
  and we do not generate events.
  A form of debouncing is only implemented for the trim buttons.

*/
void scanKeyboardMatrix() {
  int row;
  int column;

  // Rows are PORTD. I will write directly
  byte activeRow = 0b00001000;
  for (row = 0; row < ROW_COUNT; row++) {
    PORTD = (PORTD & 0b00000111) | (~activeRow & 0b11111000);

    // wait a bit before reading the row
    delay(2);
    byte columnValue = PINB & 0b00011111;

    // Assign the state of each button to the buttonState array
    byte mask = 0b00000001;
    for (column = 0; column < COLUMN_COUNT; column++) {
      int buttonIndex = row * ROW_COUNT + column;

      if ((mask & columnValue) == 0) {
        buttonState[buttonIndex] = 1;  // Button pressed
      } else {
        buttonState[buttonIndex] = 0;  // Button not pressed
      }

      mask = mask << 1;
    }
    activeRow = activeRow << 1;
  }
}


/*
  Supports the debouncing feature of the trim buttons
*/
void handleTrimDebouncing() {

  for (int axis = 0; axis < ANALOG_INPUTS_COUNT; axis++) {
    if (axisDebounce[axis] > 0) {
      --axisDebounce[axis];
    }
  }
}

/*
 Reads pins of remote extender PCF8584 and returns one byte with the current status
 Note that due to pull-up resistors, idle state of each pin is HIGH
*/
uint8_t readExtender() {

  uint8_t remotePins = 0xFF;
  Wire.requestFrom(PCF8574_ADDRESS, 1);
  if (Wire.available()) {
    remotePins = Wire.read();
  }
  return (remotePins);
}


/*
  Assign each of the pins of the extender PCF8574 into a button state
  and updates the button array, starting from the index set in buttonOffset
*/
void assignExtenderPinsToButtons(uint8_t value, int buttonOffset) {

  uint8_t mask = 0x00000001;
  for (uint8_t i = 0; i < 8; i++) {
    buttonState[buttonOffset + i] = ((value & mask) == 0 ? 1 : 0);
    mask = mask << 1;
  }
}


/*
  Populate the array that holds the invert flags
*/
void populateForcedInvert() {
    for (int axis=0; axis < ANALOG_INPUTS_COUNT; axis++) {
      forcedInvert[axis] = EEPROM.read(axis);
    }
}


/*
  Populates the array that stores the calibration data
*/
void populateCalibrationData() {
  // Calibration data are stored after the invert flags
  int address = ANALOG_INPUTS_COUNT;

  for (int axis = 0; axis < AXIS_STICK_COUNT; axis++) {
    calibrationArray[axis * CALIBRATION_VALUES_PER_AXIS + 0] = EEPROM.read(address) + 256 * EEPROM.read(address + 1);
    calibrationArray[axis * CALIBRATION_VALUES_PER_AXIS + 1] = EEPROM.read(address + 2) + 256 * EEPROM.read(address + 3);
    calibrationArray[axis * CALIBRATION_VALUES_PER_AXIS + 2] = EEPROM.read(address + 4) + 256 * EEPROM.read(address + 5);
    address += 6;
  }
}


/*
  Update EEPROM with the active configuration values
*/
void updateEEPROM(){

    int address = 0;

    // First, store the inversion flags
    for (int i = 0; i < ANALOG_INPUTS_COUNT; i++) {
      // Only if a change has occured, to avoid wear of the EEPROM storage
        if (forcedInvert[address] != EEPROM.read(address)) {
          EEPROM.write(address, forcedInvert[address]);          
        }
        address++;
    }

    // and then store the calibration values
    for (int i=0; i < sizeof(calibrationArray); i++) {
        
      uint8_t lowByte = calibrationArray[i] & 0xFF;
      uint8_t highByte = calibrationArray[i] >> 8;

      if (lowByte != EEPROM.read(address)) {
        EEPROM.write(address, lowByte); 
      }
      ++address;
      if (highByte != EEPROM.read(address)) {
        EEPROM.write(address, highByte); 
      }           
      address++;
    }
}


#ifdef USE_BLUETOOTH_MODULE

/*
  A welcome message, informing on the version of this code
*/
void btHello(){

  bluetoothPrint(F("\nFryktoria Joystick "));
  bluetoothPrint(F(VERSION));
  bluetoothPrintln();
  
}


/*
  The user interface to set the configuration
*/
void btConfigurationMode(){
  byte c;
  unsigned long startTime;

  bluetoothPrint(F("Press any button within "));
  bluetoothPrint(TIME_TO_CONFIGURE * 1000);
  bluetoothPrint(F(" seconds, to configure..."));
  bluetoothPrintln();
  startTime = millis();

  while (mySerial.available() <= 0) { 
    if ((millis() - startTime) > (TIME_TO_CONFIGURE * 1000)) {
      bluetoothPrint(F("Timeout.Entering Running mode."));
      bluetoothPrintln();
      return;
    }  
  }
 
  // Enter configuration mode  
  bluetoothPrint(F("Configuration mode"));
  bluetoothPrintln();
  c=0;
  while ((c != 'q') && (c != 'Q')){
    emptyInputStream();
    bluetoothPrint(F("\nPress:"));
    bluetoothPrintln();
    bluetoothPrint(F("  P: Show active configuration"));
    bluetoothPrintln();
    bluetoothPrint(F("  S: Show channel inversion flags"));
    bluetoothPrintln();
    bluetoothPrint(F("  I: Invert channel"));
    bluetoothPrintln();
    bluetoothPrint(F("  M: Set stick middle"));
    bluetoothPrintln();
    bluetoothPrint(F("  C: Calibrate stick edges"));
    bluetoothPrintln();
    bluetoothPrint(F("  E: Show EEPROM"));
    bluetoothPrintln();
    bluetoothPrint(F("  F: Show EEPROM formatted"));
    bluetoothPrintln();
    bluetoothPrint(F("  W: Save to EEPROM"));
    bluetoothPrintln();
    bluetoothPrint(F("  Q: Exit configuration mode"));
    bluetoothPrintln();
    // wait for a keypress
    emptyInputStream();
    while(mySerial.available() <= 0) {
    }
    
    if (mySerial.available() > 0) {
      c = mySerial.read();
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
        //mySerial.write(c);
        switch(c) {

          case 'p':
          case 'P':
            showActiveConfiguration();
            break;

          case 's':
          case 'S':
            showInversionStatus();
            break;

          case 'm':
          case 'M':
            setAxisMiddlePoint();
            break;

          case 'c':
          case 'C':
            calibrateAxisEdges();
            break;

          case 'e':
          case 'E':
            showEEPROM();
            break;

          case 'f':
          case 'F':
            showFormattedEEPROM();
            break;

          case 'w':
          case 'W':
            updateEEPROM();
            break;

          case 'i': // Invert
          case 'I':                                                                           
            showInversionStatus();
            emptyInputStream();
            int channelToInvert = selectChannel();
            if (channelToInvert > 0) {
              channelToInvert -= 1;
              emptyInputStream();
              forcedInvert[channelToInvert] == 0 ? forcedInvert[channelToInvert] = 1 : forcedInvert[channelToInvert] = 0;
            }  
            showInversionStatus();
            break;
        }
      }  
    }
    emptyInputStream();
  }
  bluetoothPrint(F("\nEntering Running mode."));
  bluetoothPrintln();

}


/*
  Request the user to select a channel.
  Here, channels are numbered from 1 to 6.
  A response 0, will exit the questionnaire.
*/
int selectChannel() {
  byte c;

  while (true) {
    bluetoothPrint(F("\nSelect channel (1-6, 0 to exit):")); 
    // Wait for a keypress
    while (mySerial.available() <= 0) {
    }

    if (mySerial.available() > 0) {    
      c = mySerial.read();
      if (c >= '0' && c <= '6') {
        return(c-48); // Return the number, not the ascii value
      }  
    }  
  }
}


/*
  Present the current active status of the analog channels inversion flags
*/
void showInversionStatus() {
  bluetoothPrint(F("Invert flags:\n"));
  for (int i = 0; i < ANALOG_INPUTS_COUNT; i++) {
    bluetoothPrint(forcedInvert[i]);
    bluetoothPrint(" ");
  }
  bluetoothPrintln();

}


/*
  Presents the full, currently active configuration data
*/
void showActiveConfiguration() {
  bluetoothPrint(F("\n Active configuration:\n"));

  showInversionStatus();

  bluetoothPrint(F("\n Calibration:\n"));
  for (int axis = 0; axis < AXIS_STICK_COUNT; axis++) {
    bluetoothPrint(axis);
    bluetoothPrint(" ");
    bluetoothPrint(calibrationArray[axis * CALIBRATION_VALUES_PER_AXIS + 0]);
    bluetoothPrint("-");
    bluetoothPrint(calibrationArray[axis * CALIBRATION_VALUES_PER_AXIS + 1]);
    bluetoothPrint("-");
    bluetoothPrint(calibrationArray[axis * CALIBRATION_VALUES_PER_AXIS + 2]);
    bluetoothPrintln();
  }  
}


/*
  Empty input buffer
*/ 
void emptyInputStream() {
  while (mySerial.available() > 0) { 
    byte c = mySerial.read();  
  }
}


/*
  Presents the content of the EEPROM in a byte form
*/
void showEEPROM() {
    bluetoothPrintln();
    bluetoothPrint(F("EEPROM data:"));
    bluetoothPrintln();
    for (int address=0; address<EEPROM_STORAGE_SIZE; address++) {            
      bluetoothPrint(address);
      bluetoothPrint(" ");
      bluetoothPrint(EEPROM.read(address));
      bluetoothPrintln();
    }  
}


/*
  Presents the content of the EEPROM in a human understandable form
*/
void showFormattedEEPROM() {

  int address = 0;

  bluetoothPrint(F("\n Inversion status:\n"));
  for (int i = 0; i < ANALOG_INPUTS_COUNT; i++) {
    bluetoothPrint(EEPROM.read(address));
    bluetoothPrint(" ");
    address++;
  }
  bluetoothPrintln();

  bluetoothPrint(F("\n Calibration:\n"));
  for (int axis = 0; axis < AXIS_STICK_COUNT; axis++) {
    bluetoothPrint(axis);
    bluetoothPrint(" ");
    bluetoothPrint(EEPROM.read(address) + 256 * EEPROM.read(address + 1));
    bluetoothPrint("-");
    bluetoothPrint(EEPROM.read(address + 2) + 256 * EEPROM.read(address + 3));
    bluetoothPrint("-");
    bluetoothPrint(EEPROM.read(address + 4) + 256 * EEPROM.read(address + 5));
    address += 6;
    bluetoothPrintln();
  }
}

#endif


#ifdef USE_DISPLAY

void displayConfigure() {

  while (true) { // We leave only when menu RUN is selected
    scanKeyboardMatrix();
    // Navigate the menu
    if (buttonState[BTN_UP] == 1 && panelDebounce[BTN_UP_DBNC] == 0) {
      menuIndex--;
      if (menuIndex < 0) {
        menuIndex = menuItems - 1;
      }
      displayMenu();
      panelDebounce[BTN_UP_DBNC] = PANEL_DEBOUNCE_LOOPS;
    }

    if (buttonState[BTN_DOWN] == 1 && panelDebounce[BTN_DOWN_DBNC] == 0) {
      menuIndex++;
      if (menuIndex >= menuItems) {
        menuIndex = 0;
      }
      displayMenu();
      panelDebounce[BTN_DOWN_DBNC] = PANEL_DEBOUNCE_LOOPS;
    }

    if (buttonState[BTN_CLEAR] == 1 && panelDebounce[BTN_CLEAR_DBNC] == 0) {
      panelDebounce[BTN_CLEAR_DBNC] = PANEL_DEBOUNCE_LOOPS;
      // Perform action based on the selected menu item
      if (menuIndex == 0) { // RUN end configuration
        lcd.print7("OK");
        delay(2000);
        lcd.print7("  ");
        return;

      } else if (menuIndex == 1) { // INV
        lcd.setSymbol(S_INPUT_SEL);
        setChannelReverse();
        lcd.clearSymbol(S_INPUT_SEL);    

      } else if (menuIndex == 2) { // CMID
        lcd.setSymbol(S_DELAY);
        setAxisMiddlePoint();
        lcd.clearSymbol(S_DELAY);
        lcd.print7("OK");
        delay(2000);
        lcd.print7("  ");  

      } else if (menuIndex == 3) { // CAL
        lcd.setSymbol(S_DELAY);
        calibrateAxisEdges();
        lcd.clearSymbol(S_DELAY);
        lcd.print7("OK");
        delay(2000);
        lcd.print7("  ");  

      } else if (menuIndex == 4) { // STOR
        updateEEPROM();
        lcd.print7("OK");
        delay(2000);
        lcd.print7("  ");
      } else {  // To add additional menu items

      }     
      displayMenu();
    }   
    handlePanelDebouncing();
  }
}


/*
  Dislays the active menu item
*/
void displayMenu() {
  lcd.print14(menu[menuIndex]);
}


/*
  Supports the debouncing feature of the panel buttons
*/
void handlePanelDebouncing() {

  for (int i = 0; i < sizeof(menuButtons); i++) {
    if (panelDebounce[i] > 0) {
      --panelDebounce[i];
    }
  }
}


/*
  Handles the user interaction for selecting an analog channel and set the normal or reverse 
  operation of the stick movement
*/
void setChannelReverse() {
  int channelIndex = 0;

  lcd.blinkSymbol(channelNumber[channelIndex]);

  while (true) { // We leave only when ENTER is pressed
    scanKeyboardMatrix();
    // Leave after ENTER is pressed
    if (buttonState[BTN_CLEAR] == 1 && panelDebounce[BTN_CLEAR_DBNC] == 0) {
      panelDebounce[BTN_CLEAR_DBNC] = PANEL_DEBOUNCE_LOOPS;
      lcd.noBlinkSymbol(channelNumber[channelIndex]);
      return;
    }  

    // Navigate the menu
    if (buttonState[BTN_LEFT] == 1 && panelDebounce[BTN_LEFT_DBNC] == 0) {
      lcd.noBlinkSymbol(channelNumber[channelIndex]);
      channelIndex--;
      if (channelIndex < 0) {
        channelIndex = ANALOG_INPUTS_COUNT - 1;
      }
      lcd.blinkSymbol(channelNumber[channelIndex]);
      panelDebounce[BTN_LEFT_DBNC] = PANEL_DEBOUNCE_LOOPS;
    }

    if (buttonState[BTN_RIGHT] == 1 && panelDebounce[BTN_RIGHT_DBNC] == 0) {
      lcd.noBlinkSymbol(channelNumber[channelIndex]);
      channelIndex++;
      if (channelIndex >= ANALOG_INPUTS_COUNT) {
        channelIndex = 0;
      }
      lcd.blinkSymbol(channelNumber[channelIndex]);
      panelDebounce[BTN_RIGHT_DBNC] = PANEL_DEBOUNCE_LOOPS;
    }

    if (buttonState[BTN_INCREASE] == 1 && panelDebounce[BTN_INCREASE_DBNC] == 0) {
      panelDebounce[BTN_INCREASE_DBNC] = PANEL_DEBOUNCE_LOOPS;
      if (forcedInvert[channelIndex] == 0) {
         forcedInvert[channelIndex] = 1;
         lcd.clearSymbol(channelUpArrow[channelIndex]);
         lcd.setSymbol(channelDownArrow[channelIndex]);  
      } else {
         forcedInvert[channelIndex] = 0;
         lcd.clearSymbol(channelDownArrow[channelIndex]);
         lcd.setSymbol(channelUpArrow[channelIndex]);

      }
    }
    handlePanelDebouncing();
  }  
}

#endif


#if defined(USE_DISPLAY) || defined(USE_BLUETOOTH_MODULE)

// Number of samples to read in order to calibrate 
// To store 10 values, it will go up to 10230, so 16 bits are fine. Even 60 samples are OK
#define AVERAGING_SAMPLE_COUNT 10

/*
  Adjust the calibration data to the current middle point of the sticks.
  WARNING: Some sticks, such as the Throttle, may not have a middle resting place. Set those sticks manually
*/
void setAxisMiddlePoint() {
  uint16_t average; 

  bluetoothPrint(F("\nSet sticks to middle point... "));
  bluetoothPrintln();
  bluetoothPrint("WATCH OUT THE THROTTLE STICK");
  bluetoothPrintln();
  for (uint8_t axis = 0; axis < AXIS_STICK_COUNT; axis++) {
      #ifdef USE_DISPLAY
        lcd.printNumber7(AXIS_STICK_COUNT - axis, false, false, false);
      #endif
    average = 0;
    for (uint8_t i=0; i<AVERAGING_SAMPLE_COUNT; i++){
      average += (uint16_t)analogRead(analogPins[axis]);

    }
    uint16_t middlePoint = average / AVERAGING_SAMPLE_COUNT;
    calibrationArray[axis * CALIBRATION_VALUES_PER_AXIS + 1] = middlePoint; 
    bluetoothPrint(F("Set middle ")); 
    bluetoothPrint(axis);
    bluetoothPrint(" ");
    bluetoothPrint(middlePoint);
    bluetoothPrintln();
  }
  bluetoothPrint(F("Middle point calibration complete \n"));
  bluetoothPrintln();
}

/*
  Calibrate the sticks to their full extent
*/
void calibrateAxisEdges() {
  bluetoothPrintln();
  bluetoothPrint(F("Move sticks to the extremes several times, for "));
  bluetoothPrint(TIME_FOR_CALIBRATION * 1000);
  bluetoothPrint(F(" seconds... "));
  bluetoothPrintln();
  
  // Since we are starting a new calibration, we reset the min and max of the calibration array to the middle
  // Note that the middle may have already been calibrated, so we use this value as starting point
  for (uint8_t axis = 0; axis < AXIS_STICK_COUNT; axis++) {
    calibrationArray[axis * CALIBRATION_VALUES_PER_AXIS + 0] = calibrationArray[axis * CALIBRATION_VALUES_PER_AXIS + 1];
    calibrationArray[axis * CALIBRATION_VALUES_PER_AXIS + 2] = calibrationArray[axis * CALIBRATION_VALUES_PER_AXIS + 1];
  }  

  unsigned long startCalibrate = millis();
  uint16_t adcValue;

  while ((millis() - startCalibrate) < (TIME_FOR_CALIBRATION * 1000)) {
    #ifdef USE_DISPLAY
      int previousRemainingSeconds = 9999;
      int remainingSeconds = ((TIME_FOR_CALIBRATION * 1000) - (millis() - startCalibrate)) / 1000;
      if (remainingSeconds != previousRemainingSeconds ) {
        lcd.printNumber7(remainingSeconds, false, false, false);
        previousRemainingSeconds = remainingSeconds;
      }
    #endif
    for (uint8_t axis = 0; axis < AXIS_STICK_COUNT; axis++) {
      adcValue = (uint16_t)analogRead(analogPins[axis]);
      if (adcValue < calibrationArray[axis * CALIBRATION_VALUES_PER_AXIS + 0]) {
        calibrationArray[axis * CALIBRATION_VALUES_PER_AXIS + 0] = adcValue;  
      } else if (adcValue > calibrationArray[axis * CALIBRATION_VALUES_PER_AXIS + 2]) {
        calibrationArray[axis * CALIBRATION_VALUES_PER_AXIS + 2] = adcValue;  
      }
    }
  }
  bluetoothPrint(F("Edges calibration complete "));
  bluetoothPrintln();

  for (uint8_t axis = 0; axis < AXIS_STICK_COUNT; axis++) {
    bluetoothPrint(axis);
    bluetoothPrint(" ");
    bluetoothPrint(calibrationArray[axis * CALIBRATION_VALUES_PER_AXIS + 0]);
    bluetoothPrint("-");
    bluetoothPrint(calibrationArray[axis * CALIBRATION_VALUES_PER_AXIS + 2]);
    bluetoothPrintln();
  }  
  bluetoothPrintln(); 
}


#endif


/*
  Wrapper functions to avoid having too many #ifdef in the code
*/
void bluetoothPrint(String message){
  #ifdef USE_BLUETOOTH_MODULE
    mySerial.print(message);
  #endif
}

void bluetoothPrint(int num){
  #ifdef USE_BLUETOOTH_MODULE
    mySerial.print(num);
  #endif  
}

void bluetoothPrint(float num){
  #ifdef USE_BLUETOOTH_MODULE
    mySerial.println(num);
  #endif  
}

void bluetoothPrint(uint16_t num){
  #ifdef USE_BLUETOOTH_MODULE
    mySerial.println(num);
  #endif  
}

void bluetoothPrintln(){
  #ifdef USE_BLUETOOTH_MODULE
    mySerial.println();
  #endif  
}