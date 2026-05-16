#ifndef GLOBAL_SWITCHES_H
#define GLOBAL_SWITCHES_H

#include <stdbool.h>
#include <stdint.h>

bool global_switches_init(void);
bool global_switches_set_skip_splash_screen(bool value);
bool global_switches_get_skip_splash_screen(void);
void global_switches_deferred_set_skip_splash_screen(void);

#endif
