#ifndef BUZZER_H_
#define BUZZER_H_

#include <stdbool.h>

/* Configure the buzzer PWM (silent). Returns 0 on success, negative errno on failure. */
int buzzer_init(void);

/* Turn the tone on or off immediately. Non-blocking. */
void buzzer_set_tone(bool on);

#endif /* BUZZER_H_ */