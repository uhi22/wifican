/*
  ESP32_CAN.cpp - Library for ESP32 built-in CAN module
    Now converted to use the built-in TWAI driver in ESP-IDF. This should allow for support for the
    full range of ESP32 hardware and probably be more stable than the old approach.
  
  Author: Collin Kidder
  
  Created: 31/1/18, significant rework 1/5/23

  uhi22 / 2023-10-10 Completely reworked to NOT use the espressif TWAI handler with unreliable queues and lists. Instead
  implement the twai interrupt directly here and use a simple FIFO.
*/

#include "projectsettings.h"
#include "Arduino.h"
#include "esp32_can_builtin_local.h"

                                                                        //tx,         rx,           mode
twai_general_config_t twai_general_cfg = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_17, GPIO_NUM_16, TWAI_MODE_NORMAL);
twai_timing_config_t twai_speed_cfg = TWAI_TIMING_CONFIG_500KBITS();
twai_filter_config_t twai_filters_cfg = TWAI_FILTER_CONFIG_ACCEPT_ALL();

/*************************** begin experimental ***************************************/
#define EXPERIMENTAL_OWN_TWAI_DRIVER
#ifdef EXPERIMENTAL_OWN_TWAI_DRIVER
#include <soc/soc.h>
#include <soc/periph_defs.h> /* for ETS_TWAI_INTR_SOURCE */
#include <hal/twai_hal.h>
#include <driver/periph_ctrl.h>
#include <esp_rom_gpio.h>

#define DRIVER_DEFAULT_INTERRUPTS   0xE7        //Exclude data overrun (bit[3]) and brp_div (bit[4])

#ifdef CONFIG_TWAI_ISR_IN_IRAM
#define TWAI_ISR_ATTR       IRAM_ATTR
#define TWAI_MALLOC_CAPS    (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#else
#define TWAI_TAG "TWAI"
#define TWAI_ISR_ATTR
#define TWAI_MALLOC_CAPS    MALLOC_CAP_DEFAULT
#endif  //CONFIG_TWAI_ISR_IN_IRAM
intr_handle_t experimental_isr_handle;
static twai_hal_context_t twai_context;
volatile uint32_t expCounterIsr;
volatile uint32_t expCounterMsg;
volatile uint32_t expCounterLostFrames;
uint8_t experimentalDriverIsInitialized = 0;
#endif
/************************************* end experimental *********************************/

//because of the way the TWAI library works, it's just easier to store the valid timings here and anything not found here
//is just plain not supported. If you need a different speed then add it here. Be sure to leave the zero record at the end
//as it serves as a terminator
const VALID_TIMING valid_timings[] = 
{
    {TWAI_TIMING_CONFIG_1MBITS(), 1000000},
    {TWAI_TIMING_CONFIG_500KBITS(), 500000},
    {TWAI_TIMING_CONFIG_250KBITS(), 250000},
    {TWAI_TIMING_CONFIG_125KBITS(), 125000},
    {TWAI_TIMING_CONFIG_800KBITS(), 800000},
    {TWAI_TIMING_CONFIG_100KBITS(), 100000},
    {TWAI_TIMING_CONFIG_50KBITS(), 50000},
    {TWAI_TIMING_CONFIG_25KBITS(), 25000},
    //caution, these next entries are custom and haven't really been fully tested yet.
    //Note that brp can take values in multiples of 2 up to 128 and multiples of 4 up to 256
    //TSEG1 can be 1 to 16 and TSEG2 can be 1 to 8. There is a silent +1 added to the sum of these two.
    //The default clock is 80MHz so plan accordingly
    {{.brp = 100, .tseg_1 = 7, .tseg_2 = 2, .sjw = 3, .triple_sampling = false}, 80000}, 
    {{.brp = 120, .tseg_1 = 15, .tseg_2 = 4, .sjw = 3, .triple_sampling = false}, 33333},
    //this one is only possible on ECO2 ESP32 or ESP32-S3 not on the older ESP32 chips
    {{.brp = 200, .tseg_1 = 15, .tseg_2 = 4, .sjw = 3, .triple_sampling = false}, 20000},
    {TWAI_TIMING_CONFIG_25KBITS(), 0} //this is a terminator record. When the code sees an entry with 0 speed it stops searching
};

ESP32CAN::ESP32CAN(gpio_num_t rxPin, gpio_num_t txPin) : CAN_COMMON(32)
{
    twai_general_cfg.rx_io = rxPin;
    twai_general_cfg.tx_io = txPin;
    cyclesSinceTraffic = 0;
    initializedResources = false;
    twai_general_cfg.tx_queue_len = BI_TX_BUFFER_SIZE;
    twai_general_cfg.rx_queue_len = 6;
    rxBufferSize = BI_RX_BUFFER_SIZE;
}

