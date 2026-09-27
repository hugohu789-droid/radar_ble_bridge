#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>

#include "alarm.h"
#include "buzzer.h"

#define LED_NODE DT_ALIAS(led_alarm)

#if !DT_NODE_HAS_STATUS(LED_NODE, okay)
#error "Alarm LED devicetree node not found, check that the overlay is applied"
#endif

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

/* Distance thresholds (mm) */
#define ALARM_WARN_MM   400
#define ALARM_DANGER_MM 150

/*
 * Keep alarming this long after the last close reading. Just long
 * enough to bridge one or two samples (~80 ms each) with a missed
 * echo, so the alarm follows the beam instead of lingering after the
 * servo has moved on.
 */
#define ALARM_HOLD_MS 300

#define SELF_TEST_MS 100

struct blink_timing {
	uint16_t on_ms;
	uint16_t off_ms;
};

/* Index matches enum alarm_level; ALARM_NONE is unused */
static const struct blink_timing timings[] = {
	[ALARM_WARN]   = { .on_ms = 80, .off_ms = 400 },
	[ALARM_DANGER] = { .on_ms = 60, .off_ms = 90 },
};

/* Written by main, read by the workqueue */
static atomic_t level = ATOMIC_INIT(ALARM_NONE);

/* Only touched by alarm_update() on the main thread */
static int64_t last_warn_ms = INT64_MIN / 2;
static int64_t last_danger_ms = INT64_MIN / 2;

static struct k_work_delayable pattern_work;
static bool output_on;

static void set_outputs(bool on)
{
	output_on = on;
	gpio_pin_set_dt(&led, on ? 1 : 0);
	buzzer_set_tone(on);
}

/*
 * Drives the LED and the buzzer from the same work item, so the blink
 * and the beep always stay in step. Stops rescheduling itself once the
 * level drops back to ALARM_NONE.
 */
static void pattern_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	enum alarm_level lvl = (enum alarm_level)atomic_get(&level);

	if (lvl == ALARM_NONE) {
		set_outputs(false);
		return;
	}

	set_outputs(!output_on);

	uint16_t delay = output_on ? timings[lvl].on_ms : timings[lvl].off_ms;

	k_work_reschedule(&pattern_work, K_MSEC(delay));
}

static enum alarm_level level_for(const struct radar_sample *sample)
{
	if (sample->status != RADAR_OK) {
		return ALARM_NONE;
	}

	if (sample->distance_mm <= ALARM_DANGER_MM) {
		return ALARM_DANGER;
	}

	if (sample->distance_mm <= ALARM_WARN_MM) {
		return ALARM_WARN;
	}

	return ALARM_NONE;
}

int alarm_init(void)
{
	if (!gpio_is_ready_dt(&led)) {
		printk("Error: alarm LED not ready\n");
		return -ENODEV;
	}

	int ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

	if (ret != 0) {
		printk("Error: failed to configure alarm LED (%d)\n", ret);
		return ret;
	}

	ret = buzzer_init();
	if (ret != 0) {
		return ret;
	}

	k_work_init_delayable(&pattern_work, pattern_handler);

	/* Self-test: one short beep + flash confirms both are wired */
	set_outputs(true);
	k_msleep(SELF_TEST_MS);
	set_outputs(false);

	return 0;
}

void alarm_update(const struct radar_sample *sample)
{
	int64_t now = k_uptime_get();

	switch (level_for(sample)) {
	case ALARM_DANGER:
		last_danger_ms = now;
		break;
	case ALARM_WARN:
		last_warn_ms = now;
		break;
	default:
		break;
	}

	enum alarm_level new_level = ALARM_NONE;

	if (now - last_danger_ms < ALARM_HOLD_MS) {
		new_level = ALARM_DANGER;
	} else if (now - last_warn_ms < ALARM_HOLD_MS) {
		new_level = ALARM_WARN;
	}

	enum alarm_level old_level = (enum alarm_level)atomic_set(&level, new_level);

	if (old_level == ALARM_NONE && new_level != ALARM_NONE) {
		/* Start the pattern. If it is still winding down from an
		 * earlier alarm this is a no-op, and the running pattern
		 * picks up the new level on its next tick.
		 */
		k_work_schedule(&pattern_work, K_NO_WAIT);
	} else if (old_level != ALARM_NONE && new_level == ALARM_NONE) {
		/* Run the handler now so the LED and buzzer go quiet
		 * immediately, instead of after the current on/off phase.
		 */
		k_work_reschedule(&pattern_work, K_NO_WAIT);
	}
}

enum alarm_level alarm_get_level(void)
{
	return (enum alarm_level)atomic_get(&level);
}