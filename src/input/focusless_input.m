/**
 * focusless_input.m - In-process input injection without focus
 *
 * Dispatches NSEvent keyDown/keyUp to [NSApp sendEvent:] on the main
 * thread via GCD.  Because the BG3SE dylib is loaded inside BG3's
 * address space, this reaches AppKit's event chain even when BG3 is
 * not the frontmost application.
 *
 * The splash auto-dismiss timer fires Space at a configurable interval
 * and stops when focusless_input_mark_socket_ready() is called or
 * the duration expires.
 */

#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#include <dispatch/dispatch.h>
#include <os/log.h>

#include "focusless_input.h"
#include "../core/logging.h"
#include "../imgui/imgui_metal_backend.h"

static bool s_initialized = false;
static dispatch_source_t s_splash_timer = NULL;
static bool s_socket_ready = false;
static int s_dismiss_count = 0;
static int s_max_dismiss = 0;

bool focusless_input_init(void) {
    if (s_initialized) return true;
    s_initialized = true;
    s_socket_ready = false;
    s_dismiss_count = 0;
    LOG_CORE_INFO("[FocuslessInput] Initialized");
    return true;
}

void focusless_input_shutdown(void) {
    focusless_input_mark_socket_ready();
    s_initialized = false;
    LOG_CORE_INFO("[FocuslessInput] Shutdown");
}

static bool try_cgevent_hid(uint16_t keyCode) {
    CGEventRef down = CGEventCreateKeyboardEvent(NULL, keyCode, true);
    CGEventRef up = CGEventCreateKeyboardEvent(NULL, keyCode, false);
    if (!down || !up) {
        if (down) CFRelease(down);
        if (up)   CFRelease(up);
        return false;
    }

    CGEventPost(kCGHIDEventTap, down);
    usleep(50000);
    CGEventPost(kCGHIDEventTap, up);

    CFRelease(down);
    CFRelease(up);
    return true;
}

static bool try_cgevent_to_self(uint16_t keyCode, uint32_t modifiers __attribute__((unused))) {
    pid_t pid = getpid();

    CGEventRef down = CGEventCreateKeyboardEvent(NULL, keyCode, true);
    CGEventRef up = CGEventCreateKeyboardEvent(NULL, keyCode, false);
    if (!down || !up) {
        if (down) CFRelease(down);
        if (up)   CFRelease(up);
        return false;
    }

    CGEventPostToPid(pid, down);
    usleep(30000);
    CGEventPostToPid(pid, up);

    CFRelease(down);
    CFRelease(up);
    return true;
}

static void try_nsapp_send_event(uint16_t keyCode, uint32_t modifiers) {
    NSWindow *keyWin = [NSApp keyWindow];
    if (!keyWin) keyWin = [NSApp mainWindow];
    if (!keyWin) {
        NSArray *windows = [NSApp windows];
        for (NSWindow *w in windows) {
            if ([w isVisible]) { keyWin = w; break; }
        }
    }

    NSEventModifierFlags nsFlags = 0;
    if (modifiers & (1 << 0)) nsFlags |= NSEventModifierFlagShift;
    if (modifiers & (1 << 1)) nsFlags |= NSEventModifierFlagControl;
    if (modifiers & (1 << 2)) nsFlags |= NSEventModifierFlagOption;
    if (modifiers & (1 << 3)) nsFlags |= NSEventModifierFlagCommand;

    NSEvent *down = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                     location:NSMakePoint(0, 0)
                                modifierFlags:nsFlags
                                    timestamp:[[NSProcessInfo processInfo] systemUptime]
                                 windowNumber:keyWin ? [keyWin windowNumber] : 0
                                      context:nil
                                   characters:@" "
                  charactersIgnoringModifiers:@" "
                                    isARepeat:NO
                                      keyCode:keyCode];

    NSEvent *up = [NSEvent keyEventWithType:NSEventTypeKeyUp
                                   location:NSMakePoint(0, 0)
                              modifierFlags:nsFlags
                                  timestamp:[[NSProcessInfo processInfo] systemUptime] + 0.01
                               windowNumber:keyWin ? [keyWin windowNumber] : 0
                                    context:nil
                                 characters:@" "
                charactersIgnoringModifiers:@" "
                                  isARepeat:NO
                                    keyCode:keyCode];

    if (down) [NSApp sendEvent:down];
    if (up)   [NSApp sendEvent:up];
}

