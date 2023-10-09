/*
 */

#include "projectsettings.h"
#include <WiFi.h>
#include "wifi_credentials.h"
#include "gvret_comm.h"
#include "can_common_local.h"
#include "esp32_can_local.h"
#include "can_manager.h"

const char* ssid     = WIFI_SSID; // Change this to your WiFi SSID
const char* password = WIFI_PASSWORD; // Change this to your WiFi password
#define WIFI_WIFICAN_SSID "wifican" /* In case we are the access point, use this WIFI SSID ... */
#define WIFI_WIFICAN_PASSWORD "topsecret" /* ... and WIFI password */

#define PIN_MODESWITCH_SUPPLY 47
#define PIN_TESTA 21
#define PIN_MODESWITCH_INPUT 35


EEPROMSettings settings;
uint8_t wifiMode;
#define WIFI_MODE_ACCESS_POINT 0 /* wifican acts as access point. It creates a new wifi network. */
#define WIFI_MODE_STATION 1 /* wifican acts as station. It connects to an existing wifi network. */

uint8_t isWifiConnected;
uint32_t lastBroadcast;
uint32_t lastFlushMicros;
uint32_t last1000ms;
WiFiUDP wifiUDPServer;
WiFiServer wifiTelnetServer;
WiFiClient telnetClient;
GVRET_Comm_Handler wifiGVRET; /* GVRET over the wifi telnet port */
CANManager canManager; /* keeps track of bus load and abstracts away some details of how things are done */

extern volatile uint32_t expCounterIsr;
extern volatile uint32_t expCounterMsg;
extern volatile uint32_t expCounterLostFrames;

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

    /* Setup the supply for the mode switch, and read the switch */
    pinMode(PIN_TESTA, OUTPUT);
    pinMode(PIN_MODESWITCH_SUPPLY, OUTPUT);
    digitalWrite(PIN_MODESWITCH_SUPPLY, HIGH);
    digitalWrite(PIN_TESTA, HIGH);
    delay(10);
    wifiMode = digitalRead(PIN_MODESWITCH_INPUT);
    Serial.print("wifiMode");
    Serial.println(wifiMode);
    digitalWrite(PIN_TESTA, LOW);
    delay(20);
    digitalWrite(PIN_TESTA, HIGH);
    delay(10);
    digitalWrite(PIN_TESTA, LOW);
    WiFi.onEvent(WiFiEvent);
    if (wifiMode == WIFI_MODE_STATION) {
      /*** We start by connecting to a WiFi network ***/
      Serial.println();
      Serial.println("******************************************************");
      Serial.print("WifiMode is 'station'. Connecting to ");
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
    } else {
      Serial.println();
      Serial.println("******************************************************");
      Serial.print("WifiMode is 'access point'.");
      WiFi.mode(WIFI_AP);
      WiFi.setSleep(true);
      WiFi.softAP((const char *)WIFI_WIFICAN_SSID, (const char *)WIFI_WIFICAN_PASSWORD);
      Serial.println("SoftAP created.");
               Serial.print("Wifi setup as SSID ");
                Serial.println(WIFI_WIFICAN_SSID);
                Serial.print("IP address: ");
                Serial.println(WiFi.softAPIP());      
      isWifiConnected = 0;
    }
    /**** setup a telnet server. We will be connected by e.g. Savvycan using telnet. */
    wifiTelnetServer.begin(23);
    wifiTelnetServer.setNoDelay(true);
    Serial.println("telnet is listening");
    delay(500);

    /***** Prepare settings *****/
    settings.canSettings[0].nomSpeed = 500000;
    settings.canSettings[0].enabled = true;
    settings.canSettings[0].listenOnly = false;
    //settings.canSettings[0].fdSpeed = 5000000;
    settings.canSettings[0].fdMode = false;    

    /***** Configure pins *****/
    //#ifdef USE_CAN
      Serial.println("CAN init..."); delay(500);
      CAN0.setDebuggingMode(1);
      CAN0.setCANPins(GPIO_NUM_4, GPIO_NUM_5);
      /* Initialize builtin CAN controller at the specified speed */
      Serial.println("CAN init2..."); delay(500);
      canManager.setup();
	    //if(CAN0.begin(500000))	{
		  //  Serial.println("Builtin CAN Init OK ...");
	    //} else {
		  //  Serial.println("BuiltIn CAN Init Failed ...");
	    //}
      //CAN0.setRXFilter(0, 0x100, 0x700, false);
	    //CAN0.watchFor(); //allow everything else through
	    //CAN0.setCallback(0, handleCAN0CB);

    //#endif

    Serial.println("init finished.");
    delay(500);

}


void loop(){
  wifihandler_cyclic();
  canManager.loop();

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
        //demoCanTransmit();
        Serial.print("debug: ");
        Serial.print(expCounterIsr);
        Serial.print(" ");
        Serial.print(expCounterMsg);
        Serial.print(" ");
        Serial.println(expCounterLostFrames);

        
    }

}

void WiFiEvent(WiFiEvent_t event){
    switch(event) {
        case ARDUINO_EVENT_WIFI_AP_START:
            Serial.println("AP Started");
            //WiFi.softAPsetHostname(AP_SSID);
            break;
        case ARDUINO_EVENT_WIFI_AP_STOP:
            Serial.println("AP Stopped");
            break;
        case ARDUINO_EVENT_WIFI_STA_START:
            Serial.println("STA Started");
            //WiFi.setHostname(AP_SSID);
            break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("STA Connected");
            WiFi.enableIpV6();
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
            Serial.print("STA IPv6: ");
            Serial.println(WiFi.localIPv6());
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.print("STA IPv4: ");
            Serial.println(WiFi.localIP());
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("STA Disconnected");
            break;
        case ARDUINO_EVENT_WIFI_STA_STOP:
            Serial.println("STA Stopped");
            break;
        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
            Serial.println("AP Station IP Assigned");
            isWifiConnected = true;
            break;
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            Serial.println("AP Station Disconnected");
            isWifiConnected = false;
            break;
        default:
            break;
    }
}