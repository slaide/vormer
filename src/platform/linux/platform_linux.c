#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef USE_X11
#define VK_USE_PLATFORM_XCB_KHR
#include <xcb/xcb.h>
#endif
#include <vulkan/vulkan.h>

#include "platform/platform.h"

/* X11 atoms */
typedef struct {
    xcb_atom_t wm_delete_window;
    xcb_atom_t wm_protocols;
    xcb_atom_t net_wm_name;
    xcb_atom_t utf8_string;
} X11Atoms;

/* Opaque structure definitions for the platform layer */
struct Platform {
    xcb_connection_t* xcb_conn;
    xcb_screen_t* screen;
    X11Atoms atoms;
    float dpi_x;
    float dpi_y;
    PlatformWindow* windows[64];  /* Track created windows for event dispatching */
    int window_count;
};

struct PlatformWindow {
    Platform* platform;  /* Reference back to platform for surface creation */
    xcb_window_t xcb_window;
    int width;
    int height;
    bool visible;
    int last_mouse_x;
    int last_mouse_y;
    bool close_requested;  /* Set to true when user clicks close button */
};

/* Helper function to get an atom by name */
static xcb_atom_t get_atom(xcb_connection_t* conn, const char* name) {
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, 0, strlen(name), name);
    xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(conn, cookie, NULL);
    if (!reply) {
        return XCB_ATOM_NONE;
    }
    xcb_atom_t atom = reply->atom;
    free(reply);
    return atom;
}

/* Helper function to find a window by XCB window ID */
static PlatformWindow* find_window_by_xcb_id(Platform* platform, xcb_window_t xcb_window) {
    for (int i = 0; i < platform->window_count; i++) {
        if (platform->windows[i] && platform->windows[i]->xcb_window == xcb_window) {
            return platform->windows[i];
        }
    }
    return NULL;
}

/* Translate XCB keycode to our KeyCode enum.
 * X11 keycodes start at 8, with keyboard layout-dependent mappings.
 * For now we use a simple mapping based on standard US QWERTY layout. */
