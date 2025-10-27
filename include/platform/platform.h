#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

/* Opaque pointers to platform-specific structures */
typedef struct Platform Platform;
typedef struct PlatformWindow PlatformWindow;

/* ============================================================================
 * Error Codes
 * ============================================================================ */

/* Platform operation result codes. Used to indicate success or specific failure modes. */
typedef enum {
    PLATFORM_OK = 0,                  /* Operation succeeded */
    PLATFORM_ERROR_GENERIC = 1,       /* Generic/unspecified error */
    PLATFORM_ERROR_OUT_OF_MEMORY = 2, /* Memory allocation failed */
    PLATFORM_ERROR_INVALID_ARG = 3,   /* Invalid argument provided */
    PLATFORM_ERROR_DISPLAY = 4,       /* Display/window system error */
    PLATFORM_ERROR_VULKAN = 5,        /* Vulkan-related error */
} PlatformError;

/* ============================================================================
 * Input State
 * ============================================================================ */

/* Key/button state with transition awareness.
 * Distinguishes between steady state and recent transitions, allowing efficient
 * detection of "was just pressed" vs "is currently held" vs "was just released".
 *
 * Invariant: state > 0 means the key/button is UP (not pressed)
 * Invariant: state <= 0 means the key/button is DOWN (pressed)
 */
typedef enum {
    /* DOWN states (state <= 0) */
    KEY_STATE_DOWN = 0,           /* Key is held down (steady state) */
    KEY_STATE_JUST_PRESSED = -1,  /* Key changed from up to down in last event */

    /* UP states (state > 0) */
    KEY_STATE_UP = 1,             /* Key is released (steady state) */
    KEY_STATE_JUST_RELEASED = 2,  /* Key changed from down to up in last event */
} KeyState;

/* ============================================================================
 * Event System
 * ============================================================================ */

typedef enum {
    EVENT_NONE,

    /* Window events */
    EVENT_WINDOW_CLOSE,
    EVENT_WINDOW_RESIZE,
    EVENT_WINDOW_FOCUS_GAINED,
    EVENT_WINDOW_FOCUS_LOST,
    EVENT_WINDOW_MINIMIZED,
    EVENT_WINDOW_RESTORED,
    EVENT_WINDOW_DPI_CHANGED,

    /* Keyboard events */
    EVENT_KEY_PRESS,
    EVENT_KEY_RELEASE,
    EVENT_KEY_REPEAT,
    EVENT_TEXT_INPUT,

    /* Mouse events */
    EVENT_MOUSE_BUTTON_PRESS,
    EVENT_MOUSE_BUTTON_RELEASE,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_SCROLL,
    EVENT_MOUSE_ENTER,
    EVENT_MOUSE_LEAVE,

    /* Touch events (mobile) */
    EVENT_TOUCH_BEGIN,
    EVENT_TOUCH_MOVE,
    EVENT_TOUCH_END,
    EVENT_TOUCH_CANCEL,

    /* Gamepad events */
    EVENT_GAMEPAD_CONNECTED,
    EVENT_GAMEPAD_DISCONNECTED,
    EVENT_GAMEPAD_BUTTON_PRESS,
    EVENT_GAMEPAD_BUTTON_RELEASE,
    EVENT_GAMEPAD_AXIS_MOTION,

    /* System events */
    EVENT_QUIT
} EventType;

typedef enum {
    /* Printable keys */
    KEY_SPACE = 32,
    KEY_APOSTROPHE = 39,
    KEY_COMMA = 44,
    KEY_MINUS = 45,
    KEY_PERIOD = 46,
    KEY_SLASH = 47,
    KEY_0 = 48, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    KEY_SEMICOLON = 59,
    KEY_EQUAL = 61,
    KEY_A = 65, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
    KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
    KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
    KEY_LEFT_BRACKET = 91,
    KEY_BACKSLASH = 92,
    KEY_RIGHT_BRACKET = 93,
    KEY_GRAVE_ACCENT = 96,

    /* Function keys */
    KEY_ESCAPE = 256,
    KEY_ENTER,
    KEY_TAB,
    KEY_BACKSPACE,
    KEY_INSERT,
    KEY_DELETE,
    KEY_RIGHT,
    KEY_LEFT,
    KEY_DOWN,
    KEY_UP,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_HOME,
    KEY_END,
    KEY_CAPS_LOCK,
    KEY_SCROLL_LOCK,
    KEY_NUM_LOCK,
    KEY_PRINT_SCREEN,
    KEY_PAUSE,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,

    /* Keypad */
    KEY_KP_0 = 320, KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_4,
    KEY_KP_5, KEY_KP_6, KEY_KP_7, KEY_KP_8, KEY_KP_9,
    KEY_KP_DECIMAL,
    KEY_KP_DIVIDE,
    KEY_KP_MULTIPLY,
    KEY_KP_SUBTRACT,
    KEY_KP_ADD,
    KEY_KP_ENTER,
    KEY_KP_EQUAL,

    /* Modifiers */
    KEY_LEFT_SHIFT = 340,
    KEY_LEFT_CONTROL,
    KEY_LEFT_ALT,
    KEY_LEFT_SUPER,
    KEY_RIGHT_SHIFT,
    KEY_RIGHT_CONTROL,
    KEY_RIGHT_ALT,
    KEY_RIGHT_SUPER
} KeyCode;

