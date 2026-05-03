/**
 * lua_timer.c - Ext.Timer Lua bindings
 *
 * Timer Functions:
 * - Ext.Timer.WaitFor(delay, callback, [repeat]) - Create timer (game time)
 * - Ext.Timer.WaitForRealtime(delay, callback, [repeat]) - Create timer (wall-clock)
 * - Ext.Timer.Cancel(handle) - Cancel timer
 * - Ext.Timer.Pause(handle) - Pause timer
 * - Ext.Timer.Resume(handle) - Resume timer
 * - Ext.Timer.IsPaused(handle) - Check if timer is paused
 *
 * Time Query Functions:
 * - Ext.Timer.MonotonicTime() - Monotonic clock in milliseconds
 * - Ext.Timer.MicrosecTime() - Monotonic clock in microseconds
 * - Ext.Timer.ClockEpoch() - Unix timestamp (seconds since epoch)
 * - Ext.Timer.ClockTime() - Formatted clock time string
 * - Ext.Timer.GameTime() - Game time in seconds (pauses when game pauses)
 * - Ext.Timer.DeltaTime() - Last frame's delta time in seconds
 * - Ext.Timer.Ticks() - Game tick count
 * - Ext.Timer.IsGamePaused() - Check if game time is paused
 */

#include "lua_timer.h"
#include "../timer/timer.h"
#include "../core/logging.h"

#include <lauxlib.h>
#include <math.h>
#include <stdlib.h>

// Maximum timer delay (24 hours in milliseconds)
#define TIMER_MAX_DELAY_MS 86400000.0

// ============================================================================
// Ext.Timer.WaitFor(delay, callback, [repeat])
// ============================================================================

static int lua_timer_waitfor(lua_State *L) {
    // Arg 1: delay in milliseconds
    double delay_ms = luaL_checknumber(L, 1);

    // Validate delay
    if (delay_ms < 0 || !isfinite(delay_ms)) {
        return luaL_error(L, "delay must be >= 0 and finite (got %f)", delay_ms);
    }
    if (delay_ms > TIMER_MAX_DELAY_MS) {
        return luaL_error(L, "delay must be <= %fms (24 hours)", TIMER_MAX_DELAY_MS);
    }

    // Arg 2: callback function
    luaL_checktype(L, 2, LUA_TFUNCTION);

    // Arg 3: optional repeat interval in milliseconds
    double repeat_ms = 0;
    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) {
        repeat_ms = luaL_checknumber(L, 3);

        // Validate repeat interval
        if (repeat_ms < 0 || !isfinite(repeat_ms)) {
            return luaL_error(L, "repeat must be >= 0 and finite (got %f)", repeat_ms);
        }
        if (repeat_ms > 0 && repeat_ms < 1.0) {
            return luaL_error(L, "repeat interval must be >= 1ms or 0 (got %f)", repeat_ms);
        }
        if (repeat_ms > TIMER_MAX_DELAY_MS) {
            return luaL_error(L, "repeat must be <= %fms (24 hours)", TIMER_MAX_DELAY_MS);
        }
    }

    // Store callback in registry
    lua_pushvalue(L, 2);
    int callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    // Create timer
    uint64_t handle = timer_create(L, delay_ms, callback_ref, repeat_ms);

    if (handle == 0) {
        return luaL_error(L, "Failed to create timer (pool exhausted)");
    }

    lua_pushinteger(L, (lua_Integer)handle);
    return 1;
}

// ============================================================================
// Ext.Timer.WaitForRealtime(delay, callback, [repeat])
// Same as WaitFor but explicitly uses wall-clock time (not game time).
// Currently identical to WaitFor since we don't track game pause state yet.
// ============================================================================

