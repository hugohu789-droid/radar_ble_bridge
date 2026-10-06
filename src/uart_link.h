#pragma once

/*
 * UART1 link to the Nucleo radar (GP4 = TX, GP5 = RX, 115200 8N1).
 *
 * Received bytes are stored in a ring buffer by the UART interrupt.
 * The interrupt then asks BTstack to poll its data sources, and the
 * bytes are split into lines inside the BTstack context. So the line
 * handler runs in the same context as all BLE code and may call
 * BTstack directly, with no locking.
 */

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace uart_link {

/*
 * Called once per received line, without CR/LF.
 * complete is false if the line overflowed the line buffer and was cut;
 * such a line should be counted as a bad frame.
 */
using LineHandler = void (*)(std::string_view line, bool complete);

/* Configure UART1 and its interrupt. Call after cyw43_arch_init(). */
void init(LineHandler handler);

/* Send raw bytes (blocking; only used for short commands) */
void send(const char *data, size_t len);

/* Bytes lost because the ring buffer was full */
uint32_t rx_overflow_count();

} // namespace uart_link
