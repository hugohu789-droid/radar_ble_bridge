#ifndef US100_H_
#define US100_H_

#include <stdint.h>

/* Usable measuring range of the US-100 (mm) */
#define US100_MIN_RANGE_MM 20
#define US100_MAX_RANGE_MM 4500

/*
 * Check that the UART is ready. The module itself must be in UART
 * mode (jumper fitted on the back).
 * Returns 0 on success, negative errno on failure.
 */
int us100_init(void);

/*
 * Trigger one measurement and wait for the result (blocks up to
 * ~150 ms). The raw reading is always written to *distance_mm when
 * the module answers.
 * Returns 0 if the reading is within range, -ERANGE if the module
 * answered with an out-of-range value (no echo / too close),
 * -ETIMEDOUT if it did not answer.
 *
 * Not thread-safe: call from a single thread. Leave at least ~60 ms
 * between measurements so echoes from the previous ping die out.
 */
int us100_read_distance_mm(uint16_t *distance_mm);

/*
 * Read the module's on-board temperature sensor (whole degrees C).
 * Returns 0 on success, -EIO on an invalid value, -ETIMEDOUT if the
 * module did not answer.
 */
int us100_read_temperature_c(int *temp_c);

#endif /* US100_H_ */