static int lua_timer_waitfor_realtime(lua_State *L) {
    // Arg 1: delay in milliseconds
    double delay_ms = luaL_checknumber(L, 1);

    // Validate delay
    if (delay_ms < 0 || !isfinite(delay_ms)) {
        return luaL_error(L, "delay must be >= 0 and finite (got %f)", delay_ms);
    }
    if (delay_ms > TIMER_MAX_DELAY_MS) {
        return luaL_error(L, "delay must be <= %fms (24 hours)", TIMER_MAX_DELAY_MS);
    }

    // Arg 2: callback function
    luaL_checktype(L, 2, LUA_TFUNCTION);

    // Arg 3: optional repeat interval in milliseconds
    double repeat_ms = 0;
    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) {
        repeat_ms = luaL_checknumber(L, 3);

        // Validate repeat interval
        if (repeat_ms < 0 || !isfinite(repeat_ms)) {
            return luaL_error(L, "repeat must be >= 0 and finite (got %f)", repeat_ms);
        }
        if (repeat_ms > 0 && repeat_ms < 1.0) {
            return luaL_error(L, "repeat interval must be >= 1ms or 0 (got %f)", repeat_ms);
        }
        if (repeat_ms > TIMER_MAX_DELAY_MS) {
            return luaL_error(L, "repeat must be <= %fms (24 hours)", TIMER_MAX_DELAY_MS);
        }
    }

    // Store callback in registry
    lua_pushvalue(L, 2);
    int callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    // Create timer (uses wall-clock time via mach_absolute_time)
    uint64_t handle = timer_create(L, delay_ms, callback_ref, repeat_ms);

    if (handle == 0) {
        return luaL_error(L, "Failed to create timer (pool exhausted)");
    }

    lua_pushinteger(L, (lua_Integer)handle);
    return 1;
}

// ============================================================================
// Ext.Timer.Cancel(handle)
// ============================================================================

static int lua_timer_cancel(lua_State *L) {
    uint64_t handle = (uint64_t)luaL_checkinteger(L, 1);

    bool result = timer_cancel(L, handle);

    lua_pushboolean(L, result);
    return 1;
}

// ============================================================================
// Ext.Timer.Pause(handle)
// ============================================================================

static int lua_timer_pause(lua_State *L) {
    uint64_t handle = (uint64_t)luaL_checkinteger(L, 1);

    bool result = timer_pause(handle);

    lua_pushboolean(L, result);
    return 1;
}

// ============================================================================
// Ext.Timer.Resume(handle)
// ============================================================================

static int lua_timer_resume(lua_State *L) {
    uint64_t handle = (uint64_t)luaL_checkinteger(L, 1);

    bool result = timer_resume(handle);

    lua_pushboolean(L, result);
    return 1;
}

// ============================================================================
// Ext.Timer.IsPaused(handle)
// ============================================================================

static int lua_timer_is_paused(lua_State *L) {
    uint64_t handle = (uint64_t)luaL_checkinteger(L, 1);

    bool result = timer_is_paused(handle);

    lua_pushboolean(L, result);
    return 1;
}

// ============================================================================
// Ext.Timer.MonotonicTime()
// ============================================================================

static int lua_timer_monotonic_time(lua_State *L) {
    double ms = timer_get_monotonic_ms();

    lua_pushnumber(L, ms);
    return 1;
}

// ============================================================================
// Ext.Timer.MicrosecTime()
// ============================================================================

static int lua_timer_microsec_time(lua_State *L) {
    double us = timer_get_microsec_time();

    lua_pushnumber(L, us);
    return 1;
}

// ============================================================================
// Ext.Timer.ClockEpoch()
// ============================================================================

static int lua_timer_clock_epoch(lua_State *L) {
    int64_t epoch = timer_get_epoch_seconds();

    lua_pushinteger(L, (lua_Integer)epoch);
    return 1;
}

// ============================================================================
// Ext.Timer.ClockTime()
// ============================================================================

static int lua_timer_clock_time(lua_State *L) {
    const char *time_str = timer_get_clock_time();

    lua_pushstring(L, time_str);
    return 1;
}

// ============================================================================
// Ext.Timer.GameTime() - Get current game time in seconds
// ============================================================================

static int lua_timer_game_time(lua_State *L) {
    double game_time = timer_get_game_time();

    lua_pushnumber(L, game_time);
    return 1;
}

// ============================================================================
// Ext.Timer.DeltaTime() - Get last frame's delta time in seconds
// ============================================================================

static int lua_timer_delta_time(lua_State *L) {
    double delta_time = timer_get_delta_time();

    lua_pushnumber(L, delta_time);
    return 1;
}

// ============================================================================
// Ext.Timer.Ticks() - Get game tick count
// ============================================================================

static int lua_timer_ticks(lua_State *L) {
    int64_t ticks = timer_get_tick_count();

    lua_pushinteger(L, (lua_Integer)ticks);
    return 1;
}

