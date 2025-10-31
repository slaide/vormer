#include <stdlib.h>
#include<stdio.h>

#include<util.h>
#include<system.h>
#include <xcb/xproto.h>

int main(int argc,char**argv){
    discard argc;
    discard argv;

    struct System system;
    struct SystemCreateInfo system_create_info={};
    System_create(&system_create_info,&system);

    struct Window window;
    struct WindowCreateInfo window_create_info={
        .system=&system,

        .width=1280,
        .height=720,

        .title="window title",
    };
    Window_create(&window_create_info,&window);

    int running = 1;
    while(running){
        xcb_flush(system.xcb.con);

        xcb_generic_event_t*xcb_event;
        while((xcb_event=xcb_poll_for_event(system.xcb.con))){
            uint8_t event_type = xcb_event->response_type & ~0x80;
            switch(event_type){
                case XCB_KEY_PRESS:
                    {
                        xcb_key_press_event_t*event=(xcb_key_press_event_t*)xcb_event;
                        printf("key press %d\n",event->detail);
                    }
                    break;
                case XCB_KEY_RELEASE:
                    {
                        xcb_key_release_event_t*event=(xcb_key_release_event_t*)xcb_event;
                        printf("key release %d\n",event->detail);
                    }
                    break;

                case XCB_BUTTON_PRESS:
                    {
                        xcb_button_press_event_t*event=(xcb_button_press_event_t*)xcb_event;
                        printf("button press %d\n",event->detail);
                    }
                    break;
                case XCB_BUTTON_RELEASE:
                    {
                        xcb_button_release_event_t*event=(xcb_button_release_event_t*)xcb_event;
                        printf("button release %d\n",event->detail);
                    }
                    break;

                case XCB_MOTION_NOTIFY:
                    {
                        auto event=(xcb_motion_notify_event_t*)xcb_event;
                        printf("motion at x %d y %d\n",event->event_x,event->event_y);
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
                        printf("window gained focus\n");
                    }
                    break;
                case XCB_FOCUS_OUT:
                    {
                        printf("window lost focus\n");
                    }
                    break;

                    printf("got known event\n");
                    break;
            
                case XCB_CONFIGURE_NOTIFY:
                    {
                        auto event=(xcb_configure_notify_event_t*)xcb_event;
                        // window might have been resized, moved.. or maybe something else.
                        // we only care about resize though
                        if(window.height!=event->height || window.width!=event->width){
                            printf("window resized to %dx%d\n",event->width,event->height);
                            window.height=event->height;
                            window.width=event->width;
                        }
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
                    break;

                case XCB_CLIENT_MESSAGE:
                    {
                        xcb_client_message_event_t*event=(xcb_client_message_event_t*)xcb_event;
                        if(event->format==32 && event->data.data32[0]==window.xcb.close_msg_data){
                            printf("window close requested\n");
                            running = 0;
                        }
                    }
                    break;

                default:
                    printf("event %d\n", event_type);
            }
            free(xcb_event);
        }
    }

    Window_destroy(&window);

    System_destroy(&system);

    return EXIT_SUCCESS;
}