typedef enum {
    MOUSE_BUTTON_LEFT = 0,
    MOUSE_BUTTON_RIGHT = 1,
    MOUSE_BUTTON_MIDDLE = 2,
    MOUSE_BUTTON_4 = 3,
    MOUSE_BUTTON_5 = 4
} MouseButton;

typedef enum {
    GAMEPAD_BUTTON_A = 0,
    GAMEPAD_BUTTON_B,
    GAMEPAD_BUTTON_X,
    GAMEPAD_BUTTON_Y,
    GAMEPAD_BUTTON_LEFT_BUMPER,
    GAMEPAD_BUTTON_RIGHT_BUMPER,
    GAMEPAD_BUTTON_BACK,
    GAMEPAD_BUTTON_START,
    GAMEPAD_BUTTON_GUIDE,
    GAMEPAD_BUTTON_LEFT_THUMB,
    GAMEPAD_BUTTON_RIGHT_THUMB,
    GAMEPAD_BUTTON_DPAD_UP,
    GAMEPAD_BUTTON_DPAD_RIGHT,
    GAMEPAD_BUTTON_DPAD_DOWN,
    GAMEPAD_BUTTON_DPAD_LEFT,
    GAMEPAD_BUTTON_EXTRA_1,
    GAMEPAD_BUTTON_EXTRA_2,
    GAMEPAD_BUTTON_EXTRA_3,
    GAMEPAD_BUTTON_EXTRA_4,
    GAMEPAD_BUTTON_EXTRA_5,
    GAMEPAD_BUTTON_EXTRA_6
} GamepadButton;

typedef enum {
    GAMEPAD_AXIS_LEFT_X = 0,
    GAMEPAD_AXIS_LEFT_Y,
    GAMEPAD_AXIS_RIGHT_X,
    GAMEPAD_AXIS_RIGHT_Y,
    GAMEPAD_AXIS_LEFT_TRIGGER,
    GAMEPAD_AXIS_RIGHT_TRIGGER,
    GAMEPAD_AXIS_EXTRA_1,
    GAMEPAD_AXIS_EXTRA_2,
    GAMEPAD_AXIS_EXTRA_3,
    GAMEPAD_AXIS_EXTRA_4
} GamepadAxis;

/* Keyboard modifier key flags for use in KeyboardModifiers.mods bitfield */
#define KEYBOARD_MOD_SHIFT 1
#define KEYBOARD_MOD_CTRL 2
#define KEYBOARD_MOD_ALT 4
#define KEYBOARD_MOD_SUPER 8

typedef struct {
    int mods;  /* Bitfield of KEYBOARD_MOD_* flags indicating which modifier keys are pressed */
} KeyboardModifiers;

typedef struct {
    EventType type;       /* Event type indicating which union field is valid */
    double timestamp;     /* Time in seconds when the event occurred */
    void* device;         /* Optional: pointer to the device that caused this event (e.g., keyboard, mouse, gamepad). NULL if not supported or not applicable for this event. */

    union {
        /* Window events */
        struct {
            int width;   /* New window width in logical (screen) coordinates */
            int height;  /* New window height in logical (screen) coordinates */
        } window_resize;

        struct {
            float xscale;  /* Horizontal DPI scale factor */
            float yscale;  /* Vertical DPI scale factor */
        } window_dpi;

        /* Keyboard events */
        struct {
            KeyCode key;              /* Logical key code */
            int scancode;             /* Physical key scancode (platform-dependent) */
            KeyboardModifiers mods;   /* Active modifier keys (shift, ctrl, alt, super) */
        } keyboard;

        struct {
            char text[32];  /* UTF-8 encoded text input, null-terminated */
        } text_input;

        /* Mouse events */
        struct {
            MouseButton button;       /* Which mouse button */
            int x, y;                 /* Absolute position in window coordinates */
            KeyboardModifiers mods;   /* Active modifier keys */
        } mouse_button;

        struct {
            int x, y;      /* Absolute position in window coordinates */
            int dx, dy;    /* Change in position since last mouse move event */
        } mouse_move;

        struct {
            float x, y;    /* Scroll delta (units depend on precise flag) */
            bool precise;  /* true=trackpad (smooth), false=wheel (discrete) */
        } mouse_scroll;

        /* Touch events */
        struct {
            int64_t touch_id;  /* Unique identifier for this touch point */
            float x, y;        /* Normalized coordinates [0, 1] relative to window */
            float pressure;    /* Pressure [0, 1] if supported, 0 otherwise */
        } touch;

        /* Gamepad events */
        struct {
            int gamepad_id;    /* Device index of the gamepad */
            const char* name;  /* Human-readable gamepad name */
        } gamepad_connection;

        struct {
            int gamepad_id;      /* Device index of the gamepad */
            GamepadButton button; /* Which button changed state */
        } gamepad_button;

        struct {
            int gamepad_id;   /* Device index of the gamepad */
            GamepadAxis axis; /* Which axis changed value */
            float value;      /* Axis value (sticks: [-1, 1], triggers: [0, 1]) */
        } gamepad_axis;
    };
} Event;

