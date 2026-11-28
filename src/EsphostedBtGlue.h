/*
   EsphostedBtGlue.h - core<->driver contract for Bluetooth HCI over the
   ESP-Hosted link.

   A host core that wants to run a Bluetooth stack on top of an ESP32
   hosted target implements the "core side" below and calls
   esphostedBtGlueInstall() from the places that reference the ESPHost
   driver (e.g. BLE begin and the WiFi driver's hardware bring-up). That
   call site pulls the driver-side glue (EsphostedBtGlue.cpp) into the
   link, which registers concrete operations for moving HCI packets over
   the ESP-Hosted link.  A core that provides nothing costs nothing: the
   glue is only linked when the core calls it.

   Core side to implement:

     void esphostedBtRegisterOps(const esphostedHciOps *ops);
         Store the ops table; the core's Bluetooth/link module calls
         through it.  bringUp() starts the ESP-Hosted link and returns
         once HCI traffic can flow; pump() runs one driver servicing pass;
         sendHci()/getHciRx() move HCI packets (H4 types: 0x01 command,
         0x02 ACL, 0x04 event).

     void esphostedBtLock(void);     [optional]
     void esphostedBtUnlock(void);   [optional]
         Taken around driver servicing during glue bring-up.  Provide
         them if the core services the driver from more than one
         execution context; otherwise the glue runs unlocked.  Provide
         both or neither.  (The arduino-pico core provides
         esphostedBtLock/Unlock under FreeRTOS and takes its async LWIP
         mutex directly in the no-OS build.)

   Enable by defining ESPHOST_BT (arduino-pico: board menu "Bluetooth
   over ESP-Hosted").

   Copyright (c) 2026 Pontus Oldberg

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _ESPHOSTED_BT_GLUE_H_
#define _ESPHOSTED_BT_GLUE_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esphostedHciOps {
    bool (*bringUp)(void);
    void (*pump)(void);
    int (*sendHci)(uint8_t packetType, uint8_t *packet, uint16_t size);
    uint16_t (*getHciRx)(uint8_t *packetType, uint8_t *buf, uint16_t bufsize);
} esphostedHciOps;

/* implemented by the core's Bluetooth/link module */
void esphostedBtRegisterOps(const esphostedHciOps *ops);

/* implemented by this library (EsphostedBtGlue.cpp), called by the core */
void esphostedBtGlueInstall(void);

#ifdef __cplusplus
}
#endif

#endif /* _ESPHOSTED_BT_GLUE_H_ */