// ============================================================================
// Ext.Timer.IsGamePaused() - Check if game time is paused
// ============================================================================

static int lua_timer_is_game_paused(lua_State *L) {
    bool paused = timer_is_game_paused();

    lua_pushboolean(L, paused);
    return 1;
}

// ============================================================================
// Ext.Timer.RegisterPersistentHandler(name, callback)
// ============================================================================

static int lua_timer_register_persistent_handler(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    // Store callback in registry
    lua_pushvalue(L, 2);
    int callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    bool result = timer_register_persistent_handler(L, name, callback_ref);

    lua_pushboolean(L, result);
    return 1;
}

// ============================================================================
// Ext.Timer.UnregisterPersistentHandler(name)
// ============================================================================

static int lua_timer_unregister_persistent_handler(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);

    bool result = timer_unregister_persistent_handler(L, name);

    lua_pushboolean(L, result);
    return 1;
}

// ============================================================================
// Ext.Timer.WaitForPersistent(delay, handler, [args], [repeat])
// ============================================================================

static int lua_timer_waitfor_persistent(lua_State *L) {
    // Arg 1: delay in milliseconds
    double delay_ms = luaL_checknumber(L, 1);

    // Validate delay
    if (delay_ms < 0 || !isfinite(delay_ms)) {
        return luaL_error(L, "delay must be >= 0 and finite (got %f)", delay_ms);
    }
    if (delay_ms > TIMER_MAX_DELAY_MS) {
        return luaL_error(L, "delay must be <= %fms (24 hours)", TIMER_MAX_DELAY_MS);
    }

    // Arg 2: handler name (string)
    const char *handler_name = luaL_checkstring(L, 2);

    // Arg 3: optional args (string or table -> JSON)
    const char *args_json = NULL;
    char args_buffer[1024] = {0};

    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) {
        if (lua_isstring(L, 3)) {
            args_json = lua_tostring(L, 3);
        } else if (lua_istable(L, 3)) {
            // Simple table to JSON conversion
            // For now, only support simple key-value pairs
            lua_pushvalue(L, 3);
            char *p = args_buffer;
            char *end = args_buffer + sizeof(args_buffer) - 2;
            *p++ = '{';
            bool first = true;

            lua_pushnil(L);
            while (lua_next(L, -2) != 0) {
                if (p >= end) { lua_pop(L, 2); break; }
                if (!first) {
                    *p++ = ',';
                }
                first = false;

                // Key (must be string)
                if (lua_type(L, -2) == LUA_TSTRING) {
                    const char *key = lua_tostring(L, -2);
                    p += snprintf(p, sizeof(args_buffer) - (p - args_buffer), "\"%s\":", key);
                }

                // Value
                int vtype = lua_type(L, -1);
                if (vtype == LUA_TSTRING) {
                    const char *val = lua_tostring(L, -1);
                    p += snprintf(p, sizeof(args_buffer) - (p - args_buffer), "\"%s\"", val);
                } else if (vtype == LUA_TNUMBER) {
                    double val = lua_tonumber(L, -1);
                    p += snprintf(p, sizeof(args_buffer) - (p - args_buffer), "%g", val);
                } else if (vtype == LUA_TBOOLEAN) {
                    p += snprintf(p, sizeof(args_buffer) - (p - args_buffer),
                                  lua_toboolean(L, -1) ? "true" : "false");
                } else {
                    p += snprintf(p, sizeof(args_buffer) - (p - args_buffer), "null");
                }

                lua_pop(L, 1);
            }
            lua_pop(L, 1);  // pop the table copy

            *p++ = '}';
            *p = '\0';
            args_json = args_buffer;
        }
    }

    // Arg 4: optional repeat interval in milliseconds
    double repeat_ms = 0;
    if (lua_gettop(L) >= 4 && !lua_isnil(L, 4)) {
        repeat_ms = luaL_checknumber(L, 4);

        if (repeat_ms < 0 || !isfinite(repeat_ms)) {
            return luaL_error(L, "repeat must be >= 0 and finite (got %f)", repeat_ms);
        }
        if (repeat_ms > 0 && repeat_ms < 1.0) {
            return luaL_error(L, "repeat interval must be >= 1ms or 0 (got %f)", repeat_ms);
        }
        if (repeat_ms > TIMER_MAX_DELAY_MS) {
            return luaL_error(L, "repeat must be <= %fms (24 hours)", TIMER_MAX_DELAY_MS);
        }
    }

    // Create persistent timer
    uint64_t handle = timer_create_persistent(L, delay_ms, handler_name, args_json, repeat_ms);

    if (handle == 0) {
        return luaL_error(L, "Failed to create persistent timer (handler not found or pool exhausted)");
    }

    lua_pushinteger(L, (lua_Integer)handle);
    return 1;
}

