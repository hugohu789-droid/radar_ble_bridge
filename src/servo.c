#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/printk.h>

#include "servo.h"

#define SERVO_NODE DT_ALIAS(servo)

#if !DT_NODE_HAS_STATUS(SERVO_NODE, okay)
#error "Servo devicetree node not found, check that the overlay is applied"
#endif

static const struct pwm_dt_spec servo = PWM_DT_SPEC_GET(SERVO_NODE);

/*
 * Pulse width for 0 and 180 degrees. 500-2500 us is the common range
 * for MG90S; individual servos vary. If the servo buzzes or strains
 * at either end, narrow this range (e.g. 600-2400).
 */
#define SERVO_MIN_PULSE_US 500
#define SERVO_MAX_PULSE_US 2500

static int current_angle = -1;

int servo_init(void)
{
	if (!pwm_is_ready_dt(&servo)) {
		printk("Error: servo PWM device not ready\n");
		return -ENODEV;
	}

	return servo_set_angle(90);
}

int servo_set_angle(int angle_deg)
{
	angle_deg = CLAMP(angle_deg, SERVO_ANGLE_MIN, SERVO_ANGLE_MAX);

	uint32_t pulse_us = SERVO_MIN_PULSE_US +
			    (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) * angle_deg /
				    (SERVO_ANGLE_MAX - SERVO_ANGLE_MIN);

	int ret = pwm_set_pulse_dt(&servo, PWM_USEC(pulse_us));

	if (ret != 0) {
		printk("Error: failed to set servo pulse (%d)\n", ret);
		return ret;
	}

	current_angle = angle_deg;
	return 0;
}

int servo_get_angle(void)
{
	return current_angle;
}
