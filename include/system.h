#pragma once

#include <xcb/xcb.h>

enum SYSTEM_INTERFACE{
    SYSTEM_INTERFACE_XCB,
    SYSTEM_INTERFACE_WAYLAND,
};

enum BUTTON{
    BUTTON_UNKNOWN=0,

    BUTTON_LEFT,
    BUTTON_MIDDLE,
    BUTTON_RIGHT,
    BUTTON_4,
    BUTTON_5,
};
enum KEY{
    KEY_ESCAPE,
};

enum EVENT_KIND{
    // indicates no event present
    EVENT_KIND_NONE=0,

    // some event has been triggered that is ignored for whatever reason
    // i.e. there may currently be more events queued that can be polled for.
    EVENT_KIND_IGNORED=1,

    EVENT_KIND_KEY_PRESS,
    EVENT_KIND_KEY_RELEASE,

    EVENT_KIND_BUTTON_PRESS,
    EVENT_KIND_BUTTON_RELEASE,

    EVENT_KIND_POINTER_MOVE,

    EVENT_KIND_FOCUS_GAINED,
    EVENT_KIND_FOCUS_LOST,

    EVENT_KIND_WINDOW_CLOSED,
    EVENT_KIND_WINDOW_RESIZED,
};

struct Event{
    // device that generated this event
    void*device;
    // time in s since start of application at which this event was registered
    float time;

    enum EVENT_KIND kind;

    union{
        struct {
            enum KEY key;
        } key_press;
        struct {
            enum KEY key;
        } key_release;

        struct {
            enum BUTTON button;
        } button_press;
        struct {
            enum BUTTON button;
        } button_release;

        struct {
            // fraction of window width, relative to window origin at bottom left (excluding title bar)
            float x;
            // fraction of window height, relative to window origin at bottom left (excluding title bar)
            float y;
        } pointer_move;

        struct {int _unused;} focus_gained;
        struct {int _unused;} focus_lost;

        struct {int _unused;} winow_close_requested;

        struct {
            int old_width;
            int old_height;
            int new_width;
            int new_height;
        } window_resize;
    };
};

struct System{
    enum SYSTEM_INTERFACE interface;
    union{
        struct XcbSystem{
            xcb_connection_t*con;

            int num_open_windows;
            struct XcbWindow**windows;

            bool useXinput2;
        }xcb;
    };
};
struct SystemCreateInfo{
    // enable extended input events
    bool xcb_enableXinput2;
};
void System_create(struct SystemCreateInfo*create_info,struct System*system);
void System_destroy(struct System*system);
void System_pollEvent(struct System*system,struct Event*event);

struct Window{
    struct System*system;

    struct XcbWindow{
        // xcb id
        int id;
        // WM_DELETE_WINDOW atom, which is sent when the window is closed via the X (close) button
        unsigned close_msg_data;
        int xi_opcode;

        int height,width;
    }*xcb;
};
struct WindowCreateInfo{
    struct System*system;

    int width,height;

    const char*title;
};
void Window_create(
    struct WindowCreateInfo*info,
    struct Window*window
);
void Window_destroy(
    struct Window*window
);