static KeyCode translate_keycode(xcb_keycode_t keycode) {
    /* Standard US QWERTY keycode mappings (8-based) */
    switch (keycode) {
        /* Row 1 - numbers and symbols */
        case 49:  return KEY_GRAVE_ACCENT;  /* ` */
        case 10:  return KEY_1;
        case 11:  return KEY_2;
        case 12:  return KEY_3;
        case 13:  return KEY_4;
        case 14:  return KEY_5;
        case 15:  return KEY_6;
        case 16:  return KEY_7;
        case 17:  return KEY_8;
        case 18:  return KEY_9;
        case 19:  return KEY_0;
        case 20:  return KEY_MINUS;
        case 21:  return KEY_EQUAL;
        case 22:  return KEY_BACKSPACE;

        /* Row 2 - QWERTY row */
        case 23:  return KEY_TAB;
        case 24:  return KEY_Q;
        case 25:  return KEY_W;
        case 26:  return KEY_E;
        case 27:  return KEY_R;
        case 28:  return KEY_T;
        case 29:  return KEY_Y;
        case 30:  return KEY_U;
        case 31:  return KEY_I;
        case 32:  return KEY_O;
        case 33:  return KEY_P;
        case 34:  return KEY_LEFT_BRACKET;
        case 35:  return KEY_RIGHT_BRACKET;
        case 51:  return KEY_BACKSLASH;

        /* Row 3 - ASDFGH row */
        case 66:  return KEY_CAPS_LOCK;
        case 38:  return KEY_A;
        case 39:  return KEY_S;
        case 40:  return KEY_D;
        case 41:  return KEY_F;
        case 42:  return KEY_G;
        case 43:  return KEY_H;
        case 44:  return KEY_J;
        case 45:  return KEY_K;
        case 46:  return KEY_L;
        case 47:  return KEY_SEMICOLON;
        case 48:  return KEY_APOSTROPHE;
        case 36:  return KEY_ENTER;

        /* Row 4 - ZXCVBN row */
        case 50:  return KEY_LEFT_SHIFT;
        case 52:  return KEY_Z;
        case 53:  return KEY_X;
        case 54:  return KEY_C;
        case 55:  return KEY_V;
        case 56:  return KEY_B;
        case 57:  return KEY_N;
        case 58:  return KEY_M;
        case 59:  return KEY_COMMA;
        case 60:  return KEY_PERIOD;
        case 61:  return KEY_SLASH;
        case 62:  return KEY_RIGHT_SHIFT;

        /* Modifiers */
        case 37:  return KEY_LEFT_CONTROL;
        case 64:  return KEY_LEFT_ALT;
        case 65:  return KEY_SPACE;
        case 108: return KEY_RIGHT_ALT;
        case 105: return KEY_RIGHT_CONTROL;

        /* Function keys */
        case 9:   return KEY_ESCAPE;
        case 67:  return KEY_F1;
        case 68:  return KEY_F2;
        case 69:  return KEY_F3;
        case 70:  return KEY_F4;
        case 71:  return KEY_F5;
        case 72:  return KEY_F6;
        case 73:  return KEY_F7;
        case 74:  return KEY_F8;
        case 75:  return KEY_F9;
        case 76:  return KEY_F10;
        case 95:  return KEY_F11;
        case 96:  return KEY_F12;

        /* Navigation */
        case 110: return KEY_HOME;
        case 112: return KEY_PAGE_UP;
        case 115: return KEY_END;
        case 117: return KEY_PAGE_DOWN;
        case 113: return KEY_LEFT;
        case 111: return KEY_UP;
        case 114: return KEY_RIGHT;
        case 116: return KEY_DOWN;

        /* Special keys */
        case 118: return KEY_INSERT;
        case 119: return KEY_DELETE;
        case 107: return KEY_PRINT_SCREEN;
        case 77:  return KEY_NUM_LOCK;
        case 125: return KEY_SCROLL_LOCK;
        case 127: return KEY_PAUSE;

        /* Keypad (numpad) */
        case 79:  return KEY_KP_7;
        case 80:  return KEY_KP_8;
        case 81:  return KEY_KP_9;
        case 82:  return KEY_KP_SUBTRACT;
        case 83:  return KEY_KP_4;
        case 84:  return KEY_KP_5;
        case 85:  return KEY_KP_6;
        case 86:  return KEY_KP_ADD;
        case 87:  return KEY_KP_1;
        case 88:  return KEY_KP_2;
        case 89:  return KEY_KP_3;
        case 104: return KEY_KP_ENTER;
        case 90:  return KEY_KP_0;
        case 99:  return KEY_KP_MULTIPLY;
        case 106: return KEY_KP_DIVIDE;
        case 91:  return KEY_KP_DECIMAL;

        default:  return (KeyCode)0;  /* Unknown keycode */
    }
}

/* Get current time as double (seconds since epoch) */
static double get_current_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ============================================================================
 * Platform Initialization & Shutdown
 * ============================================================================ */

PlatformError platform_init(Platform** out_platform) {
    if (!out_platform) {
        return PLATFORM_ERROR_INVALID_ARG;
    }

    Platform* platform = (Platform*)calloc(1, sizeof(Platform));
    if (!platform) {
        return PLATFORM_ERROR_OUT_OF_MEMORY;
    }

    /* Connect to X11 display */
    int screen_num = 0;
    platform->xcb_conn = xcb_connect(NULL, &screen_num);
    if (xcb_connection_has_error(platform->xcb_conn)) {
        free(platform);
        return PLATFORM_ERROR_DISPLAY;
    }

    /* Get the default screen */
    const xcb_setup_t* setup = xcb_get_setup(platform->xcb_conn);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screen_num; ++i) {
        xcb_screen_next(&iter);
    }
    platform->screen = iter.data;

    if (!platform->screen) {
        xcb_disconnect(platform->xcb_conn);
        free(platform);
        return PLATFORM_ERROR_DISPLAY;
    }

    /* Fetch X11 atoms */
    platform->atoms.wm_delete_window = get_atom(platform->xcb_conn, "WM_DELETE_WINDOW");
    platform->atoms.wm_protocols = get_atom(platform->xcb_conn, "WM_PROTOCOLS");
    platform->atoms.net_wm_name = get_atom(platform->xcb_conn, "_NET_WM_NAME");
    platform->atoms.utf8_string = get_atom(platform->xcb_conn, "UTF8_STRING");


    /* Calculate DPI (default to 96 if we can't determine) */
    platform->dpi_x = 96.0f;
    platform->dpi_y = 96.0f;
    if (platform->screen->width_in_millimeters > 0) {
        platform->dpi_x = (platform->screen->width_in_pixels * 25.4f) / platform->screen->width_in_millimeters;
    }
    if (platform->screen->height_in_millimeters > 0) {
        platform->dpi_y = (platform->screen->height_in_pixels * 25.4f) / platform->screen->height_in_millimeters;
    }

    /* Initialize window tracking (already zeroed by calloc) */
    platform->window_count = 0;

    *out_platform = platform;
    return PLATFORM_OK;
}

