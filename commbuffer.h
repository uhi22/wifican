#pragma once
#include <Arduino.h>
#include "config.h"
#ifdef USE_CAN
  #include "esp32_can.h"
#endif

class CommBuffer
{
public:
    CommBuffer();
    size_t numAvailableBytes();
    uint8_t* getBufferedBytes();
    void clearBufferedBytes();
    #ifdef USE_CAN
    void sendFrameToBuffer(CAN_FRAME &frame, int whichBus);
    void sendFrameToBuffer(CAN_FRAME_FD &frame, int whichBus);
    #endif
    void sendBytesToBuffer(uint8_t *bytes, size_t length);
    void sendByteToBuffer(uint8_t byt);
    void sendString(String str);
    void sendCharString(char *str);

protected:
    byte transmitBuffer[WIFI_BUFF_SIZE];
    int transmitBufferLength; //not creating a ring buffer. The buffer should be large enough to never overflow
};