


static IPAddress broadcastAddr(255,255,255,255);

void wifihandler_cyclic(void) {
    if (isWifiConnected && ((micros() - lastBroadcast) > 1000000ul) ) //every second(?) send out a broadcast ping
    {
        uint8_t buff[4] = {0x1C,0xEF,0xAC,0xED};
        lastBroadcast = micros();
        Serial.println("Transmitting heartbeat");
        wifiUDPServer.beginPacket(broadcastAddr, 17222);
        wifiUDPServer.write(buff, 4);
        wifiUDPServer.endPacket();
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