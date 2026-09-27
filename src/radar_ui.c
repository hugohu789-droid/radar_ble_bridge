#include <math.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/sys/printk.h>
#include <lvgl.h>

#include "radar_ui.h"
#include "servo.h"

/*
 * Layout on the 240x240 round screen: a half-circle radar in the upper
 * part, with its origin slightly below the centre, and text readouts
 * in the lower part.
 */
#define ORIGIN_X 120
#define ORIGIN_Y 150
#define RADIUS   108

/* Distance shown at the outer ring. Farther targets are not drawn. */
#define RANGE_MM 1000
#define RING_COUNT 3

/* A target dot fades out over this time if it is not measured again */
#define FADE_MS  6000
#define DOT_SIZE 6

/*
 * Set to 1 if the picture is mirrored compared with the real servo
 * (depends on how the servo and sensor are mounted).
 */
#define MIRROR 0

#define COLOR_GRID   lv_color_hex(0x0B5A2A)
#define COLOR_SWEEP  lv_color_hex(0x33FF77)
#define COLOR_TARGET lv_color_hex(0xFF4040)
/* Readouts use white/light blue so they stand out from the green radar */
#define COLOR_TEXT   lv_color_hex(0xFFFFFF)
#define COLOR_ANGLE  lv_color_hex(0x8FD8FF)
#define COLOR_WARN   lv_color_hex(0xFFB020)
#define COLOR_DANGER lv_color_hex(0xFF3030)

#define ANGLE_COUNT (SERVO_ANGLE_MAX - SERVO_ANGLE_MIN + 1)

/*
 * Temporary bring-up aid: print what LVGL and the display report, and
 * paint the background blue so it is obvious whether LVGL is drawing
 * anything at all. Set back to 0 once the screen works.
 */
#define RADAR_UI_DEBUG 0

/* Latest reading for every whole degree; that is all the state the drawing needs */
struct target {
	uint16_t distance_mm;
	uint32_t seen_ms; /* 0 = never seen */
};

static struct target targets[ANGLE_COUNT];
static int sweep_angle = 90;

static lv_obj_t *radar_obj;
static lv_obj_t *distance_label;
static lv_obj_t *angle_label;

/* Servo angle (0 = right, 180 = left) to screen point at radius r */
static lv_point_precise_t polar(int angle_deg, int32_t r)
{
	if (MIRROR) {
		angle_deg = SERVO_ANGLE_MAX - angle_deg;
	}

	float rad = (float)angle_deg * (3.14159265f / 180.0f);
	lv_point_precise_t p = {
		.x = (int32_t)lroundf(ORIGIN_X + r * cosf(rad)),
		.y = (int32_t)lroundf(ORIGIN_Y - r * sinf(rad)),
	};

	return p;
}

static void draw_line(lv_layer_t *layer, lv_point_precise_t p1, lv_point_precise_t p2,
		      lv_color_t color, int32_t width)
{
	lv_draw_line_dsc_t dsc;

	lv_draw_line_dsc_init(&dsc);
	dsc.color = color;
	dsc.width = width;
	dsc.p1 = p1;
	dsc.p2 = p2;
	lv_draw_line(layer, &dsc);
}

static void draw_grid(lv_layer_t *layer)
{
	lv_draw_arc_dsc_t arc;

	lv_draw_arc_dsc_init(&arc);
	arc.color = COLOR_GRID;
	arc.width = 1;
	arc.center.x = ORIGIN_X;
	arc.center.y = ORIGIN_Y;
	/* LVGL arc angles: 0 = right, clockwise, so 180..360 is the upper half */
	arc.start_angle = 180;
	arc.end_angle = 360;

	for (int i = 1; i <= RING_COUNT; i++) {
		arc.radius = RADIUS * i / RING_COUNT;
		lv_draw_arc(layer, &arc);
	}

	/* Base line and spokes every 30 degrees */
	lv_point_precise_t origin = { .x = ORIGIN_X, .y = ORIGIN_Y };

	draw_line(layer, polar(0, RADIUS), polar(180, RADIUS), COLOR_GRID, 1);
	for (int a = 30; a < 180; a += 30) {
		draw_line(layer, origin, polar(a, RADIUS), COLOR_GRID, 1);
	}
}

