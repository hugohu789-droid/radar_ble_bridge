#ifndef SERVO_H_
#define SERVO_H_

#define SERVO_ANGLE_MIN 0
#define SERVO_ANGLE_MAX 180

/*
 * Configure the servo PWM and move to the centre position (90 degrees).
 * Returns 0 on success, negative errno on failure.
 */
int servo_init(void);

/*
 * Move to the given angle in degrees. Values outside
 * SERVO_ANGLE_MIN..SERVO_ANGLE_MAX are clamped.
 * Non-blocking: returns as soon as the PWM is updated, before the
 * servo has physically finished moving.
 */
int servo_set_angle(int angle_deg);

/* Last angle that was commanded (not a measured position). */
int servo_get_angle(void);

#endif /* SERVO_H_ */
