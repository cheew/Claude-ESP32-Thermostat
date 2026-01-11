/**
 * dimmer_control.h
 * AC Dimmer Control Interface
 *
 * Hardware abstraction for AC phase-angle dimmer control
 */

#ifndef DIMMER_CONTROL_H
#define DIMMER_CONTROL_H

/**
 * Initialize AC dimmer hardware
 */
void dimmer_init(void);

/**
 * Set dimmer power output
 * @param percent Power level (0-100%)
 */
void dimmer_set_power(int percent);

/**
 * Get current dimmer power setting
 * @return Current power level (0-100%)
 */
int dimmer_get_power(void);

#endif // DIMMER_CONTROL_H