static void draw_targets(lv_layer_t *layer)
{
	uint32_t now = k_uptime_get_32();
	lv_draw_rect_dsc_t dot;

	lv_draw_rect_dsc_init(&dot);
	dot.bg_color = COLOR_TARGET;
	dot.radius = LV_RADIUS_CIRCLE;

	for (int a = 0; a < ANGLE_COUNT; a++) {
		const struct target *t = &targets[a];

		if (t->seen_ms == 0) {
			continue;
		}

		uint32_t age = now - t->seen_ms;

		if (age >= FADE_MS) {
			continue;
		}

		dot.bg_opa = (lv_opa_t)(LV_OPA_COVER * (FADE_MS - age) / FADE_MS);

		lv_point_precise_t p = polar(a + SERVO_ANGLE_MIN,
					     (int32_t)t->distance_mm * RADIUS / RANGE_MM);
		lv_area_t area = {
			.x1 = (int32_t)p.x - DOT_SIZE / 2,
			.y1 = (int32_t)p.y - DOT_SIZE / 2,
			.x2 = (int32_t)p.x + DOT_SIZE / 2,
			.y2 = (int32_t)p.y + DOT_SIZE / 2,
		};

		lv_draw_rect(layer, &dot, &area);
	}
}

/*
 * Everything in the radar area is drawn here from targets[] and
 * sweep_angle, instead of creating an LVGL object per dot and line.
 * That keeps LVGL's memory use tiny, which matters with 128 KB of RAM.
 */
static void radar_draw_cb(lv_event_t *e)
{
	lv_layer_t *layer = lv_event_get_layer(e);
	lv_point_precise_t origin = { .x = ORIGIN_X, .y = ORIGIN_Y };

	draw_grid(layer);
	draw_targets(layer);
	draw_line(layer, origin, polar(sweep_angle, RADIUS), COLOR_SWEEP, 2);
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, lv_color_t color,
			    int32_t y)
{
	lv_obj_t *label = lv_label_create(parent);

	lv_obj_set_style_text_font(label, font, 0);
	lv_obj_set_style_text_color(label, color, 0);
	lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
	lv_label_set_text(label, "");

	return label;
}

/*
 * Bring-up test that bypasses LVGL: fill the screen white with plain
 * display_write() calls, the same way the display sample does. 0xFFFF
 * is white in any RGB565 byte order.
 */
static void raw_fill_white(const struct device *display)
{
	static uint8_t stripe[240 * 8 * 2];
	struct display_buffer_descriptor desc = {
		.buf_size = sizeof(stripe),
		.width = 240,
		.height = 8,
		.pitch = 240,
	};

	memset(stripe, 0xFF, sizeof(stripe));

	for (int y = 0; y < 240; y += 8) {
		int ret = display_write(display, 0, y, &desc, stripe);

		if (ret != 0) {
			printk("UI: raw display_write failed at y=%d (%d)\n", y, ret);
			return;
		}
	}

	printk("UI: raw white fill done\n");
}