// ============================================================================
// Ext.Timer.CancelPersistent(handle)
// ============================================================================

static int lua_timer_cancel_persistent(lua_State *L) {
    uint64_t handle = (uint64_t)luaL_checkinteger(L, 1);

    bool result = timer_cancel_persistent(L, handle);

    lua_pushboolean(L, result);
    return 1;
}

// ============================================================================
// Ext.Timer.ExportPersistent()
// ============================================================================

static int lua_timer_export_persistent(lua_State *L) {
    char *json = timer_export_persistent();

    if (json) {
        lua_pushstring(L, json);
        free(json);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// ============================================================================
// Ext.Timer.ImportPersistent(json)
// ============================================================================

static int lua_timer_import_persistent(lua_State *L) {
    const char *json = luaL_checkstring(L, 1);

    int count = timer_import_persistent(L, json);

    lua_pushinteger(L, count);
    return 1;
}

// ============================================================================
// Registration
// ============================================================================

void lua_timer_register(lua_State *L, int ext_table_idx) {
    // Convert negative index to absolute since we'll be pushing onto stack
    if (ext_table_idx < 0) {
        ext_table_idx = lua_gettop(L) + ext_table_idx + 1;
    }

    // Create Ext.Timer table
    lua_newtable(L);

    // Register functions
    lua_pushcfunction(L, lua_timer_waitfor);
    lua_setfield(L, -2, "WaitFor");

    lua_pushcfunction(L, lua_timer_waitfor_realtime);
    lua_setfield(L, -2, "WaitForRealtime");

    lua_pushcfunction(L, lua_timer_cancel);
    lua_setfield(L, -2, "Cancel");

    lua_pushcfunction(L, lua_timer_pause);
    lua_setfield(L, -2, "Pause");

    lua_pushcfunction(L, lua_timer_resume);
    lua_setfield(L, -2, "Resume");

    lua_pushcfunction(L, lua_timer_is_paused);
    lua_setfield(L, -2, "IsPaused");

    lua_pushcfunction(L, lua_timer_monotonic_time);
    lua_setfield(L, -2, "MonotonicTime");

    lua_pushcfunction(L, lua_timer_microsec_time);
    lua_setfield(L, -2, "MicrosecTime");

    lua_pushcfunction(L, lua_timer_clock_epoch);
    lua_setfield(L, -2, "ClockEpoch");

    lua_pushcfunction(L, lua_timer_clock_time);
    lua_setfield(L, -2, "ClockTime");

    lua_pushcfunction(L, lua_timer_game_time);
    lua_setfield(L, -2, "GameTime");

    lua_pushcfunction(L, lua_timer_delta_time);
    lua_setfield(L, -2, "DeltaTime");

    lua_pushcfunction(L, lua_timer_ticks);
    lua_setfield(L, -2, "Ticks");

    lua_pushcfunction(L, lua_timer_is_game_paused);
    lua_setfield(L, -2, "IsGamePaused");

    // Persistent timer functions
    lua_pushcfunction(L, lua_timer_register_persistent_handler);
    lua_setfield(L, -2, "RegisterPersistentHandler");

    lua_pushcfunction(L, lua_timer_unregister_persistent_handler);
    lua_setfield(L, -2, "UnregisterPersistentHandler");

    lua_pushcfunction(L, lua_timer_waitfor_persistent);
    lua_setfield(L, -2, "WaitForPersistent");

    lua_pushcfunction(L, lua_timer_cancel_persistent);
    lua_setfield(L, -2, "CancelPersistent");

    lua_pushcfunction(L, lua_timer_export_persistent);
    lua_setfield(L, -2, "ExportPersistent");

    lua_pushcfunction(L, lua_timer_import_persistent);
    lua_setfield(L, -2, "ImportPersistent");

    // Set as Ext.Timer
    lua_setfield(L, ext_table_idx, "Timer");

    LOG_TIMER_INFO("Registered Ext.Timer namespace");
}