void platform_shutdown(Platform* platform) {
    if (!platform) {
        return;
    }

    if (platform->xcb_conn) {
        xcb_disconnect(platform->xcb_conn);
    }

    free(platform);
}

/* ============================================================================
 * Window Management
 * ============================================================================ */

PlatformError platform_create_window(Platform* platform,
                                      const WindowConfig* config,
                                      PlatformWindow** out_window) {
    if (!platform || !config || !out_window) {
        return PLATFORM_ERROR_INVALID_ARG;
    }

    if (config->width <= 0 || config->height <= 0) {
        return PLATFORM_ERROR_INVALID_ARG;
    }

    PlatformWindow* window = (PlatformWindow*)calloc(1, sizeof(PlatformWindow));
    if (!window) {
        return PLATFORM_ERROR_OUT_OF_MEMORY;
    }

    window->platform = platform;
    window->width = config->width;
    window->height = config->height;
    window->visible = false;
    window->last_mouse_x = 0;
    window->last_mouse_y = 0;
    window->close_requested = false;

    /* Create the X11 window */
    window->xcb_window = xcb_generate_id(platform->xcb_conn);

    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[] = {
        platform->screen->black_pixel,
        XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_STRUCTURE_NOTIFY
    };

    xcb_create_window(platform->xcb_conn, XCB_COPY_FROM_PARENT, window->xcb_window,
                      platform->screen->root, 0, 0, config->width, config->height, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, platform->screen->root_visual, mask,
                      values);

    /* Set window title */
    if (config->title && config->title[0] != '\0') {
        xcb_change_property(platform->xcb_conn, XCB_PROP_MODE_REPLACE, window->xcb_window,
                            platform->atoms.net_wm_name, platform->atoms.utf8_string, 8,
                            strlen(config->title), config->title);

        xcb_change_property(platform->xcb_conn, XCB_PROP_MODE_REPLACE, window->xcb_window,
                            XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(config->title),
                            config->title);
    }

    /* Set WM_PROTOCOLS to handle window close events */
    xcb_change_property(platform->xcb_conn, XCB_PROP_MODE_REPLACE, window->xcb_window,
                        platform->atoms.wm_protocols, XCB_ATOM_ATOM, 32, 1,
                        &platform->atoms.wm_delete_window);

    /* Map the window to make it visible */
    xcb_map_window(platform->xcb_conn, window->xcb_window);

    xcb_flush(platform->xcb_conn);

    /* Track window for event dispatching */
    if (platform->window_count < 64) {
        platform->windows[platform->window_count++] = window;
    }

    *out_window = window;
    return PLATFORM_OK;
}

void platform_destroy_window(PlatformWindow* window) {
    if (!window) {
        return;
    }

    free(window);
}

void platform_show_window(PlatformWindow* window) {
    if (!window) {
        return;
    }

    /* Note: Window is already mapped during creation.
     * This function brings it to the front and requests focus from the window manager.
     * Actual implementation would involve:
     * - xcb_configure_window() with XCB_CONFIG_WINDOW_STACK_MODE to raise the window
     * - Sending WM_TAKE_FOCUS or using _NET_ACTIVE_WINDOW for focus requests
     * For now, this is a placeholder as the window is already visible. */
}

/* ============================================================================
 * Event Handling
 * ============================================================================ */