int radar_ui_init(void)
{
	const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

	if (!device_is_ready(display)) {
		printk("Error: display not ready\n");
		return -ENODEV;
	}

	if (RADAR_UI_DEBUG) {
		raw_fill_white(display);
		printk("UI: blanking off (raw test) returned %d\n",
		       display_blanking_off(display));
		/* Screen should be white now; LVGL then paints it blue */
		k_msleep(3000);
	}

	struct display_capabilities caps;

	display_get_capabilities(display, &caps);

	lv_display_t *lv_disp = lv_display_get_default();

	if (RADAR_UI_DEBUG) {
		printk("UI: display %ux%u, pixel format 0x%x, supported 0x%x\n",
		       caps.x_resolution, caps.y_resolution,
		       caps.current_pixel_format, caps.supported_pixel_formats);
		printk("UI: LVGL default display %p\n", (void *)lv_disp);
	}

	if (lv_disp == NULL) {
		printk("Error: LVGL has no display (LVGL init failed)\n");
		return -ENODEV;
	}

	lv_obj_t *screen = lv_screen_active();

	lv_obj_set_style_bg_color(screen,
				  RADAR_UI_DEBUG ? lv_color_hex(0x0000FF) : lv_color_black(), 0);
	lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

	/* A bare full-screen object with no styles, used only as a drawing surface */
	radar_obj = lv_obj_create(screen);
	lv_obj_remove_style_all(radar_obj);
	lv_obj_set_size(radar_obj, 240, 240);
	lv_obj_set_pos(radar_obj, 0, 0);
	lv_obj_add_event_cb(radar_obj, radar_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

	/*
	 * Distance in large white text just below the radar base line,
	 * angle underneath in light blue. Both stay inside the round
	 * screen: the circle is still ~90 px wide at the bottom of the
	 * distance text and ~55 px wide at the bottom of the angle text.
	 */
	distance_label = make_label(screen, &lv_font_montserrat_32, COLOR_TEXT, ORIGIN_Y + 12);
	angle_label = make_label(screen, &lv_font_montserrat_20, COLOR_ANGLE, ORIGIN_Y + 50);
	lv_label_set_text(distance_label, "--- cm");

	/* Render the first frame before switching the panel on, so no garbage shows */
	lv_timer_handler();
	if (RADAR_UI_DEBUG) {
		lv_refr_now(NULL);
		printk("UI: forced LVGL refresh done\n");
	}

	int ret = display_blanking_off(display);

	if (RADAR_UI_DEBUG) {
		printk("UI: blanking off returned %d\n", ret);
	}

	return 0;
}

void radar_ui_update(const struct radar_sample *sample, enum alarm_level level)
{
	int idx = sample->angle_deg - SERVO_ANGLE_MIN;

	if (idx < 0 || idx >= ANGLE_COUNT) {
		return;
	}

	sweep_angle = sample->angle_deg;

	if (sample->status == RADAR_OK && sample->distance_mm <= RANGE_MM) {
		targets[idx].distance_mm = sample->distance_mm;
		targets[idx].seen_ms = sample->timestamp_ms ? sample->timestamp_ms : 1;
	} else {
		/* Nothing there now: forget the old dot at this angle */
		targets[idx].seen_ms = 0;
	}

	if (sample->status == RADAR_OK) {
		lv_label_set_text_fmt(distance_label, "%u.%u cm", sample->distance_mm / 10,
				      sample->distance_mm % 10);
	} else if (sample->status == RADAR_OUT_OF_RANGE) {
		lv_label_set_text(distance_label, "--- cm");
	} else {
		lv_label_set_text(distance_label, "no signal");
	}

	lv_label_set_text_fmt(angle_label, "%d\xC2\xB0", sample->angle_deg);

	lv_color_t color = level == ALARM_DANGER ? COLOR_DANGER
			   : level == ALARM_WARN ? COLOR_WARN
						  : COLOR_TEXT;

	lv_obj_set_style_text_color(distance_label, color, 0);

	/* Only the half-circle needs redrawing; the labels invalidate themselves */
	lv_area_t area = {
		.x1 = ORIGIN_X - RADIUS - DOT_SIZE,
		.y1 = ORIGIN_Y - RADIUS - DOT_SIZE,
		.x2 = ORIGIN_X + RADIUS + DOT_SIZE,
		.y2 = ORIGIN_Y + DOT_SIZE,
	};

	lv_obj_invalidate_area(radar_obj, &area);
}

uint32_t radar_ui_process(void)
{
	uint32_t next_ms = lv_timer_handler();

	/* LVGL returns a huge value when no timer is pending; don't sleep that long */
	return CLAMP(next_ms, 1, 50);
}