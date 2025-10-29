#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <stdlib.h>

#include <vulkan/vulkan.h>

#include <platform/platform.h>
#include <graphics/graphics.h>

/* Sleep for the specified duration in seconds */
static void sleep_seconds(float seconds) {
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
}

/* Recreate swapchain-dependent resources (image views and framebuffers) */
static GraphicsError recreate_swapchain_resources(
    GraphicsSystem* graphics,
    GraphicsSwapchain* swapchain,
    VkRenderPass renderpass,
    VkImageView** out_image_views,
    uint32_t* out_image_view_count,
    VkFramebuffer** out_framebuffers,
    uint32_t* out_framebuffer_count) {

    VkDevice device = graphics_system_get_vk_device(graphics);

    /* Destroy old resources if they exist */
    if (*out_image_views) {
        for (uint32_t i = 0; i < *out_image_view_count; i++) {
            vkDestroyImageView(device, (*out_image_views)[i], NULL);
        }
        free(*out_image_views);
        *out_image_views = NULL;
        *out_image_view_count = 0;
    }

    if (*out_framebuffers) {
        for (uint32_t i = 0; i < *out_framebuffer_count; i++) {
            vkDestroyFramebuffer(device, (*out_framebuffers)[i], NULL);
        }
        free(*out_framebuffers);
        *out_framebuffers = NULL;
        *out_framebuffer_count = 0;
    }

    /* Create new image views */
    GraphicsError gfx_err = graphics_create_swapchain_image_views(graphics, swapchain, out_image_views, out_image_view_count);
    if (gfx_err != GRAPHICS_OK) {
        fprintf(stderr, "Failed to create swapchain image views\n");
        return gfx_err;
    }

    /* Create new framebuffers */
    gfx_err = graphics_create_swapchain_framebuffers(graphics, swapchain, renderpass, *out_image_views, out_framebuffers, out_framebuffer_count);
    if (gfx_err != GRAPHICS_OK) {
        fprintf(stderr, "Failed to create swapchain framebuffers\n");
        for (uint32_t i = 0; i < *out_image_view_count; i++) {
            vkDestroyImageView(device, (*out_image_views)[i], NULL);
        }
        free(*out_image_views);
        *out_image_views = NULL;
        *out_image_view_count = 0;
        return gfx_err;
    }

    return GRAPHICS_OK;
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

    /* Initialize graphics system */
    GraphicsQueueRequest queue_requests[1] = {
        {
            .required_capabilities = GRAPHICS_QUEUE_GRAPHICS | GRAPHICS_QUEUE_TRANSFER,
            .queue = VK_NULL_HANDLE,
        }
    };

    GraphicsSystemCreateInfo system_create_info = {
        .enable_validation = true,
        .app_name = "Vormer Engine",
        .queue_requests = queue_requests,
        .queue_request_count = 1,
        .platform = platform,
    };

    GraphicsSystem* graphics = NULL;
    GraphicsError gfx_err = graphics_system_create(&system_create_info, &graphics);
    if (gfx_err != GRAPHICS_OK) {
        fprintf(stderr, "Failed to create graphics system\n");
        platform_destroy_window(window);
        platform_shutdown(platform);
        return 1;
    }

    /* Create surface for window */
    GraphicsSurfaceCreateInfo surface_info = {
        .window = window,
    };

    GraphicsSurface* surface = NULL;
    gfx_err = graphics_surface_create(graphics, &surface_info, &surface);
    if (gfx_err != GRAPHICS_OK) {
        fprintf(stderr, "Failed to create graphics surface\n");
        graphics_system_destroy(graphics);
        platform_destroy_window(window);
        platform_shutdown(platform);
        return 1;
    }

    /* Create swapchain */
    GraphicsSwapchainCreateInfo swapchain_info = {
        .surface = surface,
        .desired_width = 1280,
        .desired_height = 720,
        .old_swapchain = NULL,
    };

    GraphicsSwapchain* swapchain = NULL;
    gfx_err = graphics_swapchain_create(graphics, &swapchain_info, &swapchain);
    if (gfx_err != GRAPHICS_OK) {
        fprintf(stderr, "Failed to create swapchain\n");
        graphics_surface_destroy(graphics, surface);
        graphics_system_destroy(graphics);
        platform_destroy_window(window);
        platform_shutdown(platform);
        return 1;
    }

    /* Create command buffer pool with buffers matching swapchain images */
    uint32_t swapchain_image_count = graphics_swapchain_get_image_count(swapchain);
    GraphicsCommandPoolCreateInfo cmd_pool_info = {
        .buffer_count = swapchain_image_count,
        .queue_family_index = 0,  /* TODO: Get actual queue family from graphics system */
    };

    GraphicsCommandPool* cmd_pool = NULL;
    gfx_err = graphics_command_pool_create(graphics, &cmd_pool_info, &cmd_pool);
    if (gfx_err != GRAPHICS_OK) {
        fprintf(stderr, "Failed to create command buffer pool\n");
        graphics_swapchain_destroy(graphics, swapchain);
        graphics_surface_destroy(graphics, surface);
        graphics_system_destroy(graphics);
        platform_destroy_window(window);
        platform_shutdown(platform);
        return 1;
    }

    /* Create renderpass for rendering to swapchain images */
    VkFormat swapchain_format = graphics_swapchain_get_image_format(swapchain);
    VkRenderPass renderpass = VK_NULL_HANDLE;
    gfx_err = graphics_create_simple_renderpass(graphics, swapchain_format, &renderpass);
    if (gfx_err != GRAPHICS_OK) {
        fprintf(stderr, "Failed to create renderpass\n");
        graphics_command_pool_destroy(graphics, cmd_pool);
        graphics_swapchain_destroy(graphics, swapchain);
        graphics_surface_destroy(graphics, surface);
        graphics_system_destroy(graphics);
        platform_destroy_window(window);
        platform_shutdown(platform);
        return 1;
    }

    /* Create image views and framebuffers for swapchain images */
    VkImageView* image_views = NULL;
    uint32_t image_view_count = 0;
    VkFramebuffer* framebuffers = NULL;
    uint32_t framebuffer_count = 0;
    gfx_err = recreate_swapchain_resources(graphics, swapchain, renderpass, &image_views, &image_view_count, &framebuffers, &framebuffer_count);
    if (gfx_err != GRAPHICS_OK) {
        fprintf(stderr, "Failed to create swapchain resources\n");
        graphics_command_pool_destroy(graphics, cmd_pool);
        graphics_swapchain_destroy(graphics, swapchain);
        graphics_surface_destroy(graphics, surface);
        graphics_system_destroy(graphics);
        platform_destroy_window(window);
        platform_shutdown(platform);
        return 1;
    }

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

        /* Poll events and handle resize */
        bool window_resized = false;
        Event event;
        while (platform_poll_event(platform, &event)) {
            if (event.type == EVENT_WINDOW_RESIZE) {
                window_resized = true;
            }
        }

        /* Handle window resize by recreating swapchain */
        if (window_resized) {
            graphics_system_wait_idle(graphics);

            /* Get the current framebuffer size (in pixels, accounts for high-DPI scaling) */
            int fb_width, fb_height;
            platform_get_framebuffer_size(window, &fb_width, &fb_height);

            /* Recreate swapchain with actual framebuffer dimensions */
            GraphicsSwapchainCreateInfo swapchain_info = {
                .surface = surface,
                .desired_width = (uint32_t)(fb_width > 0 ? fb_width : 1280),
                .desired_height = (uint32_t)(fb_height > 0 ? fb_height : 720),
                .old_swapchain = swapchain,
            };
            GraphicsSwapchain* new_swapchain = nullptr;
            gfx_err = graphics_swapchain_create(graphics, &swapchain_info, &new_swapchain);
            if (gfx_err != GRAPHICS_OK) {
                fprintf(stderr, "Failed to recreate swapchain on resize\n");
                break;
            }
            swapchain = new_swapchain;

            /* Recreate image views and framebuffers for the new swapchain */
            gfx_err = recreate_swapchain_resources(graphics, swapchain, renderpass, &image_views, &image_view_count, &framebuffers, &framebuffer_count);
            if (gfx_err != GRAPHICS_OK) {
                fprintf(stderr, "Failed to recreate swapchain resources on resize\n");
                break;
            }

            /* Skip this frame after resize to avoid stale semaphore issues */
            continue;
        }

        /* Get command buffer for this frame (waits for GPU to finish with it) */
        VkCommandBuffer command_buffer;
        gfx_err = graphics_command_pool_get_command_buffer(graphics, cmd_pool, frame_count, &command_buffer);
        if (gfx_err != GRAPHICS_OK) {
            fprintf(stderr, "Failed to get command buffer\n");
            break;
        }

        /* Get semaphore for image acquisition (from command pool) */
        VkSemaphore image_acquired_semaphore = graphics_command_pool_get_image_acquired_semaphore(cmd_pool, frame_count);

        /* Acquire next image from swapchain */
        uint32_t image_index = 0;
        gfx_err = graphics_swapchain_acquire_image(graphics, swapchain, &image_index, image_acquired_semaphore);
        if (gfx_err != GRAPHICS_OK) {
            fprintf(stderr, "Failed to acquire swapchain image\n");
            break;
        }

        /* Begin command buffer recording */
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        VkResult cmd_result = vkBeginCommandBuffer(command_buffer, &begin_info);
        if (cmd_result != VK_SUCCESS) {
            fprintf(stderr, "Failed to begin command buffer\n");
            break;
        }

        /* Begin render pass with orange clear color */
        uint32_t swapchain_width, swapchain_height;
        graphics_swapchain_get_dimensions(swapchain, &swapchain_width, &swapchain_height);

        VkClearValue clear_value = {
            .color = {.float32 = {1.0f, 0.5f, 0.0f, 1.0f}},
        };
        VkRenderPassBeginInfo renderpass_info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = renderpass,
            .framebuffer = framebuffers[image_index],
            .renderArea = {
                .offset = {0, 0},
                .extent = {
                    .width = swapchain_width,
                    .height = swapchain_height,
                },
            },
            .clearValueCount = 1,
            .pClearValues = &clear_value,
        };

        vkCmdBeginRenderPass(command_buffer, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);

        /* Clear the color attachment explicitly */
        VkClearAttachment clear_attachment = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .colorAttachment = 0,
            .clearValue = clear_value,
        };
        VkClearRect clear_rect = {
            .rect = {
                .offset = {0, 0},
                .extent = {swapchain_width, swapchain_height},
            },
            .baseArrayLayer = 0,
            .layerCount = 1,
        };
        vkCmdClearAttachments(command_buffer, 1, &clear_attachment, 1, &clear_rect);

        /* End render pass */
        vkCmdEndRenderPass(command_buffer);

        /* End command buffer recording */
        cmd_result = vkEndCommandBuffer(command_buffer);
        if (cmd_result != VK_SUCCESS) {
            fprintf(stderr, "Failed to end command buffer\n");
            break;
        }

        /* Submit command buffer with synchronization */
        gfx_err = graphics_command_pool_submit(graphics, cmd_pool, frame_count,
                                                image_acquired_semaphore, command_buffer);
        if (gfx_err != GRAPHICS_OK) {
            fprintf(stderr, "Failed to submit command buffer\n");
            break;
        }

        /* Present the rendered image to the display */
        VkSemaphore render_complete_semaphore = graphics_command_pool_get_render_complete_semaphore(cmd_pool, frame_count);
        gfx_err = graphics_swapchain_present(graphics, swapchain, image_index, render_complete_semaphore);
        if (gfx_err != GRAPHICS_OK) {
            fprintf(stderr, "Failed to present swapchain image\n");
            break;
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

    /* Wait for GPU to finish all pending operations before cleanup */
    graphics_system_wait_idle(graphics);

    /* Cleanup graphics resources */
    VkDevice device = graphics_system_get_vk_device(graphics);

    if (framebuffers) {
        for (uint32_t i = 0; i < framebuffer_count; i++) {
            vkDestroyFramebuffer(device, framebuffers[i], NULL);
        }
        free(framebuffers);
    }
    if (image_views) {
        for (uint32_t i = 0; i < image_view_count; i++) {
            vkDestroyImageView(device, image_views[i], NULL);
        }
        free(image_views);
    }
    vkDestroyRenderPass(device, renderpass, NULL);
    graphics_command_pool_destroy(graphics, cmd_pool);
    graphics_swapchain_destroy(graphics, swapchain);
    graphics_surface_destroy(graphics, surface);
    graphics_system_destroy(graphics);

    /* Cleanup platform resources */
    platform_destroy_window(window);
    platform_shutdown(platform);

    return 0;
}
