/**
 * pid_controller.h
 * PID Temperature Control Algorithm
 * 
 * Implements proportional-integral-derivative control
 * for smooth, accurate temperature regulation
 */

#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

/**
 * Initialize PID controller with tuning parameters
 * @param kp Proportional gain
 * @param ki Integral gain
 * @param kd Derivative gain
 */
void pid_init(float kp, float ki, float kd);

/**
 * Compute PID output
 * @param current Current temperature
 * @param target Target temperature
 * @return Power output (0-100%)
 */
int pid_compute(float current, float target);

/**
 * Reset PID state (clear integral and error)
 * Call when setpoint changes or mode switches
 */
void pid_reset(void);

/**
 * Update PID tuning parameters
 * @param kp Proportional gain
 * @param ki Integral gain
 * @param kd Derivative gain
 */
void pid_set_gains(float kp, float ki, float kd);

/**
 * Get current PID gains
 * @param kp Output: proportional gain
 * @param ki Output: integral gain
 * @param kd Output: derivative gain
 */
void pid_get_gains(float* kp, float* ki, float* kd);

/**
 * Get PID debug info
 * @param p_term Output: proportional term
 * @param i_term Output: integral term
 * @param d_term Output: derivative term
 * @param error Output: current error
 */
void pid_get_debug_info(float* p_term, float* i_term, float* d_term, float* error);

#endif // PID_CONTROLLER_H
