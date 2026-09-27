#ifndef RADAR_H_
#define RADAR_H_

#include <stdint.h>
#include <zephyr/kernel.h>

enum radar_status {
	RADAR_OK = 0,           /* distance_mm is valid */
	RADAR_OUT_OF_RANGE = 1, /* no echo, or target too close/far */
	RADAR_NO_REPLY = 2,     /* US-100 did not answer */
};

struct radar_sample {
	uint32_t timestamp_ms; /* k_uptime when the sample was taken */
	int16_t angle_deg;
	uint16_t distance_mm;  /* 0 unless status is RADAR_OK */
	uint8_t status;        /* enum radar_status */
};

/*
 * Start the scan thread. servo_init() and us100_init() must have
 * succeeded first. Calling it again has no effect.
 */
void radar_start(void);

/*
 * Take the next sample from the queue, waiting up to timeout.
 * Returns 0 on success, -EAGAIN if the timeout expired.
 * The queue has a single consumer: each sample is delivered once.
 */
int radar_get_sample(struct radar_sample *sample, k_timeout_t timeout);

/* Samples dropped because the queue was full (consumer too slow). */
uint32_t radar_dropped_count(void);

#endif /* RADAR_H_ */
