#ifndef LED_H_
#define LED_H_

#include <stdbool.h>

enum led_color {
	LED_RED,
	LED_YELLOW,
	LED_GREEN,
	LED_PED_RED,
	LED_PED_GREEN,
	LED_COUNT,
};

/* Configure all LED GPIOs. Returns 0 on success, negative errno on failure. */
int led_init(void);

/* Turn one LED on or off. */
void led_set(enum led_color color, bool on);

/* Turn all LEDs off. */
void led_all_off(void);

#endif /* LED_H_ */