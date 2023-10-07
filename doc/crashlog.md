
# Crash logs

## 2023-10-05: Crashlog after around 1740s tracing the foccci with SavvyCAN:

_Guru Meditation Error: Core  1 panic'ed (LoadProhibited). Exception was unhandled.

Core  1 register dump:
PC      : 0x4037fed3  PS      : 0x00060033  A0      : 0x8037ecc1  A1      : 0x3fc96440  
A2      : 0x0000001e  A3      : 0x3fc9c1a8  A4      : 0x00000004  A5      : 0x00060023  
A6      : 0x00060023  A7      : 0x00000001  A8      : 0x00000001  A9      : 0x00000001  
A10     : 0x00000001  A11     : 0x00000003  A12     : 0x3fca7c20  A13     : 0x3fca7bd0  
A14     : 0x02c96530  A15     : 0x00ffffff  SAR     : 0x00000018  EXCCAUSE: 0x0000001c  
EXCVADDR: 0x00000022  LBEG    : 0x40056fc5  LEND    : 0x40056fe7  LCOUNT  : 0xffffffff  


Backtrace: 0x4037fed0:0x3fc96440 0x4037ecbe:0x3fc96460 0x4037dd21:0x3fc96480 0x4200d3e7:0x3fc964b0 0x40378d6d:0x3fc96500 0x400559dd:0x3fca7b60 |<-CORRUPTED




ELF file SHA256: 05979fbcff467744

Rebooting...
�ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0xc (RTC_SW_CPU_RST),boot:0x8 (SPI_FAST_FLASH_BOOT)
Saved PC:0x4207d0ea

To analyze the stack trace:
c:
cd \esp-idf-tools\tools\xtensa-esp32s3-elf\esp-2022r1-11.2.0\xtensa-esp32s3-elf\bin

xtensa-esp32s3-elf-addr2line -e C:\Users\uwemi\AppData\Local\Temp\arduino\sketches\922ED37DD22A85A06A65C59BA36D769C\wifican.ino.elf 0x4037fed0:0x3fc96440 0x4037ecbe:0x3fc96460 0x4037dd21:0x3fc96480 0x4200d3e7:0x3fc964b0 0x40378d6d:0x3fc96500 0x400559dd:0x3fca7b60

...which says:
/Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/list.c:187
/Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/tasks.c:3657
/Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/queue.c:1128
/home/lucassvaz/esp-idf/components/driver/twai.c:131
/Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/port/xtensa/xtensa_vectors.S:1114
??:0


In C:\esp-idf-v4.4.4\components\driver\twai.c, line 131, would be the place where the message is queued:
xQueueSendFromISR(p_twai_obj->rx_queue, &frame, task_woken)

This leads to C:\esp-idf-v4.4.4\components\freertos\queue.c, function xQueueGenericSendFromISR(), which calls
   xTaskRemoveFromEventList()

This leads to
  C:\esp-idf-v4.4.4\components\freertos\tasks.c, line 3657, function xTaskRemoveFromEventList(), which
    calls uxListRemove()
    
This leads to C:\esp-idf-v4.4.4\components\freertos\list.c, line 187, function uxListRemove().

And just using the PC and A1:
xtensa-esp32s3-elf-addr2line -e C:\Users\uwemi\AppData\Local\Temp\arduino\sketches\922ED37DD22A85A06A65C59BA36D769C\wifican.ino.elf 0x4037fed3:0x3fc96440

leads to
/Users/ficeto/Desktop/ESP32/ESP32S2/esp-idf-public/components/freertos/list.c:192

which is
pxItemToRemove->pxNext->pxPrevious = pxItemToRemove->pxPrevious;

