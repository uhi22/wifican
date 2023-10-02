


static IPAddress broadcastAddr(255,255,255,255);

void wifihandler_cyclic(void) {
    if (isWifiConnected && ((micros() - lastBroadcast) > 500000ul) ) //every second(?) send out a broadcast ping
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

}