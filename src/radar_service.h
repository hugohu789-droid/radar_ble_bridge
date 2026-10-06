#pragma once

/*
 * Radar Bridge GATT service.
 *
 * Owns advertising, the ATT server callbacks and the three
 * characteristics (Radar Data, Alarm Threshold, Link Status).
 * Everything here runs in the BTstack context.
 */

#include <string_view>

namespace radar_service {

/* Set up the ATT server, event handlers and periodic timer.
 * Call after l2cap_init()/sm_init() and before hci_power_control(). */
void init();

/* Feed one line received from the Nucleo (see uart_link::LineHandler) */
void on_uart_line(std::string_view line, bool complete);

} // namespace radar_service
