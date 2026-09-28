#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

#include "us100.h"

#define US100_UART_NODE DT_ALIAS(us100_uart)

#if !DT_NODE_HAS_STATUS(US100_UART_NODE, okay)
#error "US-100 UART devicetree node not found, check that the overlay is applied"
#endif

static const struct device *const uart = DEVICE_DT_GET(US100_UART_NODE);

/* UART-mode commands (from the US-100 datasheet) */
#define US100_CMD_DISTANCE    0x55 /* reply: 2 bytes, distance in mm, high byte first */
#define US100_CMD_TEMPERATURE 0x50 /* reply: 1 byte, temperature + 45 */

#define US100_TEMP_OFFSET 45

#define DISTANCE_TIMEOUT_MS    150
#define TEMPERATURE_TIMEOUT_MS 50

/*
 * Received bytes are pushed here from the UART interrupt. The STM32
 * USART only holds one byte, so reading it in the ISR means no byte is
 * lost even if the radar thread is kept waiting by higher-priority
 * work (LVGL rendering on main, for example).
 */
K_MSGQ_DEFINE(rx_queue, sizeof(uint8_t), 8, 1);

static void uart_isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);
	uint8_t c;

	/*
	 * Latch the interrupt status before querying it. In this Zephyr
	 * version uart_irq_update() returns void (older versions returned
	 * int), so it is called without checking a result.
	 */
	uart_irq_update(dev);

	if (!uart_irq_rx_ready(dev)) {
		return;
	}

	while (uart_fifo_read(dev, &c, 1) == 1) {
		/* If the queue is full the byte is dropped; the reader will time out */
		(void)k_msgq_put(&rx_queue, &c, K_NO_WAIT);
	}
}

/* Discard anything left over from an earlier, late reply */
static void flush_rx(void)
{
	k_msgq_purge(&rx_queue);
}

static int read_bytes(uint8_t *buf, size_t len, int32_t timeout_ms)
{
	int64_t deadline = k_uptime_get() + timeout_ms;

	for (size_t i = 0; i < len; i++) {
		int64_t remaining = deadline - k_uptime_get();

		if (remaining <= 0 ||
		    k_msgq_get(&rx_queue, &buf[i], K_MSEC(remaining)) != 0) {
			return -ETIMEDOUT;
		}
	}

	return 0;
}

int us100_init(void)
{
	if (!device_is_ready(uart)) {
		printk("Error: US-100 UART not ready\n");
		return -ENODEV;
	}

	int ret = uart_irq_callback_user_data_set(uart, uart_isr, NULL);

	if (ret != 0) {
		printk("Error: US-100 UART interrupt setup failed (%d)\n", ret);
		return ret;
	}

	uart_irq_rx_enable(uart);
	flush_rx();
	return 0;
}

int us100_read_distance_mm(uint16_t *distance_mm)
{
	uint8_t reply[2];
	int ret;

	flush_rx();
	uart_poll_out(uart, US100_CMD_DISTANCE);

	ret = read_bytes(reply, sizeof(reply), DISTANCE_TIMEOUT_MS);
	if (ret != 0) {
		return ret;
	}

	*distance_mm = ((uint16_t)reply[0] << 8) | reply[1];

	if (*distance_mm < US100_MIN_RANGE_MM || *distance_mm > US100_MAX_RANGE_MM) {
		return -ERANGE;
	}

	return 0;
}

int us100_read_temperature_c(int *temp_c)
{
	uint8_t raw;
	int ret;

	flush_rx();
	uart_poll_out(uart, US100_CMD_TEMPERATURE);

	ret = read_bytes(&raw, 1, TEMPERATURE_TIMEOUT_MS);
	if (ret != 0) {
		return ret;
	}

	/* 0 and 0xFF are not valid readings */
	if (raw == 0 || raw == 0xFF) {
		return -EIO;
	}

	*temp_c = (int)raw - US100_TEMP_OFFSET;
	return 0;
}