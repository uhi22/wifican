

/* Hint: The ESP32 wifi library sources are here:
 C:\Users\<username>\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.13\libraries\WiFi
 */

static IPAddress broadcastAddr(255,255,255,255);
#define ALIVEBUFFERSIZE 50
uint8_t alivebuffer[ALIVEBUFFERSIZE];
uint32_t aliveCounter;

void wifihandler_cyclic(void) {
    if (isWifiConnected && ((micros() - lastBroadcast) > 1000000ul) ) //every second send out a broadcast ping
    {
        uint8_t buff[4] = {0x1C,0xEF,0xAC,0xED};
        lastBroadcast = micros();
        Serial.println("Transmitting heartbeat");
        wifiUDPServer.beginPacket(broadcastAddr, 17222);
        wifiUDPServer.write(buff, 4);
        wifiUDPServer.endPacket();

        /* and also send an alive-message via telnet */
        wifihandler_sendAliveToTelnet();
    }


    if (wifiTelnetServer.hasClient()) {
                            telnetClient = wifiTelnetServer.available();
                                Serial.print("New client: ");
                                //Serial.print(i); Serial.print(' ');
                                Serial.println(telnetClient.remoteIP());
                                    //leds[SysSettings.LED_CONNECTION_STATUS] = CRGB::Blue;
                                    //FastLED.show();
    }
    if (telnetClient) {
      if (telnetClient.connected()) {
          /* get data from the telnet client and push it to input processing */
           while(telnetClient.available()) {
                                uint8_t inByt;
                                inByt = telnetClient.read();
                                //SysSettings.isWifiActive = true;
                                //Serial.println(inByt); /* echo to serial - just for debugging. Don't leave this on! */
                                wifiGVRET.processIncomingByte(inByt);
                            }
      }
    }
}

void wifihandler_sendBufferedData(void) {
        size_t wifiLength = wifiGVRET.numAvailableBytes();
        uint8_t* buff = wifiGVRET.getBufferedBytes();
        if (telnetClient && telnetClient.connected())
        {
            telnetClient.write(buff, wifiLength);
        }
    wifiGVRET.clearBufferedBytes();
}

void wifihandler_sendAliveToTelnet(void) {
  uint8_t i;
  uint16_t aliveCanID;
  i=0;
  aliveCanID = 0x1000; /* simulated CAN ID. Real IDs are in range 0 to 0x7FF. */
  #define whichBus 0

        alivebuffer[i++] = 0xF1;
        alivebuffer[i++] = 0; //0 = canbus frame sending
        uint32_t now = micros();
        alivebuffer[i++] = (uint8_t)(now & 0xFF);
        alivebuffer[i++] = (uint8_t)(now >> 8);
        alivebuffer[i++] = (uint8_t)(now >> 16);
        alivebuffer[i++] = (uint8_t)(now >> 24);
        alivebuffer[i++] = (uint8_t)(aliveCanID & 0xFF);
        alivebuffer[i++] = (uint8_t)(aliveCanID >> 8);
        alivebuffer[i++] = (uint8_t)(aliveCanID >> 16);
        alivebuffer[i++] = (uint8_t)(aliveCanID >> 24);
        alivebuffer[i++] = 8 + (uint8_t)(whichBus << 4);
        /* 8 data bytes */
        alivebuffer[i++] = (uint8_t)(aliveCounter>>24);
        alivebuffer[i++] = (uint8_t)(aliveCounter>>16);
        alivebuffer[i++] = (uint8_t)(aliveCounter>>8);
        alivebuffer[i++] = (uint8_t)(aliveCounter);
        alivebuffer[i++] = 0x55;
        alivebuffer[i++] = 0x66;
        alivebuffer[i++] = 0x77;
        alivebuffer[i++] = 0x88;
        alivebuffer[i++] = 0; /* checksum, but not used. */
        if (telnetClient && telnetClient.connected())
        {
            telnetClient.write(alivebuffer, i);
        }
        aliveCounter++;
}