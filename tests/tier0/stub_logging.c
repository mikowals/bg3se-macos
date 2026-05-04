/*
 * Stub implementations of logging.h functions for Tier 0 tests.
 * All logging is silently discarded.
 */

#include "logging.h"

void log_init(void) {}
void log_shutdown(void) {}

void log_set_global_level(LogLevel level) { (void)level; }
LogLevel log_get_global_level(void) { return LOG_LEVEL_NONE; }

void log_set_module_level(LogModule module, LogLevel level) {
    (void)module; (void)level;
}
LogLevel log_get_module_level(LogModule module) {
    (void)module;
    return LOG_LEVEL_NONE;
}

void log_set_output_flags(uint32_t flags) { (void)flags; }
void log_set_format(LogFormat format) { (void)format; }
void log_set_color_enabled(bool enabled) { (void)enabled; }

bool log_should_write(LogLevel level, LogModule module) {
    (void)level; (void)module;
    return false;
}

void log_write(LogLevel level, LogModule module,
               const char *file, int line, const char *fmt, ...) {
    (void)level; (void)module; (void)file; (void)line; (void)fmt;
}

void log_message(const char *fmt, ...) { (void)fmt; }

int log_register_callback(LogCallback cb, void *userdata,
                          LogLevel min_level, uint32_t module_mask) {
    (void)cb; (void)userdata; (void)min_level; (void)module_mask;
    return -1;
}
void log_unregister_callback(int callback_id) { (void)callback_id; }

LogLevel log_level_from_string(const char *str) {
    (void)str;
    return LOG_LEVEL_NONE;
}
