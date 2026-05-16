/**
 * focusless_input.h - In-process input injection without focus
 *
 * Sends keyboard events directly via NSApp on BG3's main run loop.
 * Primary use: auto-dismiss the "Press Any Key" splash screen
 * during headless harness operation.
 *
 * Controlled by BG3SE_AUTO_DISMISS_SPLASH=1 env var.
 */

#ifndef FOCUSLESS_INPUT_H
#define FOCUSLESS_INPUT_H

#include <stdbool.h>
#include <stdint.h>

bool focusless_input_init(void);
void focusless_input_shutdown(void);

bool focusless_input_post_key_press(uint16_t keyCode, uint32_t modifiers);
bool focusless_input_post_mouse_click(double x_fraction, double y_fraction_top_origin);

void focusless_input_start_splash_autodismiss(double duration, double interval);
void focusless_input_mark_socket_ready(void);

#endif /* FOCUSLESS_INPUT_H */
