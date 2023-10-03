#include "projectsettings.h"
#include <Arduino.h>
#include "can_manager.h"
#include "esp32_can_local.h"
#include "config.h"
//#include "SerialConsole.h"
#include "gvret_comm.h"
//#include "lawicel.h"
//#include "ELM327_Emulator.h"

CANManager::CANManager()
{

}

void CANManager::setup()
{

            CAN0.enable();
                CAN0.begin(settings.canSettings[0].nomSpeed, 255);
                Serial.printf("Enabled CAN%u with speed %u\n", 0, settings.canSettings[0].nomSpeed);

                CAN0.setListenOnlyMode(false);
            CAN0.watchFor();

        busLoad[0].bitsPerQuarter = settings.canSettings[0].nomSpeed / 4;
        busLoad[0].bitsSoFar = 0;
        busLoad[0].busloadPercentage = 0;
        if (busLoad[0].bitsPerQuarter == 0) busLoad[0].bitsPerQuarter = 125000;

    busLoadTimer = millis();
}

void CANManager::addBits(int offset, CAN_FRAME &frame)
{
    if (offset < 0) return;
    if (offset >= NUM_BUSES) return;
    busLoad[offset].bitsSoFar += 41 + (frame.length * 9);
    if (frame.extended) busLoad[offset].bitsSoFar += 18;
}



void CANManager::sendFrame(CAN_COMMON *bus, CAN_FRAME &frame)
{
    int whichBus = 0;
    //for (int i = 0; i < NUM_BUSES; i++) if (canBuses[i] == bus) whichBus = i;
    bus->sendFrame(frame);
    addBits(whichBus, frame);
}



void CANManager::displayFrame(CAN_FRAME &frame, int whichBus)
{
    //if (settings.enableLawicel && SysSettings.lawicelMode) 
    //{
        //lawicel.sendFrameToBuffer(frame, whichBus);
    //} 
    //else 
    {
        wifiGVRET.sendFrameToBuffer(frame, whichBus);
    }
}


void CANManager::loop()
{
    CAN_FRAME incoming;
    size_t wifiLength = wifiGVRET.numAvailableBytes();
    size_t maxLength = wifiLength;

    if (millis() > (busLoadTimer + 250)) {
        busLoadTimer = millis();
        busLoad[0].busloadPercentage = ((busLoad[0].busloadPercentage * 3) + (((busLoad[0].bitsSoFar * 1000) / busLoad[0].bitsPerQuarter) / 10)) / 4;
        //Force busload percentage to be at least 1% if any traffic exists at all. This forces the LED to light up for any traffic.
        if (busLoad[0].busloadPercentage == 0 && busLoad[0].bitsSoFar > 0) busLoad[0].busloadPercentage = 1;
        busLoad[0].bitsPerQuarter = settings.canSettings[0].nomSpeed / 4;
        busLoad[0].bitsSoFar = 0;
        if(busLoad[0].busloadPercentage > busLoad[1].busloadPercentage){
            //updateBusloadLED(busLoad[0].busloadPercentage);
        } else{
            //updateBusloadLED(busLoad[1].busloadPercentage);
        }
    }


        while ( (CAN0.available() > 0) && (maxLength < (WIFI_BUFF_SIZE - 80)))
        {
            if (settings.canSettings[0].fdMode == 0)
            {
                CAN0.read(incoming);
                addBits(0, incoming);
                displayFrame(incoming, 0);
            }
            else
            {
            }
            //toggleRXLED();
            //if ( (incoming.id > 0x7DF && incoming.id < 0x7F0) || elmEmulator.getMonitorMode() ) elmEmulator.processCANReply(incoming);
            wifiLength = wifiGVRET.numAvailableBytes();
            maxLength = wifiLength;
        }

}
