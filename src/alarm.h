#ifndef ALARM_H_
#define ALARM_H_

#include "radar.h"

enum alarm_level {
	ALARM_NONE,
	ALARM_WARN,   /* object within ALARM_WARN_MM: slow beep + blink */
	ALARM_DANGER, /* object within ALARM_DANGER_MM: fast beep + blink */
};

/*
 * Configure the red LED and the buzzer, then give a short beep and
 * flash as a wiring self-test.
 * Returns 0 on success, negative errno on failure.
 */
int alarm_init(void);

/*
 * Feed every radar sample in. The alarm level is the highest level
 * seen within the last ALARM_HOLD_MS, so it doesn't cut out as soon
 * as the sweep moves past the object.
 * Call from a single thread (main).
 */
void alarm_update(const struct radar_sample *sample);

enum alarm_level alarm_get_level(void);

#endif /* ALARM_H_ */
