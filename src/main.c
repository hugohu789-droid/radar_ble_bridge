#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "servo.h"
#include "us100.h"
#include "radar.h"
#include "protocol.h"
#include "alarm.h"
#include "radar_ui.h"

static void handle_sample(const struct radar_sample *sample)
{
	protocol_send_sample(sample);
	alarm_update(sample);
	/* Only records the sample and marks the area dirty; no drawing here */
	radar_ui_update(sample, alarm_get_level());
}

static void report_drops(void)
{
	static uint32_t last_dropped;
	uint32_t dropped = radar_dropped_count();

	if (dropped != last_dropped) {
		printk("Warning: %u radar samples dropped (queue full)\n",
		       dropped - last_dropped);
		last_dropped = dropped;
	}
}

int main(void)
{
	if (radar_ui_init() != 0) {
		return 0;
	}

	if (servo_init() != 0) {
		return 0;
	}

	if (us100_init() != 0) {
		return 0;
	}

	if (alarm_init() != 0) {
		return 0;
	}

	if (protocol_init() != 0) {
		printk("Error: protocol UART not ready\n");
		return 0;
	}

	printk("Radar started\n");
	radar_start();

	/*
	 * main is the single consumer of the radar queue and also the only
	 * thread that touches LVGL.
	 *
	 * Rendering a frame can take longer than the gap between samples.
	 * So after waking up, drain every sample that is waiting (each one
	 * is cheap to handle), then render once. The screen then always
	 * shows the latest state instead of falling further and further
	 * behind the servo.
	 */
	struct radar_sample sample;
	uint32_t wait_ms = radar_ui_process();

	while (1) {
		if (radar_get_sample(&sample, K_MSEC(wait_ms)) == 0) {
			do {
				handle_sample(&sample);
			} while (radar_get_sample(&sample, K_NO_WAIT) == 0);
		}

		report_drops();
		wait_ms = radar_ui_process();
	}

	return 0;
}