bool platform_poll_event(Platform* platform, Event* out_event) {
    if (!platform || !out_event) {
        return false;
    }

    /* Poll for next XCB event without blocking */
    xcb_generic_event_t* xcb_event = xcb_poll_for_event(platform->xcb_conn);
    if (!xcb_event) {
        return false;
    }

    /* Determine event type and dispatch */
    uint8_t event_type = xcb_event->response_type & ~0x80;

    /* Find the window this event belongs to */
    PlatformWindow* window = NULL;

    /* Initialize default event */
    *out_event = (Event){0};
    out_event->type = EVENT_NONE;
    out_event->timestamp = get_current_time();

    switch (event_type) {
        case XCB_KEY_PRESS: {
            xcb_key_press_event_t* key_ev = (xcb_key_press_event_t*)xcb_event;
            window = find_window_by_xcb_id(platform, key_ev->event);
            if (!window) break;

            out_event->type = EVENT_KEY_PRESS;
            out_event->keyboard.key = translate_keycode(key_ev->detail);
            out_event->keyboard.scancode = key_ev->detail;
            out_event->keyboard.mods.mods = (key_ev->state & 0x0F);  /* Extract modifier bits */
            break;
        }

        case XCB_KEY_RELEASE: {
            xcb_key_release_event_t* key_ev = (xcb_key_release_event_t*)xcb_event;
            window = find_window_by_xcb_id(platform, key_ev->event);
            if (!window) break;

            out_event->type = EVENT_KEY_RELEASE;
            out_event->keyboard.key = translate_keycode(key_ev->detail);
            out_event->keyboard.scancode = key_ev->detail;
            out_event->keyboard.mods.mods = (key_ev->state & 0x0F);
            break;
        }

        case XCB_BUTTON_PRESS: {
            xcb_button_press_event_t* btn_ev = (xcb_button_press_event_t*)xcb_event;
            window = find_window_by_xcb_id(platform, btn_ev->event);
            if (!window) break;

            /* XCB buttons: 1=left, 2=middle, 3=right, 4=scroll up, 5=scroll down, etc. */
            if (btn_ev->detail >= 4 && btn_ev->detail <= 5) {
                /* Mouse wheel scroll */
                out_event->type = EVENT_MOUSE_SCROLL;
                out_event->mouse_scroll.x = 0.0f;
                out_event->mouse_scroll.y = (btn_ev->detail == 4) ? 1.0f : -1.0f;
                out_event->mouse_scroll.precise = false;
            } else if (btn_ev->detail >= 1 && btn_ev->detail <= 3) {
                /* Mouse button press */
                out_event->type = EVENT_MOUSE_BUTTON_PRESS;
                out_event->mouse_button.button = (MouseButton)(btn_ev->detail - 1);  /* 1->0, 2->1, 3->2 */
                out_event->mouse_button.x = btn_ev->event_x;
                out_event->mouse_button.y = btn_ev->event_y;
                out_event->mouse_button.mods.mods = (btn_ev->state & 0x0F);
            }
            break;
        }

        case XCB_BUTTON_RELEASE: {
            xcb_button_release_event_t* btn_ev = (xcb_button_release_event_t*)xcb_event;
            window = find_window_by_xcb_id(platform, btn_ev->event);
            if (!window) break;

            if (btn_ev->detail >= 1 && btn_ev->detail <= 3) {
                /* Mouse button release */
                out_event->type = EVENT_MOUSE_BUTTON_RELEASE;
                out_event->mouse_button.button = (MouseButton)(btn_ev->detail - 1);
                out_event->mouse_button.x = btn_ev->event_x;
                out_event->mouse_button.y = btn_ev->event_y;
                out_event->mouse_button.mods.mods = (btn_ev->state & 0x0F);
            }
            break;
        }

        case XCB_MOTION_NOTIFY: {
            xcb_motion_notify_event_t* motion_ev = (xcb_motion_notify_event_t*)xcb_event;
            window = find_window_by_xcb_id(platform, motion_ev->event);
            if (!window) break;

            out_event->type = EVENT_MOUSE_MOVE;
            out_event->mouse_move.x = motion_ev->event_x;
            out_event->mouse_move.y = motion_ev->event_y;
            out_event->mouse_move.dx = motion_ev->event_x - window->last_mouse_x;
            out_event->mouse_move.dy = motion_ev->event_y - window->last_mouse_y;
            window->last_mouse_x = motion_ev->event_x;
            window->last_mouse_y = motion_ev->event_y;
            break;
        }

        case XCB_EXPOSE: {
            xcb_expose_event_t* expose_ev = (xcb_expose_event_t*)xcb_event;
            window = find_window_by_xcb_id(platform, expose_ev->window);
            if (!window) break;

            /* Treat expose as window resize for now (could be just a redraw hint) */
            out_event->type = EVENT_WINDOW_RESIZE;
            out_event->window_resize.width = window->width;
            out_event->window_resize.height = window->height;
            break;
        }

        case XCB_CONFIGURE_NOTIFY: {
            xcb_configure_notify_event_t* config_ev = (xcb_configure_notify_event_t*)xcb_event;
            window = find_window_by_xcb_id(platform, config_ev->window);
            if (!window) break;

            /* Check if size actually changed */
            if (config_ev->width != window->width || config_ev->height != window->height) {
                window->width = config_ev->width;
                window->height = config_ev->height;
                out_event->type = EVENT_WINDOW_RESIZE;
                out_event->window_resize.width = window->width;
                out_event->window_resize.height = window->height;
            }
            break;
        }

        case XCB_CLIENT_MESSAGE: {
            xcb_client_message_event_t* client_msg = (xcb_client_message_event_t*)xcb_event;
            window = find_window_by_xcb_id(platform, client_msg->window);
            if (!window) break;

            /* Check for WM_DELETE_WINDOW (user pressed the close button) */
            if (client_msg->type == platform->atoms.wm_protocols &&
                client_msg->data.data32[0] == platform->atoms.wm_delete_window) {
                window->close_requested = true;
                out_event->type = EVENT_WINDOW_CLOSE;
            }
            break;
        }

        default:
            /* Unknown event type */
            break;
    }

    free(xcb_event);
    return (out_event->type != EVENT_NONE);
}

