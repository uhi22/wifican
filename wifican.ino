/*
 */

#include <WiFi.h>
#include "wifi_credentials.h"
#include "gvret_comm.h"

const char* ssid     = WIFI_SSID; // Change this to your WiFi SSID
const char* password = WIFI_PASSWORD; // Change this to your WiFi password

EEPROMSettings settings;
uint8_t isWifiConnected;
uint32_t lastBroadcast;
uint32_t lastFlushMicros;
WiFiUDP wifiUDPServer;
WiFiServer wifiTelnetServer;
WiFiClient telnetClient;
GVRET_Comm_Handler wifiGVRET; /* GVRET over the wifi telnet port */

void toggleRXLED()
{
}


void setup()
{
    Serial.begin(115200);
    while(!Serial){delay(100);}

    // We start by connecting to a WiFi network

    Serial.println();
    Serial.println("******************************************************");
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    isWifiConnected = 1;
    wifiTelnetServer.begin(23); //setup as a telnet server
    wifiTelnetServer.setNoDelay(true);
}


void loop(){
  wifihandler_cyclic();

    size_t wifiLength = wifiGVRET.numAvailableBytes();

    //If the max time has passed or the buffer is almost filled then send buffered data out
    if ((micros() - lastFlushMicros > SER_BUFF_FLUSH_INTERVAL) || (wifiLength > (WIFI_BUFF_SIZE - 40)) ) 
    {
        lastFlushMicros = micros();
        if (wifiLength > 0)
        {
            wifihandler_sendBufferedData();
        }
    }
}