/* ============================================================================
 * Window Configuration
 * ============================================================================ */

/* Three-state boolean for window configuration options.
 * Allows specifying "use default" vs explicit true/false settings. */
typedef enum {
    WINDOW_DONTCARE = 0,  /* Use platform default behavior */
    WINDOW_FALSE = 1,     /* Explicitly disable the option */
    WINDOW_TRUE = 2       /* Explicitly enable the option */
} WindowOption;

typedef struct {
    const char* title;    /* Optional: window title. NULL or empty for default. Required field set to non-NULL to use custom title. */
    int width;            /* Required: window width in logical coordinates. Must be > 0. */
    int height;           /* Required: window height in logical coordinates. Must be > 0. */
    WindowOption fullscreen;  /* Optional: request fullscreen mode. Defaults to WINDOW_DONTCARE (platform-dependent). */
    WindowOption resizable;   /* Optional: allow window resizing. Defaults to WINDOW_DONTCARE (platform-dependent). */
    WindowOption decorated;   /* Optional: show window decorations (title bar, borders). Defaults to WINDOW_DONTCARE (platform-dependent). */
    WindowOption vsync;       /* Optional: enable vertical sync. Defaults to WINDOW_DONTCARE (platform-dependent). */
} WindowConfig;

/* ============================================================================
 * Cursor Management
 * ============================================================================ */

typedef enum {
    CURSOR_ARROW,
    CURSOR_IBEAM,
    CURSOR_CROSSHAIR,
    CURSOR_HAND,
    CURSOR_HRESIZE,
    CURSOR_VRESIZE,
    CURSOR_HIDDEN
} CursorType;

/* ============================================================================
 * Platform Initialization & Shutdown
 * ============================================================================ */

/* Initialize the platform layer.
 * out_platform: pointer where the initialized Platform will be written.
 * Returns PLATFORM_OK on success, or a PlatformError code on failure.
 * On failure, out_platform is not written to. */
PlatformError platform_init(Platform** out_platform);

/* Shutdown the platform layer and release all resources.
 * platform: the Platform instance to shutdown. Must have been returned by platform_init(). */
void platform_shutdown(Platform* platform);

/* ============================================================================
 * Window Management
 * ============================================================================ */

/* Create a window with the given configuration.
 * platform: the Platform instance (must be initialized).
 * config: pointer to WindowConfig specifying window properties.
 * out_window: pointer where the created PlatformWindow will be written.
 * Returns PLATFORM_OK on success, or a PlatformError code on failure.
 * On failure, out_window is not written to. */
PlatformError platform_create_window(Platform* platform,
                                      const WindowConfig* config,
                                      PlatformWindow** out_window);

/* Destroy a window and release its resources. */
void platform_destroy_window(PlatformWindow* window);

/* Set the window title. */
void platform_set_window_title(PlatformWindow* window, const char* title);

/* Get the logical window size (in screen coordinates). */
void platform_get_window_size(PlatformWindow* window, int* width, int* height);

/* Get the framebuffer size (in pixels, may differ from window size on high-DPI displays). */
void platform_get_framebuffer_size(PlatformWindow* window, int* width,
                                    int* height);

/* Set the logical window size. */
void platform_set_window_size(PlatformWindow* window, int width, int height);

/* Set fullscreen mode. */
void platform_set_fullscreen(PlatformWindow* window, bool fullscreen);

/* Bring the window to the front and draw attention to it.
 * The window is always mapped on creation, but this brings it to the top of the window stack
 * and may request focus from the window manager or other OS-specific attention mechanisms. */
