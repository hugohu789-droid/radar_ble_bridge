#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include "led.h"

#define RED_NODE      DT_ALIAS(led_red)
#define YELLOW_NODE   DT_ALIAS(led_yellow)
#define GREEN_NODE    DT_ALIAS(led_green)
#define PED_RED_NODE  DT_ALIAS(led_ped_red)
#define PED_GREEN_NODE DT_ALIAS(led_ped_green)

#if !DT_NODE_HAS_STATUS(RED_NODE, okay) || !DT_NODE_HAS_STATUS(YELLOW_NODE, okay) || \
	!DT_NODE_HAS_STATUS(GREEN_NODE, okay) || !DT_NODE_HAS_STATUS(PED_RED_NODE, okay) || \
	!DT_NODE_HAS_STATUS(PED_GREEN_NODE, okay)
#error "LED devicetree nodes not found, check that the overlay is applied"
#endif

static const struct gpio_dt_spec leds[LED_COUNT] = {
	[LED_RED]        = GPIO_DT_SPEC_GET(RED_NODE, gpios),
	[LED_YELLOW]     = GPIO_DT_SPEC_GET(YELLOW_NODE, gpios),
	[LED_GREEN]      = GPIO_DT_SPEC_GET(GREEN_NODE, gpios),
	[LED_PED_RED]    = GPIO_DT_SPEC_GET(PED_RED_NODE, gpios),
	[LED_PED_GREEN]  = GPIO_DT_SPEC_GET(PED_GREEN_NODE, gpios),
};

int led_init(void)
{
	for (int i = 0; i < LED_COUNT; i++) {
		if (!gpio_is_ready_dt(&leds[i])) {
			printk("Error: LED %d device not ready\n", i);
			return -ENODEV;
		}

		int ret = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);

		if (ret != 0) {
			printk("Error: failed to configure LED %d (%d)\n", i, ret);
			return ret;
		}
	}

	return 0;
}

void led_set(enum led_color color, bool on)
{
	gpio_pin_set_dt(&leds[color], on ? 1 : 0);
}

void led_all_off(void)
{
	for (int i = 0; i < LED_COUNT; i++) {
		gpio_pin_set_dt(&leds[i], 0);
	}
}