bool platform_window_close_requested(PlatformWindow* window) {
    if (!window) {
        return false;
    }
    return window->close_requested;
}

void platform_get_window_size(PlatformWindow* window, int* width, int* height) {
    if (!window) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }

#ifdef USE_X11
    Platform* platform = window->platform;
    if (!platform || !platform->xcb_conn) {
        if (width) *width = window->width;
        if (height) *height = window->height;
        return;
    }

    /* Query current window geometry from X11 */
    xcb_get_geometry_cookie_t cookie = xcb_get_geometry(platform->xcb_conn, window->xcb_window);
    xcb_get_geometry_reply_t* reply = xcb_get_geometry_reply(platform->xcb_conn, cookie, NULL);
    if (reply) {
        window->width = reply->width;
        window->height = reply->height;
        free(reply);
    }
#endif

    if (width) *width = window->width;
    if (height) *height = window->height;
}

void platform_get_framebuffer_size(PlatformWindow* window, int* width, int* height) {
    /* On Linux with X11, framebuffer size is the same as window size (no high-DPI scaling) */
    platform_get_window_size(window, width, height);
}

/* ============================================================================
 * Vulkan Support
 * ============================================================================ */

PlatformError platform_create_vulkan_surface(PlatformWindow* window,
                                              VkInstance instance,
                                              VkSurfaceKHR* out_surface) {
    if (!window || !instance || !out_surface) {
        return PLATFORM_ERROR_INVALID_ARG;
    }

#ifdef USE_X11
    if (!window->platform || !window->platform->xcb_conn || !window->platform->screen) {
        return PLATFORM_ERROR_VULKAN;
    }

    /* Create XCB surface for Vulkan rendering */
    VkXcbSurfaceCreateInfoKHR surface_info = {
        .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
        .pNext = NULL,
        .flags = 0,
        .connection = window->platform->xcb_conn,
        .window = window->xcb_window,
    };

    VkResult result = vkCreateXcbSurfaceKHR(instance, &surface_info, NULL, out_surface);
    if (result != VK_SUCCESS) {
        return PLATFORM_ERROR_VULKAN;
    }

    return PLATFORM_OK;
#else
    /* Wayland support not yet implemented */
    return PLATFORM_ERROR_VULKAN;
#endif
}

PlatformError platform_get_required_vulkan_extensions(const char** out_extensions,
                                                       uint32_t* out_count) {
    if (!out_extensions || !out_count) {
        return PLATFORM_ERROR_INVALID_ARG;
    }

    static const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef USE_X11
        VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#endif
#ifdef USE_WAYLAND
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
#endif
    };

    *out_extensions = (const char*)extensions;
    *out_count = sizeof(extensions) / sizeof(extensions[0]);
    return PLATFORM_OK;
}