ESP32CAN::ESP32CAN() : CAN_COMMON(BI_NUM_FILTERS) 
{
    twai_general_cfg.tx_queue_len = BI_TX_BUFFER_SIZE;
    twai_general_cfg.rx_queue_len = 6;

    rxBufferSize = BI_RX_BUFFER_SIZE;

    for (int i = 0; i < BI_NUM_FILTERS; i++)
    {
        filters[i].id = 0;
        filters[i].mask = 0;
        filters[i].extended = false;
        filters[i].configured = false;
    }
    initializedResources = false;
    cyclesSinceTraffic = 0;
}

void ESP32CAN::setCANPins(gpio_num_t rxPin, gpio_num_t txPin)
{
    twai_general_cfg.rx_io = rxPin;
    twai_general_cfg.tx_io = txPin;
}

void CAN_WatchDog_Builtin( void *pvParameters )
{
    ESP32CAN* espCan = (ESP32CAN*)pvParameters;
    const TickType_t xDelay = 200 / portTICK_PERIOD_MS;
    twai_status_info_t status_info;

    for(;;)
    {
        vTaskDelay( xDelay );
        espCan->cyclesSinceTraffic++;

        if (twai_get_status_info(&status_info) == ESP_OK)
        {
            if (status_info.state == TWAI_STATE_BUS_OFF)
            {
                espCan->cyclesSinceTraffic = 0;
                if (twai_initiate_recovery() != ESP_OK)
                {
                    printf("Could not initiate bus recovery!\n");
                }
            }
        }
    }
}

void ESP32CAN::setRXBufferSize(int newSize)
{
    rxBufferSize = newSize;
}

void ESP32CAN::setTXBufferSize(int newSize)
{
    twai_general_cfg.tx_queue_len = newSize;
}

int ESP32CAN::_setFilterSpecific(uint8_t mailbox, uint32_t id, uint32_t mask, bool extended)
{
    if (mailbox < BI_NUM_FILTERS)
    {
        filters[mailbox].id = id & mask;
        filters[mailbox].mask = mask;
        filters[mailbox].extended = extended;
        filters[mailbox].configured = true;
        return mailbox;
    }
    return -1;
}

int ESP32CAN::_setFilter(uint32_t id, uint32_t mask, bool extended)
{
    for (int i = 0; i < BI_NUM_FILTERS; i++)
    {
        if (!filters[i].configured) 
        {
            _setFilterSpecific(i, id, mask, extended);
            return i;
        }
    }
    if (debuggingMode) Serial.println("Could not set filter!");
    return -1;
}

#define CAN_RX_FIFO_SIZE 20
uint8_t iRxFifoWriteIndex = 0;
uint8_t iRxFifoReadIndex = 0;
twai_hal_frame_t aRxFifo[CAN_RX_FIFO_SIZE];

uint8_t ESP32CAN::isReceiveDataAvailable(void) {
  /* received data is available, if the write index and read index are different. */
  return (iRxFifoWriteIndex != iRxFifoReadIndex);
}

void ESP32CAN::consumeReceivedData(CAN_FRAME &msg)
{
  CAN_FRAME frame;
  if (iRxFifoWriteIndex != iRxFifoReadIndex) {
    frame.id = aRxFifo[iRxFifoReadIndex].standard.id[0]; /* high part of ID */
    frame.id <<= 8;
    frame.id += aRxFifo[iRxFifoReadIndex].standard.id[1]; /* low part of ID */
    frame.id >>= 5; /* The 11 bit ID is stored left-aligned in the driver. Make a normal ID 0 to 7ff out of it. */
    //Serial.printf("ID %04x\n", frame.id);
    Serial.print(".");
    frame.length = aRxFifo[iRxFifoReadIndex].dlc;
    memcpy(frame.data.uint8, aRxFifo[iRxFifoReadIndex].standard.data, 8);
    iRxFifoReadIndex=(iRxFifoReadIndex+1)%CAN_RX_FIFO_SIZE;
  }
  msg = frame;
}

void ESP32CAN::demo_checkForReceivedFramesAndConsume(void) {
  uint16_t id;
  if (iRxFifoWriteIndex != iRxFifoReadIndex) {
    id = aRxFifo[iRxFifoReadIndex].standard.id[0]; /* high part of ID */
    id <<= 8;
    id += aRxFifo[iRxFifoReadIndex].standard.id[1]; /* low part of ID */
    id >>= 5; /* The 11 bit ID is stored left-aligned in the driver. Make a normal ID 0 to 7ff out of it. */
    Serial.printf("ID %04x\n", id);
    //Serial.println(id, HEX);
    iRxFifoReadIndex=(iRxFifoReadIndex+1)%CAN_RX_FIFO_SIZE;
  }
}


