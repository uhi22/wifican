/*
 */

#include "projectsettings.h"
#include <WiFi.h>
#include "wifi_credentials.h"
#include "gvret_comm.h"
#include "can_common_local.h"
#include "esp32_can_local.h"

const char* ssid     = WIFI_SSID; // Change this to your WiFi SSID
const char* password = WIFI_PASSWORD; // Change this to your WiFi password

EEPROMSettings settings;
uint8_t isWifiConnected;
uint32_t lastBroadcast;
uint32_t lastFlushMicros;
uint32_t last1000ms;
WiFiUDP wifiUDPServer;
WiFiServer wifiTelnetServer;
WiFiClient telnetClient;
GVRET_Comm_Handler wifiGVRET; /* GVRET over the wifi telnet port */

void toggleRXLED()
{
}

void demoCanTransmit(void) {
 CAN_FRAME txframe;
  txframe.data.byte[0]=0x11;
  txframe.data.byte[1]=0x22;
  txframe.data.byte[2]=0x33;
  txframe.data.byte[3]=0x44;
  txframe.data.byte[4]=0x55;
  txframe.data.byte[5]=0x66;
  txframe.data.byte[6]=0x77;
  txframe.data.byte[7]=0x88;
  txframe.id = 0x567;
  txframe.extended = false;
  txframe.rtr = 0;
  txframe.length = 8;
  CAN0.sendFrame(txframe);
  Serial.println("Demo CAN frame transmitted.");
}

void setup()
{
    Serial.begin(115200);
    while(!Serial){delay(100);}

    /* For debugging, show the task priority */
    Serial.print("Setup: priority = ");
    Serial.println(uxTaskPriorityGet(NULL));

    /*** We start by connecting to a WiFi network ***/

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
    /**** setup a telnet server. We will be connected by e.g. Savvycan using telnet. */
    wifiTelnetServer.begin(23);
    wifiTelnetServer.setNoDelay(true);
    Serial.println("telnet is listening");
    delay(500);

    /***** Configure pins *****/
    //#ifdef USE_CAN
      Serial.println("CAN init..."); delay(500);
      CAN0.setDebuggingMode(1);
      CAN0.setCANPins(GPIO_NUM_4, GPIO_NUM_5);
      /* Initialize builtin CAN controller at the specified speed */
      Serial.println("CAN init2..."); delay(500);
	    if(CAN0.begin(500000))	{
		    Serial.println("Builtin CAN Init OK ...");
	    } else {
		    Serial.println("BuiltIn CAN Init Failed ...");
	    }
      //CAN0.setRXFilter(0, 0x100, 0x700, false);
	    //CAN0.watchFor(); //allow everything else through
	    //CAN0.setCallback(0, handleCAN0CB);

    //#endif

    Serial.println("init finished.");
    delay(500);

}


void loop(){
  wifihandler_cyclic();

    size_t wifiLength = wifiGVRET.numAvailableBytes();

    /* If the max time has passed or the buffer is almost filled then send buffered data out */
    if ((micros() - lastFlushMicros > SER_BUFF_FLUSH_INTERVAL) || (wifiLength > (WIFI_BUFF_SIZE - 40)) ) 
    {
        lastFlushMicros = micros();
        if (wifiLength > 0)
        {
            wifihandler_sendBufferedData();
        }
    }

    if (micros() - last1000ms > 1000000uL ) {
        last1000ms = micros();
        /* this part runs once per second */
        demoCanTransmit();
    }

}