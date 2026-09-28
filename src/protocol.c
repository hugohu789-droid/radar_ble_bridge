#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

#include "protocol.h"

/*
 * Send on the dedicated PC link (CH340 on UART4) when the overlay
 * defines it, otherwise fall back to the console UART.
 */
#if DT_NODE_HAS_STATUS(DT_ALIAS(radar_link), okay)
#define PROTOCOL_UART_NODE DT_ALIAS(radar_link)
#else
#define PROTOCOL_UART_NODE DT_CHOSEN(zephyr_console)
#endif

static const struct device *const out_uart = DEVICE_DT_GET(PROTOCOL_UART_NODE);

K_MUTEX_DEFINE(tx_lock);

static uint8_t checksum(const char *s, int len)
{
	uint8_t cs = 0;

	for (int i = 0; i < len; i++) {
		cs ^= (uint8_t)s[i];
	}

	return cs;
}

static void send_line(const char *line)
{
	k_mutex_lock(&tx_lock, K_FOREVER);

	for (const char *p = line; *p != '\0'; p++) {
		uart_poll_out(out_uart, *p);
	}

	k_mutex_unlock(&tx_lock);
}

int protocol_init(void)
{
	if (!device_is_ready(out_uart)) {
		return -ENODEV;
	}

	return 0;
}

void protocol_send_sample(const struct radar_sample *sample)
{
	char body[32];
	char line[48];

	int len = snprintk(body, sizeof(body), "R,%d,%u,%u", sample->angle_deg,
			   sample->distance_mm, sample->status);

	snprintk(line, sizeof(line), "$%s*%02X\r\n", body, checksum(body, len));
	send_line(line);
}