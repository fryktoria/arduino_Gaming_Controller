#include "Arduino.h"
#include "ibus.h"

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

void IBus::end() {
  // write checksum
  buffer[offset++] = checksum & 0xff;
  buffer[offset++] = checksum >> 8;
  
  frameLength = offset;
  // Transmit the entire stream
  for (int i=0 ; i < frameLength; i++){
    Serial.write(buffer[i]);
  }

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
