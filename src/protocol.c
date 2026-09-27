#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>

#include "protocol.h"

/* Same UART as the console for now; point this elsewhere to separate data from logs */
static const struct device *const out_uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

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