void IRAM_ATTR experimentalInterrupt(void *arg) {
  uint32_t events;
  uint8_t nextIndex;
  expCounterIsr++;
   // original from twai_intr_handler_main()
  events = twai_hal_get_events(&twai_context);    //Get the events that triggered the interrupt
  if (events & TWAI_HAL_EVENT_RX_BUFF_FRAME) {
    uint32_t msg_count = twai_hal_get_rx_msg_count(&twai_context);
    for (uint32_t i = 0; i < msg_count; i++) {
        twai_hal_frame_t frame;
        if (twai_hal_read_rx_buffer_and_clear(&twai_context, &frame)) {
            expCounterMsg++;
            nextIndex = (iRxFifoWriteIndex+1) % CAN_RX_FIFO_SIZE;
            if (nextIndex!=iRxFifoReadIndex) {
              /* we still have space free. Add the frame to the FIFO. */
              memcpy(&(aRxFifo[iRxFifoWriteIndex]), &frame, sizeof(frame));
              iRxFifoWriteIndex=nextIndex;
            } else {
              /* no free space in FIFO. Discard the frame and count the lost frame. */
              expCounterLostFrames++;
            }
        }
    }
  }
}


static void twai_configure_gpio(gpio_num_t tx, gpio_num_t rx, gpio_num_t clkout, gpio_num_t bus_status)
{
    //Set TX pin
    gpio_set_pull_mode(tx, GPIO_FLOATING);
    esp_rom_gpio_connect_out_signal(tx, TWAI_TX_IDX, false, false);
    esp_rom_gpio_pad_select_gpio(tx);

    //Set RX pin
    gpio_set_pull_mode(rx, GPIO_FLOATING);
    esp_rom_gpio_connect_in_signal(rx, TWAI_RX_IDX, false);
    esp_rom_gpio_pad_select_gpio(rx);
    gpio_set_direction(rx, GPIO_MODE_INPUT);

    //Configure output clock pin (Optional)
    if (clkout >= 0 && clkout < GPIO_NUM_MAX) {
        gpio_set_pull_mode(clkout, GPIO_FLOATING);
        esp_rom_gpio_connect_out_signal(clkout, TWAI_CLKOUT_IDX, false, false);
        esp_rom_gpio_pad_select_gpio(clkout);
    }

    //Configure bus status pin (Optional)
    if (bus_status >= 0 && bus_status < GPIO_NUM_MAX) {
        gpio_set_pull_mode(bus_status, GPIO_FLOATING);
        esp_rom_gpio_connect_out_signal(bus_status, TWAI_BUS_OFF_ON_IDX, false, false);
        esp_rom_gpio_pad_select_gpio(bus_status);
    }
}

void experimentalInitOne(void) {
  Serial.println("ExpInitOne: 1"); Serial.flush();
  if (experimentalDriverIsInitialized) {
    Serial.println("experimental driver is already initialized. Nothing to do.");
    return;
  }
  periph_module_reset(PERIPH_TWAI_MODULE);
  periph_module_enable(PERIPH_TWAI_MODULE);            //Enable APB CLK to TWAI peripheral
  bool init = twai_hal_init(&twai_context);
  assert(init);
  (void)init;
  twai_hal_configure(&twai_context, &twai_speed_cfg, &twai_filters_cfg, DRIVER_DEFAULT_INTERRUPTS, twai_general_cfg.clkout_divider);
  //TWAI_EXIT_CRITICAL();

    //Allocate GPIO and Interrupts
    twai_configure_gpio(twai_general_cfg.tx_io, twai_general_cfg.rx_io, twai_general_cfg.clkout_io, twai_general_cfg.bus_off_io);  
  ESP_ERROR_CHECK(esp_intr_alloc(ETS_TWAI_INTR_SOURCE, twai_general_cfg.intr_flags, experimentalInterrupt, NULL, &experimental_isr_handle));
  Serial.println("ExpInitOne: 2"); Serial.flush();

  twai_hal_start(&twai_context, twai_general_cfg.mode);
  experimentalDriverIsInitialized = 1;
  Serial.println("ExpInitOne: 3"); Serial.flush();
}

