#pragma once

#include <xcb/xcb.h>

enum SYSTEM_INTERFACE{
    SYSTEM_INTERFACE_XCB,
    SYSTEM_INTERFACE_WAYLAND,
};

struct System{
    enum SYSTEM_INTERFACE interface;
    union{
        struct XcbSystem{
            xcb_connection_t*con;
        }xcb;
    };
};
struct SystemCreateInfo{
    int _ignored;
};
void System_create(struct SystemCreateInfo*create_info,struct System*system);
void System_destroy(struct System*system);

struct Window{
    struct System*system;

    int height,width;

    struct XcbWindow{
        // xcb id
        int id;
        // WM_DELETE_WINDOW atom, which is sent when the window is closed via the X (close) button
        unsigned close_msg_data;
    }xcb;
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
