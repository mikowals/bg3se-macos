#ifndef FOCUS_HACK_H
#define FOCUS_HACK_H

#include <stdbool.h>

bool focus_hack_init(void);
bool focus_hack_force_focused(void);
bool focus_hack_is_focused(void);
void focus_hack_deferred_force_focus(void);

#endif
