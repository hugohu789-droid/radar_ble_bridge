/*
 * Radar BLE bridge - Raspberry Pi Pico 2 W (CYW43439, BTstack)
 *
 * Receives radar frames from the Nucleo over UART1, validates them and
 * forwards them to a phone through the Radar Bridge GATT service.
 *
 * Threading: pico_cyw43_arch_none runs BTstack in the background
 * (async context driven by interrupts). The GATT service, its timer and
 * the UART line handler all run in that one context, so no locking is
 * needed. The UART interrupt only fills a ring buffer and wakes the
 * BTstack context. main() initialises everything and then idles.
 */

#include <cstdio>

#include "btstack.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "radar_service.h"
#include "uart_link.h"

int main()
{
    stdio_init_all();

    if (cyw43_arch_init() != 0) {
        printf("cyw43_arch_init failed\n");
        return -1;
    }

    l2cap_init();
    sm_init();

    radar_service::init();
    uart_link::init(radar_service::on_uart_line);

    hci_power_control(HCI_POWER_ON);

    while (true) {
        sleep_ms(1000);
    }
}