void ESP32CAN::_init()
{
    if (debuggingMode) { Serial.println("Built in CAN Init in _init()"); Serial.flush(); }
    for (int i = 0; i < BI_NUM_FILTERS; i++)
    {
        filters[i].id = 0;
        filters[i].mask = 0;
        filters[i].extended = false;
        filters[i].configured = false;
    }
    if (debuggingMode) { Serial.println("Built in CAN Init Step 2"); Serial.flush(); }

}

uint32_t ESP32CAN::init(uint32_t ul_baudrate)
{
  Serial.println("Built in CAN Init in init with baudrate"); Serial.flush(); 
    _init();
    set_baudrate(ul_baudrate);
    if (debuggingMode)
    {
        //Reconfigure alerts to detect Error Passive and Bus-Off error states
        uint32_t alerts_to_enable = TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_OFF | TWAI_ALERT_AND_LOG | TWAI_ALERT_ERR_ACTIVE 
                                  | TWAI_ALERT_ARB_LOST | TWAI_ALERT_BUS_ERROR | TWAI_ALERT_TX_FAILED | TWAI_ALERT_RX_QUEUE_FULL;
        if (twai_reconfigure_alerts(alerts_to_enable, NULL) == ESP_OK)
        {
            printf("Alerts reconfigured\n");
        }
        else
        {
        printf("Failed to reconfigure alerts");
        }
    }
    return ul_baudrate;
}

uint32_t ESP32CAN::beginAutoSpeed()
{
    twai_general_config_t oldMode = twai_general_cfg;

    _init();

    twai_stop();
    twai_general_cfg.mode = TWAI_MODE_LISTEN_ONLY;
    int idx = 0;
    while (valid_timings[idx].speed != 0)
    {
        twai_speed_cfg = valid_timings[idx].cfg;
        disable();
        Serial.print("Trying Speed ");
        Serial.print(valid_timings[idx].speed);
        enable();
        delay(600); //wait a while
        if (cyclesSinceTraffic < 2) //only would happen if there had been traffic
        {
            disable();
            twai_general_cfg.mode = oldMode.mode;
            enable();
            Serial.println(" SUCCESS!");
            return valid_timings[idx].speed;
        }
        else
        {
            Serial.println(" FAILED.");
        }
        idx++;
    }
    Serial.println("None of the tested CAN speeds worked!");
    twai_stop();
    return 0;
}

uint32_t ESP32CAN::set_baudrate(uint32_t ul_baudrate)
{
    disable();
    //now try to find a valid timing to use
    int idx = 0;
    while (valid_timings[idx].speed != 0)
    {
        if (valid_timings[idx].speed == ul_baudrate)
        {
            twai_speed_cfg = valid_timings[idx].cfg;
            Serial.println("In set_baudrate() will call enable().");
            enable();
            return ul_baudrate;
        }
        idx++;
    }
    printf("Could not find a valid bit timing! You will need to add your desired speed to the library!\n");
    return 0;
}

void ESP32CAN::setListenOnlyMode(bool state)
{
    disable();
    twai_general_cfg.mode = state?TWAI_MODE_LISTEN_ONLY:TWAI_MODE_NORMAL;
    enable();
}

void ESP32CAN::enable()
{
  experimentalInitOne();
}

void ESP32CAN::disable()
{
    twai_stop();
    //vTaskDelay(pdMS_TO_TICKS(100)); //a bit of delay here seems to fix a race condition triggered by task_LowLevelRX
    //twai_driver_uninstall();
}

bool ESP32CAN::sendFrame(CAN_FRAME& txFrame)
{
    twai_message_t __TX_frame;

    __TX_frame.identifier = txFrame.id;
    __TX_frame.data_length_code = txFrame.length;
    __TX_frame.rtr = txFrame.rtr;
    __TX_frame.extd = txFrame.extended;
    for (int i = 0; i < 8; i++) __TX_frame.data[i] = txFrame.data.byte[i];

    //don't wait long if the queue was full. The end user code shouldn't be sending faster
    //than the buffer can empty. Set a bigger TX buffer or delay sending if this is a problem.
    switch (twai_transmit(&__TX_frame, pdMS_TO_TICKS(4)))
    {
    case ESP_OK:
        if (debuggingMode) Serial.write('<');
        break;
    case ESP_ERR_TIMEOUT:
        if (debuggingMode) Serial.write('T');
        break;
    case ESP_ERR_INVALID_ARG:
    case ESP_FAIL:
    case ESP_ERR_INVALID_STATE:
    case ESP_ERR_NOT_SUPPORTED:
        if (debuggingMode) Serial.write('!');
        break;
    }
    
    return true;
}