bool focusless_input_post_key_press(uint16_t keyCode, uint32_t modifiers) {
    if (!s_initialized) return false;

    // CGEvent can be posted from any thread — do it immediately so it
    // works even when the main thread is blocked by Bink video playback.
    bool cg_ok = try_cgevent_to_self(keyCode, modifiers);

    // Also try the global HID tap (reaches BG3 even without focus)
    try_cgevent_hid(keyCode);

    // NSEvent fallback requires AppKit on the main thread
    if (imgui_metal_first_drawable_seen() && !s_socket_ready) {
        dispatch_async(dispatch_get_main_queue(), ^{
            @autoreleasepool {
                try_nsapp_send_event(keyCode, modifiers);
            }
        });
    }

    LOG_CORE_DEBUG("[FocuslessInput] Posted key %d (CG=%s, Metal=%s, attempt=%d)",
                  keyCode, cg_ok ? "yes" : "no",
                  imgui_metal_first_drawable_seen() ? "yes" : "no",
                  s_dismiss_count);

    return true;
}

void focusless_input_start_splash_autodismiss(double duration, double interval) {
    if (!s_initialized) return;
    if (s_splash_timer) return;

    s_socket_ready = false;
    s_dismiss_count = 0;
    s_max_dismiss = (int)(duration / interval);

    LOG_CORE_INFO("[FocuslessInput] Splash auto-dismiss started (%.0fs duration, %.1fs interval, max %d attempts)",
                  duration, interval, s_max_dismiss);

    // Use global queue — main queue is blocked during Bink video playback
    dispatch_queue_t queue = dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0);
    s_splash_timer = dispatch_source_create(
        DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);

    uint64_t interval_ns = (uint64_t)(interval * NSEC_PER_SEC);
    uint64_t start_delay_ns = (uint64_t)(3.0 * NSEC_PER_SEC);
    dispatch_source_set_timer(s_splash_timer,
                              dispatch_time(DISPATCH_TIME_NOW, start_delay_ns),
                              interval_ns, interval_ns / 10);

    dispatch_source_set_event_handler(s_splash_timer, ^{
        if (s_socket_ready || s_dismiss_count >= s_max_dismiss) {
            LOG_CORE_INFO("[FocuslessInput] Splash timer stopped (%s, %d attempts)",
                          s_socket_ready ? "socket ready" : "max reached", s_dismiss_count);
            dispatch_source_cancel(s_splash_timer);
            s_splash_timer = NULL;
            return;
        }

        s_dismiss_count++;
        if (imgui_metal_first_drawable_seen()) {
            LOG_CORE_INFO("[FocuslessInput] Dismiss attempt #%d (Escape+Space, Metal ready)", s_dismiss_count);
        } else {
            LOG_CORE_INFO("[FocuslessInput] Dismiss attempt #%d (Escape+Space, pre-Metal)", s_dismiss_count);
        }

        // kVK_Escape = 0x35 (skips Bink intro videos)
        focusless_input_post_key_press(0x35, 0);

        // kVK_Space = 0x31 (dismisses "Press Any Key" splash)
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(200 * NSEC_PER_MSEC)),
                       dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
            focusless_input_post_key_press(0x31, 0);
        });
    });

    dispatch_source_set_cancel_handler(s_splash_timer, ^{
        LOG_CORE_DEBUG("[FocuslessInput] Timer cancelled");
    });

    dispatch_resume(s_splash_timer);
}

void focusless_input_mark_socket_ready(void) {
    s_socket_ready = true;
    if (s_splash_timer) {
        LOG_CORE_INFO("[FocuslessInput] Socket ready — stopping splash timer after %d attempts",
                      s_dismiss_count);
        dispatch_source_cancel(s_splash_timer);
        s_splash_timer = NULL;
    }
}
