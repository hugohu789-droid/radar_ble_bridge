#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "servo.h"
#include "us100.h"
#include "radar.h"
#include "protocol.h"
#include "alarm.h"
#include "radar_ui.h"

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
	 * thread that touches LVGL. Waiting on the queue with a timeout
	 * lets LVGL keep running between samples.
	 */
	struct radar_sample sample;
	uint32_t wait_ms = radar_ui_process();

	while (1) {
		if (radar_get_sample(&sample, K_MSEC(wait_ms)) == 0) {
			protocol_send_sample(&sample);
			alarm_update(&sample);
			radar_ui_update(&sample, alarm_get_level());
		}

		wait_ms = radar_ui_process();
	}

	return 0;
}