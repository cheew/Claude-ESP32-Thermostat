/**
 * dimmer_control.cpp
 * AC Dimmer Control Implementation
 */

#include "dimmer_control.h"
#include <RBDdimmer.h>

// Pin configuration
#define DIMMER_PIN 5      // GPIO5 - PWM control pin for AC dimmer
#define ZEROCROSS_PIN 27  // GPIO27 - Zero-cross detection pin

// Hardware object
static dimmerLamp dimmer(DIMMER_PIN, ZEROCROSS_PIN);
static int current_power = 0;

void dimmer_init(void) {
  dimmer.begin(NORMAL_MODE, ON);
  dimmer.setPower(0);
  current_power = 0;
}

void dimmer_set_power(int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;

  dimmer.setPower(percent);
  current_power = percent;
}

int dimmer_get_power(void) {
  return current_power;
}
