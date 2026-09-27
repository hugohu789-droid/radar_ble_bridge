#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/printk.h>

#include "buzzer.h"

#define BUZZER_NODE DT_ALIAS(buzzer_pwm)

#if !DT_NODE_HAS_STATUS(BUZZER_NODE, okay)
#error "Buzzer PWM devicetree node not found, check that the overlay is applied"
#endif

static const struct pwm_dt_spec buzzer = PWM_DT_SPEC_GET(BUZZER_NODE);

int buzzer_init(void)
{
	if (!pwm_is_ready_dt(&buzzer)) {
		printk("Error: buzzer PWM device not ready\n");
		return -ENODEV;
	}

	buzzer_set_tone(false);
	return 0;
}

/*
 * A passive buzzer needs a continuous square wave to make a sound -
 * a 50% duty cycle does that. 0% duty (no pulse) is silent.
 */
void buzzer_set_tone(bool on)
{
	pwm_set_pulse_dt(&buzzer, on ? buzzer.period / 2 : 0);
}