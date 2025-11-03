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
        struct Event event;
        while((System_pollEvent(&system,&event),event.kind!=EVENT_KIND_NONE)){
            switch(event.kind){
                case EVENT_KIND_KEY_PRESS:
                    {
                        printf("key press %d\n",event.key_press.key);
                    }
                    break;
                case EVENT_KIND_KEY_RELEASE:
                    {
                        printf("key release %d\n",event.key_release.key);
                    }
                    break;

                case EVENT_KIND_BUTTON_PRESS:
                    {
                        printf("button press %d\n",event.button_press.button);
                    }
                    break;
                case EVENT_KIND_BUTTON_RELEASE:
                    {
                        printf("button release %d\n",event.button_release.button);
                    }
                    break;

                case EVENT_KIND_POINTER_MOVE:
                    {
                        printf("pointer moved to %f x %f y\n",event.pointer_move.x,event.pointer_move.y);
                    }
                    break;

                case EVENT_KIND_FOCUS_GAINED:
                    {
                        printf("window gained focus\n");
                    }
                    break;
                case EVENT_KIND_FOCUS_LOST:
                    {
                        printf("window lost focus\n");
                    }
                    break;
            
                case EVENT_KIND_WINDOW_RESIZED:
                    {
                        printf(
                            "window got resized from %dx%d to %dx%d\n",
                            event.window_resize.old_width,
                            event.window_resize.old_height,
                            event.window_resize.new_width,
                            event.window_resize.new_height
                        );
                    }
                    break;

                case EVENT_KIND_WINDOW_CLOSED:
                    {
                        printf("window closed\n");
                        running = 0;
                    }
                    break;

                case EVENT_KIND_IGNORED:
                    break;

                default:
                    printf("event %d\n", event.kind);
            }
        }
    }

    Window_destroy(&window);

    System_destroy(&system);

    return EXIT_SUCCESS;
}
