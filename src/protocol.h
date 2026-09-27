#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include "radar.h"

/*
 * Line-based text protocol, sent on the console UART (ST-LINK virtual
 * COM port, 115200 8N1). One line per sample:
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
 * Log messages from printk share the same UART. The PC side should
 * only accept lines that start with '$' and have a valid checksum,
 * and ignore everything else.
 */

/* Returns 0 on success, negative errno on failure. */
int protocol_init(void);

void protocol_send_sample(const struct radar_sample *sample);

#endif /* PROTOCOL_H_ */
