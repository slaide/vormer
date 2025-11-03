#include <string.h>
#include <stdlib.h>

#include <xcb/xcb.h>
#include <xcb/xinput.h>

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

    *system=(struct System){
        .interface=SYSTEM_INTERFACE_XCB,
        .xcb={
            .con=con,
            .num_open_windows=0,
            .windows=nullptr,
            .useXinput2=create_info->xcb_enableXinput2,
        },
    };
}
void System_destroy(struct System*system){
    switch(system->interface){
        case SYSTEM_INTERFACE_XCB:
            xcb_disconnect(system->xcb.con);
            xcb_flush(system->xcb.con);

            free(system->xcb.windows);

            break;
        default:
            CHECK(false,"unimplemented\n");
    }
}
static inline enum BUTTON xcbButton_to_vormerButton(int xcb_button){
    switch(xcb_button){
        case XCB_BUTTON_INDEX_1:
            return BUTTON_LEFT;
        case XCB_BUTTON_INDEX_2:
            return BUTTON_MIDDLE;
        case XCB_BUTTON_INDEX_3:
            return BUTTON_RIGHT;
        case XCB_BUTTON_INDEX_4:
            return BUTTON_4;
        case XCB_BUTTON_INDEX_5:
            return BUTTON_5;
        default:
            return BUTTON_UNKNOWN;
    }
}
static inline float fp1616_to_float(xcb_input_fp1616_t fp1616){
    float maj=(float)(fp1616>>16);
    float min=(float)(fp1616&0xFFFF)/(float)(0xFFFF);
    return maj+min;
}
static inline double fp3232_to_float(xcb_input_fp3232_t fp3232){
    return fp3232.integral + (fp3232.frac / (double)(1ULL << 32));
}
void System_pollEvent(struct System*system,struct Event*event){
    *event=(struct Event){};

    xcb_flush(system->xcb.con);

    xcb_generic_event_t*xcb_event=xcb_poll_for_event(system->xcb.con);
    if(!xcb_event)
        return;
    
    uint8_t event_type = xcb_event->response_type & ~0x80;
    switch(event_type){
        case XCB_KEY_PRESS:
            {
                auto xevent=(xcb_key_press_event_t*)xcb_event;
                printf("key press %d\n",xevent->detail);

                *event=(struct Event){
                    .kind=EVENT_KIND_KEY_PRESS,
                    .key_press={
                        
                    },
                };
            }
            break;
        case XCB_KEY_RELEASE:
            {
                auto xevent=(xcb_key_release_event_t*)xcb_event;
                printf("key release %d\n",xevent->detail);

                *event=(struct Event){
                    .kind=EVENT_KIND_KEY_RELEASE,
                    .key_release={

                    },
                };
            }
            break;

        case XCB_BUTTON_PRESS:
            {
                auto xevent=(xcb_button_press_event_t*)xcb_event;

                enum BUTTON vormer_button=xcbButton_to_vormerButton(xevent->detail);

                *event=(struct Event){
                    .kind=EVENT_KIND_BUTTON_PRESS,
                    .button_press={
                        .button=vormer_button,
                    },
                };
            }
            break;
        case XCB_BUTTON_RELEASE:
            {
                auto xevent=(xcb_button_release_event_t*)xcb_event;

                enum BUTTON vormer_button=xcbButton_to_vormerButton(xevent->detail);

                *event=(struct Event){
                    .kind=EVENT_KIND_BUTTON_RELEASE,
                    .button_release={
                        .button=vormer_button,
                    },
                };
            }
            break;

        case XCB_MOTION_NOTIFY:
            {
                auto xevent=(xcb_motion_notify_event_t*)xcb_event;

                if(system->xcb.num_open_windows==0){
                    event->kind=EVENT_KIND_IGNORED;
                    break;
                }

                auto window=system->xcb.windows[0];

                *event=(struct Event){
                    .kind=EVENT_KIND_POINTER_MOVE,
                    .pointer_move={
                        .x=(float)xevent->event_x/(float)window->width,
                        .y=(float)(window->height-xevent->event_y)/(float)window->height,
                    },
                };
            }
            break;

        case XCB_ENTER_NOTIFY:
            {
                printf("cursor entered the window\n");
            }
            break;
        case XCB_LEAVE_NOTIFY:
            {
                printf("cursor left the window\n");
            }
            break;

        case XCB_FOCUS_IN:
            {
                *event=(struct Event){
                    .kind=EVENT_KIND_FOCUS_GAINED,
                };
            }
            break;
        case XCB_FOCUS_OUT:
            {
                *event=(struct Event){
                    .kind=EVENT_KIND_FOCUS_LOST,
                };
            }
            break;

            printf("got known event\n");
            break;
    
        case XCB_CONFIGURE_NOTIFY:
            {
                auto xevent=(xcb_configure_notify_event_t*)xcb_event;
                // window might have been resized, moved.. or maybe something else.
                // we only care about resize though
                if(system->xcb.num_open_windows==0){
                    event->kind=EVENT_KIND_IGNORED;
                    printf("ignoring configure (1)\n");
                    break;
                }

                auto window=system->xcb.windows[0];

                int
                    oldsize[2]={
                        [0]=window->width,
                        [1]=window->height
                    },
                    newsize[2]={
                        [0]=xevent->width,
                        [1]=xevent->height
                    };

                if(window->height==xevent->height && window->width==xevent->width){
                    event->kind=EVENT_KIND_IGNORED;
                    printf("ignoring configure (2)\n");
                    break;
                }

                *event=(struct Event){
                    .kind=EVENT_KIND_WINDOW_RESIZED,
                    .window_resize={
                        .old_width=oldsize[0],
                        .old_height=oldsize[1],
                        .new_width=newsize[0],
                        .new_height=newsize[1],
                    }
                };
            }
            break;

        case XCB_EXPOSE:
        case XCB_KEYMAP_NOTIFY:
        case XCB_REPARENT_NOTIFY:
        case XCB_MAP_NOTIFY:
        case XCB_PROPERTY_NOTIFY:
        case XCB_COLORMAP_NOTIFY:
        case XCB_VISIBILITY_NOTIFY:
            // ignored
            event->kind=EVENT_KIND_IGNORED;
            break;

        case XCB_CLIENT_MESSAGE:
            {
                xcb_client_message_event_t*xevent=(xcb_client_message_event_t*)xcb_event;

                if(system->xcb.num_open_windows==0){
                    event->kind=EVENT_KIND_IGNORED;
                    break;
                }

                auto window=system->xcb.windows[0];

                if(xevent->format==32 && xevent->data.data32[0]==window->close_msg_data){
                    *event=(struct Event){
                        .kind=EVENT_KIND_WINDOW_CLOSED,
                    };
                }
            }
            break;

        case XCB_GE_GENERIC:
            {
                printf("got better input event\n");

                if(system->xcb.num_open_windows==0){
                    event->kind=EVENT_KIND_IGNORED;
                    break;
                }

                auto window=system->xcb.windows[0];

                auto xevent=(struct xcb_ge_generic_event_t*)xcb_event;
                if(xevent->extension!=window->xi_opcode){
                    event->kind=EVENT_KIND_IGNORED;
                    break;
                }

                // https://xcb.freedesktop.org/manual/xinput_8h_source.html
                switch(xevent->event_type){
                    case XCB_INPUT_MOTION:
                        {
                            auto xi_event=(xcb_input_motion_event_t*)xevent;

                            // Get valuator mask and values (using button_press accessors since motion is a typedef)
                            uint32_t *valuator_mask = xcb_input_button_press_valuator_mask(xi_event);
                            xcb_input_fp3232_t *axisvalues = xcb_input_button_press_axisvalues(xi_event);
                            
                            // Track which valuators are present
                            int value_index = 0;
                            for (int i = 0; i < xi_event->valuators_len * 32; i++) {
                                // Check if this valuator is set in the mask
                                if (valuator_mask[i / 32] & (1 << (i % 32))) {
                                    double value = fp3232_to_float(axisvalues[value_index]);
                                    
                                    // Valuator 2 is typically vertical scroll
                                    // Valuator 3 is typically horizontal scroll
                                    if (i == 2) {
                                        printf("Vertical scroll value: %f\n", value);
                                    } else if (i == 3) {
                                        printf("Horizontal scroll value: %f\n", value);
                                    }
                                    
                                    value_index++;
                                }
                            }

                            float event_x=fp1616_to_float(xi_event->event_x);
                            float event_y=fp1616_to_float(xi_event->event_y);

                            *event=(struct Event){
                                .kind=EVENT_KIND_POINTER_MOVE,
                                .pointer_move={
                                    .x=event_x/(float)window->width,
                                    .y=(float)(window->height-event_y)/(float)window->height,
                                },
                            };
                        }
                        break;
                    case XCB_INPUT_SCROLL_TYPE_VERTICAL:
                    case XCB_INPUT_SCROLL_TYPE_HORIZONTAL:
                        {
                            //auto xi_event=(xcb_input_scroll_type_t*)xevent;

                            //printf("%d\n",xi_event);
                        }
                        break;
                    default:
                        printf("unhandled xi2 event %d\n", xevent->event_type);
                        event->kind=EVENT_KIND_IGNORED;
                        break;
                }
            }   
            break;

        default:
            printf("unhandled xcb event %d\n", event_type);
            // more invalid than ignored, but kinda comes out to the same result.
            // (we know there is something, but don't actually care what it is)
            event->kind=EVENT_KIND_IGNORED;
    }
    free(xcb_event);
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

            uint8_t xi_opcode={};

            if(info->system->xcb.useXinput2){
                // register xinput2 event handling
                xcb_query_extension_cookie_t xi_cookie=xcb_query_extension(con, strlen("XInputExtension"), "XInputExtension");
                xcb_query_extension_reply_t *xi_reply = xcb_query_extension_reply(
                    con,
                    xi_cookie,
                    NULL
                );
                xi_opcode = xi_reply->major_opcode;
                free(xi_reply);

                // register _which_ xinput2 events we want to handle
                struct {
                    xcb_input_event_mask_t head;
                    uint32_t mask;
                } mask;

                mask.head.deviceid = XCB_INPUT_DEVICE_ALL_MASTER;
                mask.head.mask_len = 1;
                mask.mask =
                    XCB_INPUT_XI_EVENT_MASK_MOTION
                    | XCB_INPUT_XI_EVENT_MASK_BUTTON_PRESS
                    | XCB_INPUT_XI_EVENT_MASK_BUTTON_RELEASE
                    | XCB_INPUT_XI_EVENT_MASK_FOCUS_IN
                    | XCB_INPUT_XI_EVENT_MASK_FOCUS_OUT
                    | XCB_INPUT_XI_EVENT_MASK_BUTTON_PRESS
                    | XCB_INPUT_XI_EVENT_MASK_BUTTON_RELEASE;

                xcb_input_xi_select_events(con, window_id, 1, &mask.head);
            }

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

            auto xcb_window=(struct XcbWindow*)malloc(sizeof(struct XcbWindow));
            *xcb_window=(struct XcbWindow){
                .id=window_id,

                .close_msg_data=wm_delete_window,
                .xi_opcode=xi_opcode,

                .width=info->width,
                .height=info->height,
            };

            info->system->xcb.num_open_windows++;
            info->system->xcb.windows=realloc(
                info->system->xcb.windows,
                info->system->xcb.num_open_windows*sizeof(struct XcbWindow)
            );
            info->system->xcb.windows[info->system->xcb.num_open_windows-1]=xcb_window;

            *window=(struct Window){
                .system=info->system,
                .xcb=xcb_window,
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
            xcb_destroy_window(window->system->xcb.con,window->xcb->id);
            free(window->system->xcb.windows[0]);

            if(window->system->xcb.num_open_windows==1){
                // nothing to do. we dont need to free the memory.
            }else{
                // TODO
            }

            window->system->xcb.num_open_windows--;

            xcb_flush(window->system->xcb.con);

            break;

        default:CHECK(false,"unimplemented");
    }
}
