#include "Arduino.h"
#include "ibus.h"

/*
ibus code is based on https://github.com/Cleric-K/vJoySerialFeeder/tree/master/Arduino/Joystick.
I kept all of the comments in this example.
I modified the code to store channels in a static buffer and transmit all at the end.
This method has the disadvantage that it occupies a lot of precious RAM, but I plan
to re-use the ibus stream in the future, to transmit on other media.
*/


/*
 * See IbusReader.cs for details about the IBUS protocol
 */

#define COMMAND40 0x40

IBus::IBus(int numChannels) {
  len = 4 + numChannels * 2;
}

void IBus::begin() {
  checksum = 0xffff - len - COMMAND40;
  offset = 0;
  buffer[offset++] = len & 0xff;
  buffer[offset++] = COMMAND40;
}

/*
  Completes the ibus packet by calculating
  and adding the checksum to the packet.

  Note: In older versions, transmission to 
  the serial port took place within this function.
  This functionality was moved because this part 
  of the code is not aware of any serial ports 
  or any other stream. Also to allow the main
  code to send the ibus packet to multiple
  destinations. This code should use the 
  public variable buffer[] to access the contents
  of the packet, and the public variable frameLength
  to know the length of the packet. 
*/
void IBus::end() {
  // write checksum
  buffer[offset++] = checksum & 0xff;
  buffer[offset++] = checksum >> 8;
  
  frameLength = offset;

}

/*
 * Write 16bit value in little endian format and update checksum
 */
void IBus::write(unsigned short val) {
  byte b = val & 0xff;
  buffer[offset++] = b;
  checksum -= b;
  b = val >> 8;
  buffer[offset++] = b;
  checksum -= b;
}
