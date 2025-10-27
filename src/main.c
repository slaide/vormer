#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "platform/platform.h"

/* Sleep for the specified duration in seconds */
static void sleep_seconds(float seconds) {
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    /* Initialize platform */
    Platform* platform = NULL;
    PlatformError err = platform_init(&platform);
    if (err != PLATFORM_OK) {
        fprintf(stderr, "Failed to initialize platform: %d\n", err);
        return 1;
    }

    /* Create window */
    WindowConfig window_config = {
        .title = "Vormer Engine",
        .width = 1280,
        .height = 720,
        .fullscreen = WINDOW_DONTCARE,
        .resizable = WINDOW_TRUE,
        .decorated = WINDOW_DONTCARE,
        .vsync = WINDOW_DONTCARE,
    };

    PlatformWindow* window = NULL;
    err = platform_create_window(platform, &window_config, &window);
    if (err != PLATFORM_OK) {
        fprintf(stderr, "Failed to create window: %d\n", err);
        platform_shutdown(platform);
        return 1;
    }

    platform_show_window(window);

    /* Run event loop for 30 seconds at 30 FPS */
    const double target_fps = 30.0;
    const double frame_time = 1.0 / target_fps;
    const double run_duration = 10.0;  /* Run for 10 seconds */

    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    double elapsed = 0.0;
    int frame_count = 0;

    while (elapsed < run_duration && !platform_window_close_requested(window)) {
        struct timespec frame_start;
        clock_gettime(CLOCK_MONOTONIC, &frame_start);

        /* Poll events (but don't need to check for close here anymore) */
        Event event;
        while (platform_poll_event(platform, &event)) {
            /* Just process events, close state is checked in loop condition */
        }

        /* Update elapsed time */
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        elapsed = (double)(current_time.tv_sec - start_time.tv_sec) +
                  (double)(current_time.tv_nsec - start_time.tv_nsec) / 1e9;

        frame_count++;

        /* Sleep to maintain 30 FPS */
        struct timespec frame_end;
        clock_gettime(CLOCK_MONOTONIC, &frame_end);
        double frame_elapsed = (double)(frame_end.tv_sec - frame_start.tv_sec) +
                               (double)(frame_end.tv_nsec - frame_start.tv_nsec) / 1e9;

        if (frame_elapsed < frame_time) {
            sleep_seconds((float)(frame_time - frame_elapsed));
        }
    }

    if (platform_window_close_requested(window)) {
        printf("Window closed by user, rendered %d frames\n", frame_count);
    } else {
        printf("Engine ran for 10 seconds, rendered %d frames\n", frame_count);
    }

    /* Cleanup */
    platform_destroy_window(window);
    platform_shutdown(platform);

    return 0;
}
