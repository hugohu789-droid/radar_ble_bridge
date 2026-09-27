#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/printk.h>

#include "radar.h"
#include "servo.h"
#include "us100.h"

/* Sweep settings */
#define STEP_DEG          2
#define SETTLE_MIN_MS     20 /* minimum wait after any move */
#define SETTLE_PER_DEG_MS 2  /* MG90S: roughly 0.1 s per 60 degrees */

/*
 * Readings per angle. 1 gives the fastest sweep (~7 s for 0-180).
 * Set to 3 to take the median of three readings, which removes
 * occasional spikes at the cost of a sweep about 2-3x slower.
 */
#define SAMPLES_PER_POINT 1

/* Leave time for echoes from the previous ping to die out */
#define MIN_PING_INTERVAL_MS 50

#define RADAR_STACK_SIZE 1024
/*
 * Must be a higher priority (lower number) than main, which renders
 * LVGL. Otherwise screen updates hold up the servo and it stutters.
 * The thread sleeps most of the time, so LVGL still gets plenty of CPU.
 * See CONFIG_MAIN_THREAD_PRIORITY in prj.conf.
 */
#define RADAR_PRIORITY   5
#define QUEUE_DEPTH      32

K_MSGQ_DEFINE(radar_msgq, sizeof(struct radar_sample), QUEUE_DEPTH, 4);

static atomic_t dropped;
static atomic_t started;
static int64_t last_ping_ms;

static void radar_thread(void *p1, void *p2, void *p3);

/* Defined with an infinite start delay, so it only runs after radar_start() */
K_THREAD_DEFINE(radar_tid, RADAR_STACK_SIZE, radar_thread, NULL, NULL, NULL,
		RADAR_PRIORITY, 0, SYS_FOREVER_MS);

static uint16_t median(uint16_t *values, int count)
{
	/* Insertion sort - count is tiny */
	for (int i = 1; i < count; i++) {
		uint16_t v = values[i];
		int j = i - 1;

		while (j >= 0 && values[j] > v) {
			values[j + 1] = values[j];
			j--;
		}
		values[j + 1] = v;
	}

	return values[count / 2];
}

static void wait_for_ping_slot(void)
{
	int64_t elapsed = k_uptime_get() - last_ping_ms;

	if (elapsed < MIN_PING_INTERVAL_MS) {
		k_msleep(MIN_PING_INTERVAL_MS - elapsed);
	}
}

/*
 * Returns 0 and the (median) distance if at least one reading was in
 * range, otherwise the error from the last reading.
 */
static int measure(uint16_t *distance_mm)
{
	uint16_t readings[SAMPLES_PER_POINT];
	int valid = 0;
	int last_err = -ETIMEDOUT;

	for (int i = 0; i < SAMPLES_PER_POINT; i++) {
		uint16_t raw = 0;

		wait_for_ping_slot();
		last_ping_ms = k_uptime_get();

		int ret = us100_read_distance_mm(&raw);

		if (ret == 0) {
			readings[valid++] = raw;
		} else {
			last_err = ret;
		}
	}

	if (valid == 0) {
		return last_err;
	}

	*distance_mm = median(readings, valid);
	return 0;
}

static void sample_at(int angle, int prev_angle)
{
	struct radar_sample sample = { .angle_deg = angle };
	uint16_t mm = 0;

	servo_set_angle(angle);
	k_msleep(MAX(SETTLE_MIN_MS, abs(angle - prev_angle) * SETTLE_PER_DEG_MS));

	int ret = measure(&mm);

	if (ret == 0) {
		sample.status = RADAR_OK;
		sample.distance_mm = mm;
	} else if (ret == -ERANGE) {
		sample.status = RADAR_OUT_OF_RANGE;
	} else {
		sample.status = RADAR_NO_REPLY;
	}

	sample.timestamp_ms = k_uptime_get_32();

	/*
	 * Never block the scan on a slow consumer: if the queue is full,
	 * drop this sample and count it.
	 */
	if (k_msgq_put(&radar_msgq, &sample, K_NO_WAIT) != 0) {
		atomic_inc(&dropped);
	}
}

static void radar_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int angle = SERVO_ANGLE_MIN;
	int prev_angle = servo_get_angle();
	int dir = STEP_DEG;

	while (1) {
		sample_at(angle, prev_angle);
		prev_angle = angle;

		int next = angle + dir;

		/* Bounce back at either end of the sweep */
		if (next > SERVO_ANGLE_MAX || next < SERVO_ANGLE_MIN) {
			dir = -dir;
			next = angle + dir;
		}

		angle = next;
	}
}

void radar_start(void)
{
	if (atomic_cas(&started, 0, 1)) {
		k_thread_start(radar_tid);
	}
}

int radar_get_sample(struct radar_sample *sample, k_timeout_t timeout)
{
	return k_msgq_get(&radar_msgq, sample, timeout);
}

uint32_t radar_dropped_count(void)
{
	return (uint32_t)atomic_get(&dropped);
}