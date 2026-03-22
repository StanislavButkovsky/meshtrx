/*
  Minimal machdep implementation for ESP32 (no profiling).
*/
#include "machdep.h"

void machdep_profile_init(void) {}
void machdep_profile_reset(void) {}
unsigned int machdep_profile_sample(void) { return 0; }
unsigned int machdep_profile_sample_and_log(unsigned int start, char s[]) {
  (void)start; (void)s;
  return 0;
}
void machdep_profile_print_logged_samples(void) {}
