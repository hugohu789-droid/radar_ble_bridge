#include "uart_link.h"

#include <atomic>

#include "btstack_run_loop.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/uart.h"

namespace uart_link {

namespace {

uart_inst_t *const kUart = uart1;
constexpr uint kTxPin = 4;
constexpr uint kRxPin = 5;
constexpr uint kBaudRate = 115200;

/*
 * Single-producer (UART IRQ) / single-consumer (BTstack context) ring
 * buffer. Power-of-two size so indices can wrap freely. At 115200 baud
 * this holds ~45 ms of data, far more than the BTstack poll latency.
 */
constexpr uint32_t kRingSize = 512;
static_assert((kRingSize & (kRingSize - 1)) == 0, "ring size must be a power of two");

uint8_t ring[kRingSize];
std::atomic<uint32_t> head{0}; /* written by the IRQ */
std::atomic<uint32_t> tail{0}; /* written by the consumer */
std::atomic<uint32_t> overflow_count{0};

/* Longest accepted line; radar frames are ~20 characters */
constexpr size_t kMaxLineLen = 64;
char line_buf[kMaxLineLen];
size_t line_len = 0;
bool line_cut = false;

LineHandler line_handler = nullptr;
btstack_data_source_t data_source;

void on_uart_irq()
{
    while (uart_is_readable(kUart)) {
        const uint8_t c = static_cast<uint8_t>(uart_getc(kUart));
        const uint32_t h = head.load(std::memory_order_relaxed);

        if (h - tail.load(std::memory_order_acquire) >= kRingSize) {
            overflow_count.fetch_add(1, std::memory_order_relaxed);
            continue;
        }

        ring[h % kRingSize] = c;
        head.store(h + 1, std::memory_order_release);
    }

    /* Wake the BTstack context to process the new bytes */
    btstack_run_loop_poll_data_sources_from_irq();
}

/* Runs in the BTstack context */
void process_rx(btstack_data_source_t *ds, btstack_data_source_callback_type_t type)
{
    (void)ds;
    (void)type;

    uint32_t t = tail.load(std::memory_order_relaxed);
    const uint32_t h = head.load(std::memory_order_acquire);

    while (t != h) {
        const char c = static_cast<char>(ring[t % kRingSize]);
        ++t;

        if (c == '\n') {
            size_t len = line_len;
            if (len > 0 && line_buf[len - 1] == '\r') {
                --len;
            }
            if (line_handler != nullptr && (len > 0 || line_cut)) {
                line_handler(std::string_view(line_buf, len), !line_cut);
            }
            line_len = 0;
            line_cut = false;
            continue;
        }

        if (line_len < kMaxLineLen) {
            line_buf[line_len++] = c;
        } else {
            line_cut = true;
        }
    }

    tail.store(t, std::memory_order_release);
}

} // namespace

void init(LineHandler handler)
{
    line_handler = handler;

    uart_init(kUart, kBaudRate);
    gpio_set_function(kTxPin, GPIO_FUNC_UART);
    gpio_set_function(kRxPin, GPIO_FUNC_UART);
    uart_set_hw_flow(kUart, false, false);
    uart_set_format(kUart, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(kUart, true);

    /* Register the consumer before enabling the interrupt that triggers it */
    btstack_run_loop_set_data_source_handler(&data_source, &process_rx);
    btstack_run_loop_enable_data_source_callbacks(&data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&data_source);

    const uint irq = UART_IRQ_NUM(kUart);
    irq_set_exclusive_handler(irq, on_uart_irq);
    irq_set_enabled(irq, true);
    uart_set_irqs_enabled(kUart, true, false);
}

void send(const char *data, size_t len)
{
    uart_write_blocking(kUart, reinterpret_cast<const uint8_t *>(data), len);
}

uint32_t rx_overflow_count()
{
    return overflow_count.load(std::memory_order_relaxed);
}

} // namespace uart_link
