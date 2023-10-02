# wifican


## How does a CAN transmission work?

- CANManager::sendFrame
- CAN_COMMON.sendFrame(...)
- ESP32CAN::sendFrame(CAN_FRAME& txFrame)
- twai_transmit()

The states and transitions of the ESP32S3 CAN driver are described here: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/twai.html

## How does the CAN reception work?

- During startup, the task task_LowLevelRX is started.
- The task_LowLevelRX suspends in twai_receive().
- If data is available, the task_LowLevelRX resumes and calls processFrame().
- ESP32CAN::processFrame() filters and distributes the data, using xQueueSend(callbackQueue, ...)
- The task_CAN reads the messages from the queue: xQueueReceive(callbackQueue, ...) and calls the ESP32CAN::sendCallback.
- The ESP32CAN::sendCallback either calls
    - one of the listeners (gotFrame()) or
    - a mailbox callback (cbCANFrame) or
    - a general callback (cbGeneral)


