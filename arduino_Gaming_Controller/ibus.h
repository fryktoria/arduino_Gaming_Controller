#pragma once

class IBus {
  private:
    int len;
    int checksum;
    // An index to the buffer location to write the next byte    
    int offset;

  public:
    IBus(int numChannels);
  
    void begin();
    void end();
    void write(unsigned short);
    // I want the buffer to be public, to be able to re-use the ibus stream
    // Make sure that the buffer size can accomodate 4+numChannels*2 bytes.
    // 128 bytes support a max of 62 channels
    // There is no online check 
    byte buffer[128];
    int frameLength;
};

