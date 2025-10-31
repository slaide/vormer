#include <string.h>
#include <stdlib.h>

#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include <util.h>
#include <system.h>

static xcb_atom_t xcb_get_atom(xcb_connection_t*con,const char*atom_name){
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(con, 0, strlen(atom_name), atom_name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(con, cookie, NULL);
    CHECK(reply!=nullptr,"failed to get atom for %s\n",atom_name);
    xcb_atom_t atom=reply->atom;
    free(reply);
    return atom;
}

void System_create(struct SystemCreateInfo*create_info,struct System*system){
    discard create_info;

    xcb_connection_t*con=xcb_connect(nullptr,nullptr);
    CHECK(con!=nullptr,"failed to xcb connect");

    printf("established connection\n");

    *system=(struct System){
        .interface=SYSTEM_INTERFACE_XCB,
        .xcb={
            .con=con,
        },
    };
}
void System_destroy(struct System*system){
    switch(system->interface){
        case SYSTEM_INTERFACE_XCB:
            xcb_disconnect(system->xcb.con);
            xcb_flush(system->xcb.con);
            break;
        default:
            CHECK(false,"unimplemented\n");
    }
}

void Window_create(
    struct WindowCreateInfo*info,
    struct Window*window
){
    switch(info->system->interface){
        case SYSTEM_INTERFACE_XCB:{
            xcb_connection_t*con=info->system->xcb.con;

            const xcb_setup_t*setup=xcb_get_setup(con);
            xcb_screen_iterator_t screen_iter=xcb_setup_roots_iterator(setup);
            // screen = screen_iter.data

            printf(
                "black_pixel: 0x%06x, white_pixel: 0x%06x, depth: %u\n", 
                screen_iter.data->black_pixel,
                screen_iter.data->white_pixel,
                screen_iter.data->root_depth
            );

            int window_id=xcb_generate_id(con);

            uint32_t value_mask = XCB_CW_EVENT_MASK | XCB_CW_BACK_PIXEL;
            uint32_t value_list[] = {
                screen_iter.data->black_pixel,

                XCB_EVENT_MASK_STRUCTURE_NOTIFY
                | XCB_EVENT_MASK_EXPOSURE
                | XCB_EVENT_MASK_POINTER_MOTION
                | XCB_EVENT_MASK_BUTTON_PRESS
                | XCB_EVENT_MASK_BUTTON_RELEASE
                | XCB_EVENT_MASK_KEY_PRESS
                | XCB_EVENT_MASK_KEY_RELEASE
                | XCB_EVENT_MASK_FOCUS_CHANGE
                | XCB_EVENT_MASK_STRUCTURE_NOTIFY
                | XCB_EVENT_MASK_ENTER_WINDOW
                | XCB_EVENT_MASK_LEAVE_WINDOW
            };
            xcb_create_window(
                con,
                screen_iter.data->root_depth,
                window_id,
                screen_iter.data->root,
                0,0,
                info->width,info->height,
                10,
                XCB_WINDOW_CLASS_INPUT_OUTPUT,
                screen_iter.data->root_visual,
                value_mask,
                value_list
            );

            // Set up WM_DELETE_WINDOW for close button detection
            int wm_protocols=xcb_get_atom(con,"WM_PROTOCOLS");
            int wm_delete_window=xcb_get_atom(con,"WM_DELETE_WINDOW");
            printf("wm_delete_window %d\n",wm_delete_window);

            xcb_change_property(
                con, 
                XCB_PROP_MODE_REPLACE, 
                window_id,
                wm_protocols, 
                XCB_ATOM_ATOM, 
                32, 
                1,
                &wm_delete_window
            );

            if(info->title){
                int wm_name=xcb_get_atom(con,"WM_NAME");

                int title_len=strlen(info->title);
                xcb_change_property(
                    con,
                    XCB_PROP_MODE_REPLACE,
                    window_id,
                    wm_name,
                    XCB_ATOM_STRING,
                    8,
                    title_len,
                    info->title
                );
            }

            // Set window type to normal application window (enables minimize/maximize)
            // Other window types:
            // _NET_WM_WINDOW_TYPE_NORMAL         <- centered, only "close" decoration
            // _NET_WM_WINDOW_TYPE_DIALOG         <- centered, only "close" decoration
            // _NET_WM_WINDOW_TYPE_UTILITY        <- doesnt work
            // _NET_WM_WINDOW_TYPE_SPLASH         <- centered, no decoration
            // _NET_WM_WINDOW_TYPE_TOOLBAR        <- doesnt work
            // _NET_WM_WINDOW_TYPE_MENU           <- offset, only "close" decoration
            // _NET_WM_WINDOW_TYPE_DROPDOWN_MENU  <- offset, no decoration
            // _NET_WM_WINDOW_TYPE_POPUP_MENU     <- offset, no decoration
            // _NET_WM_WINDOW_TYPE_TOOLTIP        <- offset, no decoration
            // _NET_WM_WINDOW_TYPE_NOTIFICATION
            // _NET_WM_WINDOW_TYPE_COMBO
            // _NET_WM_WINDOW_TYPE_DND
            // _NET_WM_WINDOW_TYPE_DOCK
            xcb_atom_t net_wm_window_type=xcb_get_atom(con,"_NET_WM_WINDOW_TYPE");
            xcb_atom_t net_wm_window_type_normal=xcb_get_atom(con,"_NET_WM_WINDOW_TYPE_NORMAL");
            xcb_change_property(
                con,
                XCB_PROP_MODE_REPLACE,
                window_id,
                net_wm_window_type,
                XCB_ATOM_ATOM,
                32,
                1,
                &net_wm_window_type_normal
            );

            struct {
                unsigned flags;
                unsigned functions;
                unsigned decorations;
                int input_mode;
                unsigned status;
            } mwm_hints={
                1<<1,
                0,
                1,
                0,
                0
            };
            int motif_mw_hints=xcb_get_atom(con, "_MOTIF_WM_HINTS");
            xcb_change_property(
                con,
                XCB_PROP_MODE_REPLACE,
                window_id,
                motif_mw_hints,
                motif_mw_hints,
                32,
                5,
                &mwm_hints
            );

            xcb_atom_t net_wm_allowed_actions = xcb_get_atom(con, "_NET_WM_ALLOWED_ACTIONS");
            xcb_atom_t actions[] = {
                xcb_get_atom(con, "_NET_WM_ACTION_MINIMIZE"),
                xcb_get_atom(con, "_NET_WM_ACTION_MAXIMIZE_HORZ"),
                xcb_get_atom(con, "_NET_WM_ACTION_MAXIMIZE_VERT"),
                xcb_get_atom(con, "_NET_WM_ACTION_CLOSE"),
                xcb_get_atom(con, "_NET_WM_ACTION_MOVE"),
                xcb_get_atom(con, "_NET_WM_ACTION_RESIZE")
            };

            xcb_change_property(
                con,
                XCB_PROP_MODE_REPLACE,
                window_id,
                net_wm_allowed_actions,
                XCB_ATOM_ATOM,
                32,
                6,  // number of actions
                actions
            );

            xcb_map_window(con, window_id);
            xcb_flush(con);

            *window=(struct Window){
                .system=info->system,

                .width=info->width,
                .height=info->height,

                .xcb={
                    .id=window_id,
                    .close_msg_data=wm_delete_window,
                },
            };
        }
            break;

        default:
            CHECK(false,"unimplemented");
    }
}
void Window_destroy(
    struct Window*window
){
    switch(window->system->interface){
        case SYSTEM_INTERFACE_XCB:
            xcb_destroy_window(window->system->xcb.con,window->xcb.id);
            xcb_flush(window->system->xcb.con);
            break;
        default:CHECK(false,"unimplemented");
    }
}
