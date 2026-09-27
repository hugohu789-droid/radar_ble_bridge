#ifndef RADAR_UI_H_
#define RADAR_UI_H_

#include <stdint.h>

#include "radar.h"
#include "alarm.h"

/*
 * Build the LVGL screen and switch the display on.
 * Returns 0 on success, negative errno on failure.
 *
 * LVGL is not thread-safe: radar_ui_init(), radar_ui_update() and
 * radar_ui_process() must all be called from the same thread (main).
 */
int radar_ui_init(void);

/* Record a new sample and mark the radar area for redraw. Cheap. */
void radar_ui_update(const struct radar_sample *sample, enum alarm_level level);

/*
 * Let LVGL render and flush whatever changed. Returns how many ms the
 * caller may wait before calling it again.
 */
uint32_t radar_ui_process(void);

#endif /* RADAR_UI_H_ */