void platform_show_window(PlatformWindow* window);

/* Hide the window. */
void platform_hide_window(PlatformWindow* window);

/* Focus the window (bring to front). */
void platform_focus_window(PlatformWindow* window);

/* Get content scale factor (DPI scaling). */
float platform_get_content_scale(PlatformWindow* window);

/* Get monitor DPI. */
void platform_get_monitor_dpi(PlatformWindow* window, float* xdpi, float* ydpi);

/* ============================================================================
 * Event Handling
 * ============================================================================ */

/* Poll for the next event. Returns true if an event was available, false otherwise. */
bool platform_poll_event(Platform* platform, Event* out_event);

/* Check if a window has been requested to close (e.g., user clicked the close button).
 * window: the PlatformWindow to check.
 * Returns true if the window has a pending close request, false otherwise. */
bool platform_window_close_requested(PlatformWindow* window);

/* ============================================================================
 * Input State Queries
 * ============================================================================ */

/* Get the current state of a key.
 * platform: the Platform instance (must be initialized).
 * key: the KeyCode to query.
 * Returns a KeyState value indicating the key's state and any recent transitions.
 * Use (state <= 0) to check if pressed, (state > 0) to check if released. */
KeyState platform_get_key_state(Platform* platform, KeyCode key);

/* Get the current state of a mouse button.
 * platform: the Platform instance (must be initialized).
 * button: the MouseButton to query.
 * Returns a KeyState value indicating the button's state and any recent transitions.
 * Use (state <= 0) to check if pressed, (state > 0) to check if released. */
KeyState platform_get_mouse_button_state(Platform* platform, MouseButton button);

/* Get the current mouse position in screen coordinates. */
void platform_get_mouse_position(Platform* platform, int* x, int* y);

/* Get the mouse delta (change) since last frame. */
void platform_get_mouse_delta(Platform* platform, int* dx, int* dy);

/* Check if a gamepad is connected. */
bool platform_is_gamepad_connected(Platform* platform, int gamepad_id);

/* Check if a gamepad button is currently pressed. */
bool platform_is_gamepad_button_down(Platform* platform, int gamepad_id,
                                      GamepadButton button);

/* Get gamepad axis value (stick or trigger). */
float platform_get_gamepad_axis(Platform* platform, int gamepad_id,
                                GamepadAxis axis);

/* Get gamepad name.
 * platform: the Platform instance (must be initialized).
 * gamepad_id: the device index to query.
 * out_name: pointer where the gamepad name will be written (valid pointer only on success).
 * Returns PLATFORM_OK on success, or a PlatformError code on failure (e.g., invalid gamepad_id).
 * On success, out_name points to a null-terminated string owned by the platform layer. */
PlatformError platform_get_gamepad_name(Platform* platform, int gamepad_id,
                                         const char** out_name);

/* ============================================================================
 * Cursor Management
 * ============================================================================ */

/* Set the cursor type. */
void platform_set_cursor(PlatformWindow* window, CursorType cursor);

/* Enable/disable cursor capture (relative mouse mode). */
void platform_set_cursor_capture(PlatformWindow* window, bool capture);

/* ============================================================================
 * Clipboard
 * ============================================================================ */

/* Get clipboard text. Returns NULL if empty or unavailable. */
const char* platform_get_clipboard_text(Platform* platform);

/* Set clipboard text. */
void platform_set_clipboard_text(Platform* platform, const char* text);

/* ============================================================================
 * Vulkan Integration
 * ============================================================================ */

/* Forward declare Vulkan types */
typedef struct VkSurfaceKHR_T* VkSurfaceKHR;
typedef struct VkInstance_T* VkInstance;

/* Create a Vulkan surface for this window.
 * window: the PlatformWindow to create a surface for.
 * instance: the VkInstance to use for surface creation.
 * out_surface: pointer where the created VkSurfaceKHR will be written.
 * Returns PLATFORM_OK on success, or a PlatformError code on failure.
 * On failure, out_surface is not written to. */
PlatformError platform_create_vulkan_surface(PlatformWindow* window,
                                              VkInstance instance,
                                              VkSurfaceKHR* out_surface);

/* Get required Vulkan instance extensions for this platform.
 * This function queries which Vulkan extensions are necessary for creating
 * a surface and presenting on the current platform.
 * out_extensions: pointer to array where extension names will be written.
 * out_count: pointer where the number of extensions will be written.
 * Returns PLATFORM_OK on success, or a PlatformError code on failure.
 * On failure, out_extensions and out_count are not written to. */
PlatformError platform_get_required_vulkan_extensions(const char** out_extensions,
                                                       uint32_t* out_count);

#endif /* PLATFORM_H */
