#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include "radar.h"

/*
 * Line-based text protocol, sent on the "radar-link" UART (CH340 on
 * UART4, 115200 8N1), or on the console UART if that alias is not
 * defined. One line per sample:
 *
 *   $R,<angle_deg>,<distance_mm>,<status>*<CS>\r\n
 *
 *   angle_deg    0..180
 *   distance_mm  0 unless status is 0
 *   status       0 = OK, 1 = out of range, 2 = no reply from sensor
 *   CS           XOR of every character between '$' and '*',
 *                as two uppercase hex digits (NMEA style)
 *
 * Example: $R,90,1234,0*43
 *
 * The PC side should still only accept lines that start with '$' and
 * have a valid checksum, and ignore everything else (e.g. noise when
 * the adapter is plugged in, or logs if the console fallback is used).
 */

/* Returns 0 on success, negative errno on failure. */
int protocol_init(void);

void protocol_send_sample(const struct radar_sample *sample);

#endif /* PROTOCOL_H_ */