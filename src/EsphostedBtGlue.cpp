/*
   EsphostedBtGlue.cpp - driver-side glue for ESP-Hosted Bluetooth

   Registers the CEspControl entry points a host core needs to drive
   Bluetooth HCI over the ESP-Hosted link (see EsphostedBtGlue.h for the
   core<->driver contract).

   This object lives in the ESPHost library on purpose: the core must not
   reference CEspControl directly, because a sketch that uses neither WiFi
   nor BLE never links this library and would fail to link.  The core only
   calls through the ops table; this file is pulled into the link by
   esphostedBtGlueInstall() calls from the core - i.e. exactly when the
   driver is present.  The glue itself is core agnostic: the only
   platform-specific part is the link lock taken during bring-up, see
   esphostedBtLinkLock below.

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

#ifdef ESPHOST_BT

#include <Arduino.h>
#include "CEspControl.h"
#include "EsphostedBtGlue.h"

/* Locking during glue bring-up: every pass into the driver must hold the
   core's link lock so other execution contexts (handshake GPIO IRQ,
   background servicing, other RTOS tasks) never run inside the driver
   concurrently.  The seam is per platform:

   - arduino-pico, no-OS: the async LWIP mutex (a core header), taken RAII
     style, is the lock.
   - everything else: the optional core hooks esphostedBtLock/Unlock, weak
     references so cores without them (single context, or no Bluetooth core
     side at all) link fine and run unlocked. */
#if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED) && !defined(__FREERTOS)
#include <lwip_wrap.h>
#else
extern "C" {
__attribute__((weak)) void esphostedBtLock(void);
__attribute__((weak)) void esphostedBtUnlock(void);
}
#endif

struct esphostedBtLinkLock {
#if defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED) && !defined(__FREERTOS)
    LWIPMutex m;
#else
    esphostedBtLinkLock()  { if (esphostedBtLock)   esphostedBtLock(); }
    ~esphostedBtLinkLock() { if (esphostedBtUnlock) esphostedBtUnlock(); }
#endif
};

static volatile bool s_linkUp = false;

static int esphostedInitEventCb(CCtrlMsgWrapper *resp) {
    (void) resp;
    s_linkUp = true;
    return ESP_CONTROL_OK;
}

static bool glueBringUp(void) {
    if (s_linkUp) {
        return true;
    }
    // CEspCbk keeps a single callback per event: the init-event slot belongs
    // to this glue whenever ESPHOST_BT is defined (the WiFi driver delegates
    // link bring-up to the core, which lands here).
    CEspControl::getInstance().listenForInitEvent(esphostedInitEventCb);
    if (CEspControl::getInstance().initSpiDriver() != 0) {
        return false;
    }
    uint32_t start = millis();
    while (!s_linkUp && (millis() - start < 10000)) {
        {
            esphostedBtLinkLock lk;
            CEspControl::getInstance().communicateWithEsp();
        }
        delay(100);
    }
    return s_linkUp;
}

static void gluePump(void) {
    CEspControl::getInstance().communicateWithEsp();
}

static int glueSendHci(uint8_t packetType, uint8_t *packet, uint16_t size) {
    int rv = CEspControl::getInstance().sendHciPacket(packetType, packet, size);
    return rv == ESP_CONTROL_OK ? 0 : -1;
}

static uint16_t glueGetHciRx(uint8_t *packetType, uint8_t *buf, uint16_t bufsize) {
    return CEspControl::getInstance().getHciRx(*packetType, buf, bufsize);
}

static const esphostedHciOps s_ops = {
    &glueBringUp,
    &gluePump,
    &glueSendHci,
    &glueGetHciRx,
};

extern "C" void esphostedBtGlueInstall(void) {
    esphostedBtRegisterOps(&s_ops);
}

#endif // ESPHOST_BT
