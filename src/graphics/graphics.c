#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_X11
#define VK_USE_PLATFORM_XCB_KHR
#include <xcb/xcb.h>
#endif
#include <vulkan/vulkan.h>

#include "graphics/graphics.h"
#include "platform/platform.h"

/* ============================================================================
 * Graphics System Implementation
 * ============================================================================ */

struct GraphicsSystem {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;

    /* Store queue and family info for later access */
    uint32_t graphics_queue_family;
    VkQueue graphics_queue;
};

GraphicsError graphics_system_create(const GraphicsSystemCreateInfo* create_info,
                                      GraphicsSystem** out_system) {
    if (!create_info || !out_system) {
        return GRAPHICS_ERROR_INVALID_ARG;
    }

    /* Allocate system structure */
    GraphicsSystem* system = (GraphicsSystem*)calloc(1, sizeof(GraphicsSystem));
    if (!system) {
        return GRAPHICS_ERROR_OUT_OF_MEMORY;
    }

    /* ========== Create VkInstance ========== */
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = create_info->app_name ? create_info->app_name : "App",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Vormer",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    /* Get platform-required extensions */
    uint32_t extension_count = 0;
    const char** extensions = nullptr;
    /* TODO: Call platform_get_vulkan_extensions(create_info->platform, &extensions, &extension_count) */
    /* For now, allocate a small array for common extensions */
    const char* default_extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef PLATFORM_LINUX
#ifdef USE_X11
        VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#endif
#ifdef USE_WAYLAND
        VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
#endif
#endif
    };
    extensions = default_extensions;
    extension_count = sizeof(default_extensions) / sizeof(default_extensions[0]);

    /* Build layer array if validation is enabled */
    const char* layers[] = {NULL};
    uint32_t layer_count = 0;
    if (create_info->enable_validation) {
        layers[0] = "VK_LAYER_KHRONOS_validation";
        layer_count = 1;
    }

    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extensions,
        .enabledLayerCount = layer_count,
        .ppEnabledLayerNames = layer_count > 0 ? layers : NULL,
    };

    VkResult result = vkCreateInstance(&instance_info, NULL, &system->instance);
    if (result != VK_SUCCESS) {
        free(system);
        return GRAPHICS_ERROR_GENERIC;
    }

    /* ========== Enumerate and select physical device ========== */
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(system->instance, &device_count, NULL);
    if (device_count == 0) {
        vkDestroyInstance(system->instance, NULL);
        free(system);
        return GRAPHICS_ERROR_GENERIC;
    }

    VkPhysicalDevice* devices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * device_count);
    if (!devices) {
        vkDestroyInstance(system->instance, NULL);
        free(system);
        return GRAPHICS_ERROR_OUT_OF_MEMORY;
    }

    vkEnumeratePhysicalDevices(system->instance, &device_count, devices);

    /* Select first device (in production, you'd query capabilities) */
    system->physical_device = devices[0];
    free(devices);

    /* ========== Find queue families ========== */
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(system->physical_device, &queue_family_count, NULL);

    VkQueueFamilyProperties* queue_families =
        (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
    if (!queue_families) {
        vkDestroyInstance(system->instance, NULL);
        free(system);
        return GRAPHICS_ERROR_OUT_OF_MEMORY;
    }

    vkGetPhysicalDeviceQueueFamilyProperties(system->physical_device, &queue_family_count, queue_families);

    /* Find a queue family that supports graphics */
    system->graphics_queue_family = 0;
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            system->graphics_queue_family = i;
            break;
        }
    }

    free(queue_families);

    /* ========== Create logical device ========== */
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = system->graphics_queue_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };

    const char* device_extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkPhysicalDeviceFeatures device_features = {0};

    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
        .enabledExtensionCount = sizeof(device_extensions) / sizeof(device_extensions[0]),
        .ppEnabledExtensionNames = device_extensions,
        .pEnabledFeatures = &device_features,
        .enabledLayerCount = 0,  /* Deprecated in newer Vulkan */
    };

    result = vkCreateDevice(system->physical_device, &device_info, NULL, &system->device);
    if (result != VK_SUCCESS) {
        vkDestroyInstance(system->instance, NULL);
        free(system);
        return GRAPHICS_ERROR_GENERIC;
    }

    /* Get the graphics queue */
    vkGetDeviceQueue(system->device, system->graphics_queue_family, 0, &system->graphics_queue);

    /* Populate queue requests with queue handles */
    for (uint32_t i = 0; i < create_info->queue_request_count; i++) {
        if (create_info->queue_requests[i].required_capabilities & GRAPHICS_QUEUE_GRAPHICS) {
            create_info->queue_requests[i].queue = system->graphics_queue;
        }
        /* TODO: Handle other queue types (compute, transfer) */
    }

    *out_system = system;
    return GRAPHICS_OK;
}

void graphics_system_destroy(GraphicsSystem* system) {
    if (!system) return;

    if (system->device != VK_NULL_HANDLE) {
        vkDestroyDevice(system->device, NULL);
    }
    if (system->instance != VK_NULL_HANDLE) {
        vkDestroyInstance(system->instance, NULL);
    }

    free(system);
}

GraphicsError graphics_system_wait_idle(GraphicsSystem* system) {
    if (!system || system->device == VK_NULL_HANDLE) {
        return GRAPHICS_ERROR_INVALID_ARG;
    }

    VkResult result = vkDeviceWaitIdle(system->device);
    if (result != VK_SUCCESS) {
        return GRAPHICS_ERROR_GENERIC;
    }

    return GRAPHICS_OK;
}

VkInstance graphics_system_get_vk_instance(GraphicsSystem* system) {
    return system ? system->instance : VK_NULL_HANDLE;
}

VkPhysicalDevice graphics_system_get_vk_physical_device(GraphicsSystem* system) {
    return system ? system->physical_device : VK_NULL_HANDLE;
}

VkDevice graphics_system_get_vk_device(GraphicsSystem* system) {
    return system ? system->device : VK_NULL_HANDLE;
}

/* ============================================================================
 * Surface Implementation
 * ============================================================================ */

struct GraphicsSurface {
    VkSurfaceKHR surface;
};

GraphicsError graphics_surface_create(GraphicsSystem* system,
                                       const GraphicsSurfaceCreateInfo* create_info,
                                       GraphicsSurface** out_surface) {
    if (!system || !create_info || !out_surface || !create_info->window) {
        return GRAPHICS_ERROR_INVALID_ARG;
    }

    if (system->instance == VK_NULL_HANDLE) {
        return GRAPHICS_ERROR_GENERIC;
    }

    GraphicsSurface* surface = (GraphicsSurface*)calloc(1, sizeof(GraphicsSurface));
    if (!surface) {
        return GRAPHICS_ERROR_OUT_OF_MEMORY;
    }

    /* Create VkSurfaceKHR from platform window */
    PlatformError err = platform_create_vulkan_surface(create_info->window,
                                                        system->instance,
                                                        &surface->surface);
    if (err != PLATFORM_OK || surface->surface == VK_NULL_HANDLE) {
        free(surface);
        return GRAPHICS_ERROR_GENERIC;
    }

    *out_surface = surface;
    return GRAPHICS_OK;
}

void graphics_surface_destroy(GraphicsSystem* system, GraphicsSurface* surface) {
    if (!surface) return;

    if (system && system->instance != VK_NULL_HANDLE && surface->surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(system->instance, surface->surface, NULL);
    }

    free(surface);
}

VkSurfaceKHR graphics_surface_get_vk_surface(GraphicsSurface* surface) {
    return surface ? surface->surface : VK_NULL_HANDLE;
}

/* ============================================================================
 * Swapchain Implementation
 * ============================================================================ */

struct GraphicsSwapchain {
    VkSwapchainKHR swapchain;
    VkImage* images;
    uint32_t image_count;
    VkFormat image_format;
    uint32_t width;
    uint32_t height;
};

GraphicsError graphics_swapchain_create(GraphicsSystem* system,
                                         const GraphicsSwapchainCreateInfo* create_info,
                                         GraphicsSwapchain** out_swapchain) {
    if (!system || !create_info || !out_swapchain || !create_info->surface) {
        return GRAPHICS_ERROR_INVALID_ARG;
    }

    if (system->device == VK_NULL_HANDLE || system->physical_device == VK_NULL_HANDLE) {
        return GRAPHICS_ERROR_GENERIC;
    }

    GraphicsSwapchain* swapchain = (GraphicsSwapchain*)calloc(1, sizeof(GraphicsSwapchain));
    if (!swapchain) {
        return GRAPHICS_ERROR_OUT_OF_MEMORY;
    }

    VkSurfaceKHR surface = graphics_surface_get_vk_surface(create_info->surface);
    if (surface == VK_NULL_HANDLE) {
        free(swapchain);
        return GRAPHICS_ERROR_GENERIC;
    }

    /* Query surface capabilities */
    VkSurfaceCapabilitiesKHR surface_caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(system->physical_device, surface, &surface_caps);

    /* Query surface formats */
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(system->physical_device, surface, &format_count, NULL);
    if (format_count == 0) {
        free(swapchain);
        return GRAPHICS_ERROR_GENERIC;
    }

    VkSurfaceFormatKHR* surface_formats =
        (VkSurfaceFormatKHR*)malloc(sizeof(VkSurfaceFormatKHR) * format_count);
    if (!surface_formats) {
        free(swapchain);
        return GRAPHICS_ERROR_OUT_OF_MEMORY;
    }

    vkGetPhysicalDeviceSurfaceFormatsKHR(system->physical_device, surface, &format_count, surface_formats);

    /* Use first available format (sRGB if available) */
    VkFormat chosen_format = surface_formats[0].format;
    swapchain->image_format = chosen_format;
    free(surface_formats);

    /* Query present modes */
    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(system->physical_device, surface, &present_mode_count, NULL);
    if (present_mode_count == 0) {
        free(swapchain);
        return GRAPHICS_ERROR_GENERIC;
    }

    VkPresentModeKHR* present_modes = (VkPresentModeKHR*)malloc(sizeof(VkPresentModeKHR) * present_mode_count);
    if (!present_modes) {
        free(swapchain);
        return GRAPHICS_ERROR_OUT_OF_MEMORY;
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR(system->physical_device, surface, &present_mode_count, present_modes);

    /* Use FIFO (vsync) mode */
    VkPresentModeKHR chosen_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    free(present_modes);

    /* Clamp dimensions to surface capabilities */
    uint32_t width = create_info->desired_width;
    uint32_t height = create_info->desired_height;
    if (surface_caps.currentExtent.width != UINT32_MAX) {
        width = surface_caps.currentExtent.width;
        height = surface_caps.currentExtent.height;
    } else {
        if (width > surface_caps.maxImageExtent.width) {
            width = surface_caps.maxImageExtent.width;
        }
        if (width < surface_caps.minImageExtent.width) {
            width = surface_caps.minImageExtent.width;
        }
        if (height > surface_caps.maxImageExtent.height) {
            height = surface_caps.maxImageExtent.height;
        }
        if (height < surface_caps.minImageExtent.height) {
            height = surface_caps.minImageExtent.height;
        }
    }

    swapchain->width = width;
    swapchain->height = height;

    /* Determine image count */
    uint32_t image_count = surface_caps.minImageCount + 1;
    if (surface_caps.maxImageCount > 0 && image_count > surface_caps.maxImageCount) {
        image_count = surface_caps.maxImageCount;
    }

    /* Create swapchain */
    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = image_count,
        .imageFormat = chosen_format,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = {.width = width, .height = height},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = NULL,
        .preTransform = surface_caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = chosen_present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = create_info->old_swapchain ?
            ((GraphicsSwapchain*)create_info->old_swapchain)->swapchain : VK_NULL_HANDLE,
    };

    VkResult result = vkCreateSwapchainKHR(system->device, &swapchain_info, NULL, &swapchain->swapchain);
    if (result != VK_SUCCESS) {
        free(swapchain);
        return GRAPHICS_ERROR_GENERIC;
    }

    /* Destroy the old swapchain if one was provided (Vulkan spec requires explicit destruction) */
    if (create_info->old_swapchain) {
        GraphicsSwapchain* old_swapchain = (GraphicsSwapchain*)create_info->old_swapchain;
        if (old_swapchain->swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(system->device, old_swapchain->swapchain, NULL);
            old_swapchain->swapchain = VK_NULL_HANDLE;
        }
    }

    /* Retrieve swapchain images */
    vkGetSwapchainImagesKHR(system->device, swapchain->swapchain, &image_count, NULL);

    swapchain->images = (VkImage*)malloc(sizeof(VkImage) * image_count);
    if (!swapchain->images) {
        vkDestroySwapchainKHR(system->device, swapchain->swapchain, NULL);
        free(swapchain);
        return GRAPHICS_ERROR_OUT_OF_MEMORY;
    }

    vkGetSwapchainImagesKHR(system->device, swapchain->swapchain, &image_count, swapchain->images);
    swapchain->image_count = image_count;

    *out_swapchain = swapchain;
    return GRAPHICS_OK;
}

void graphics_swapchain_destroy(GraphicsSystem* system, GraphicsSwapchain* swapchain) {
    if (!swapchain) return;

    if (system && system->device != VK_NULL_HANDLE && swapchain->swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(system->device, swapchain->swapchain, NULL);
    }

    if (swapchain->images) {
        free(swapchain->images);
    }
    free(swapchain);
}

void graphics_swapchain_get_dimensions(GraphicsSwapchain* swapchain,
                                        uint32_t* out_width,
                                        uint32_t* out_height) {
    if (!swapchain || !out_width || !out_height) return;

    *out_width = swapchain->width;
    *out_height = swapchain->height;
}

uint32_t graphics_swapchain_get_image_count(GraphicsSwapchain* swapchain) {
    return swapchain ? swapchain->image_count : 0;
}

VkImage graphics_swapchain_get_image(GraphicsSwapchain* swapchain, uint32_t index) {
    if (!swapchain || index >= swapchain->image_count) {
        return VK_NULL_HANDLE;
    }
    return swapchain->images[index];
}

VkFormat graphics_swapchain_get_image_format(GraphicsSwapchain* swapchain) {
    return swapchain ? swapchain->image_format : VK_FORMAT_UNDEFINED;
}

GraphicsError graphics_swapchain_acquire_image(GraphicsSystem* system,
                                                GraphicsSwapchain* swapchain,
                                                uint32_t* out_image_index,
                                                VkSemaphore image_acquired_semaphore) {
    if (!system || !swapchain || !out_image_index) {
        return GRAPHICS_ERROR_INVALID_ARG;
    }

    if (system->device == VK_NULL_HANDLE || swapchain->swapchain == VK_NULL_HANDLE) {
        return GRAPHICS_ERROR_GENERIC;
    }

    uint32_t image_index = 0;
    /* Use the provided semaphore for synchronization (caller provides from command pool) */
    VkResult result = vkAcquireNextImageKHR(system->device, swapchain->swapchain,
                                             UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return GRAPHICS_ERROR_OUT_OF_DATE;
    }
    if (result != VK_SUCCESS) {
        fprintf(stderr, "vkAcquireNextImageKHR failed with result: %d\n", result);
        return GRAPHICS_ERROR_GENERIC;
    }

    *out_image_index = image_index;
    return GRAPHICS_OK;
}

GraphicsError graphics_swapchain_present(GraphicsSystem* system,
                                          GraphicsSwapchain* swapchain,
                                          uint32_t image_index,
                                          VkSemaphore render_complete_semaphore) {
    if (!system || !swapchain) {
        return GRAPHICS_ERROR_INVALID_ARG;
    }

    if (system->graphics_queue == VK_NULL_HANDLE || swapchain->swapchain == VK_NULL_HANDLE) {
        return GRAPHICS_ERROR_GENERIC;
    }

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = (render_complete_semaphore != VK_NULL_HANDLE) ? 1 : 0,
        .pWaitSemaphores = (render_complete_semaphore != VK_NULL_HANDLE) ? &render_complete_semaphore : NULL,
        .swapchainCount = 1,
        .pSwapchains = &swapchain->swapchain,
        .pImageIndices = &image_index,
    };

    VkResult result = vkQueuePresentKHR(system->graphics_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return GRAPHICS_ERROR_OUT_OF_DATE;
    }
    if (result != VK_SUCCESS) {
        fprintf(stderr, "vkQueuePresentKHR failed with result: %d\n", result);
        return GRAPHICS_ERROR_GENERIC;
    }

    return GRAPHICS_OK;
}

/* ============================================================================
 * Command Buffer Pool Implementation
 * ============================================================================ */

struct GraphicsCommandPool {
    VkCommandPool vk_pool;
    VkCommandBuffer* command_buffers;
    VkFence* fences;
    VkSemaphore* render_complete_semaphores;
    VkSemaphore* image_acquired_semaphores;  /* For swapchain image acquisition */
    uint32_t buffer_count;
    VkQueue submit_queue;
};

GraphicsError graphics_command_pool_create(GraphicsSystem* system,
                                            const GraphicsCommandPoolCreateInfo* create_info,
                                            GraphicsCommandPool** out_pool) {
    if (!system || !create_info || !out_pool || create_info->buffer_count == 0) {
        return GRAPHICS_ERROR_INVALID_ARG;
    }

    GraphicsCommandPool* pool = (GraphicsCommandPool*)calloc(1, sizeof(GraphicsCommandPool));
    if (!pool) {
        return GRAPHICS_ERROR_OUT_OF_MEMORY;
    }

    pool->buffer_count = create_info->buffer_count;
    pool->submit_queue = system->graphics_queue;

    /* Allocate command buffers array */
    pool->command_buffers = (VkCommandBuffer*)calloc(pool->buffer_count, sizeof(VkCommandBuffer));
    if (!pool->command_buffers) {
        free(pool);
        return GRAPHICS_ERROR_OUT_OF_MEMORY;
    }

    /* Allocate fences array */
    pool->fences = (VkFence*)calloc(pool->buffer_count, sizeof(VkFence));
    if (!pool->fences) {
        free(pool->command_buffers);
        free(pool);
        return GRAPHICS_ERROR_OUT_OF_MEMORY;
    }

    /* Allocate render_complete semaphores array */
    pool->render_complete_semaphores = (VkSemaphore*)calloc(pool->buffer_count, sizeof(VkSemaphore));
    if (!pool->render_complete_semaphores) {
        free(pool->fences);
        free(pool->command_buffers);
        free(pool);
        return GRAPHICS_ERROR_OUT_OF_MEMORY;
    }

    /* Allocate image_acquired semaphores array */
    pool->image_acquired_semaphores = (VkSemaphore*)calloc(pool->buffer_count, sizeof(VkSemaphore));
    if (!pool->image_acquired_semaphores) {
        free(pool->render_complete_semaphores);
        free(pool->fences);
        free(pool->command_buffers);
        free(pool);
        return GRAPHICS_ERROR_OUT_OF_MEMORY;
    }

    /* Create VkCommandPool */
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = create_info->queue_family_index,
    };

    VkResult result = vkCreateCommandPool(system->device, &pool_info, NULL, &pool->vk_pool);
    if (result != VK_SUCCESS) {
        free(pool->render_complete_semaphores);
        free(pool->fences);
        free(pool->command_buffers);
        free(pool);
        return GRAPHICS_ERROR_GENERIC;
    }

    /* Allocate command buffers */
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool->vk_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = pool->buffer_count,
    };

    result = vkAllocateCommandBuffers(system->device, &alloc_info, pool->command_buffers);
    if (result != VK_SUCCESS) {
        vkDestroyCommandPool(system->device, pool->vk_pool, NULL);
        free(pool->render_complete_semaphores);
        free(pool->fences);
        free(pool->command_buffers);
        free(pool);
        return GRAPHICS_ERROR_GENERIC;
    }

    /* Create fences for each buffer (initially signaled) */
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,  /* Start signaled so first frame doesn't wait */
    };

    for (uint32_t i = 0; i < pool->buffer_count; i++) {
        result = vkCreateFence(system->device, &fence_info, NULL, &pool->fences[i]);
        if (result != VK_SUCCESS) {
            /* Cleanup previously created fences */
            for (uint32_t j = 0; j < i; j++) {
                vkDestroyFence(system->device, pool->fences[j], NULL);
            }
            vkFreeCommandBuffers(system->device, pool->vk_pool, pool->buffer_count, pool->command_buffers);
            vkDestroyCommandPool(system->device, pool->vk_pool, NULL);
            free(pool->render_complete_semaphores);
            free(pool->fences);
            free(pool->command_buffers);
            free(pool);
            return GRAPHICS_ERROR_GENERIC;
        }
    }

    /* Create semaphores for each buffer */
    VkSemaphoreCreateInfo sem_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    for (uint32_t i = 0; i < pool->buffer_count; i++) {
        result = vkCreateSemaphore(system->device, &sem_info, NULL, &pool->render_complete_semaphores[i]);
        if (result != VK_SUCCESS) {
            /* Cleanup previously created semaphores */
            for (uint32_t j = 0; j < i; j++) {
                vkDestroySemaphore(system->device, pool->render_complete_semaphores[j], NULL);
            }
            /* Cleanup fences */
            for (uint32_t j = 0; j < pool->buffer_count; j++) {
                vkDestroyFence(system->device, pool->fences[j], NULL);
            }
            vkFreeCommandBuffers(system->device, pool->vk_pool, pool->buffer_count, pool->command_buffers);
            vkDestroyCommandPool(system->device, pool->vk_pool, NULL);
            free(pool->image_acquired_semaphores);
            free(pool->render_complete_semaphores);
            free(pool->fences);
            free(pool->command_buffers);
            free(pool);
            return GRAPHICS_ERROR_GENERIC;
        }

        result = vkCreateSemaphore(system->device, &sem_info, NULL, &pool->image_acquired_semaphores[i]);
        if (result != VK_SUCCESS) {
            /* Cleanup previously created semaphores */
            vkDestroySemaphore(system->device, pool->render_complete_semaphores[i], NULL);
            for (uint32_t j = 0; j < i; j++) {
                vkDestroySemaphore(system->device, pool->image_acquired_semaphores[j], NULL);
                vkDestroySemaphore(system->device, pool->render_complete_semaphores[j], NULL);
            }
            /* Cleanup fences */
            for (uint32_t j = 0; j < pool->buffer_count; j++) {
                vkDestroyFence(system->device, pool->fences[j], NULL);
            }
            vkFreeCommandBuffers(system->device, pool->vk_pool, pool->buffer_count, pool->command_buffers);
            vkDestroyCommandPool(system->device, pool->vk_pool, NULL);
            free(pool->image_acquired_semaphores);
            free(pool->render_complete_semaphores);
            free(pool->fences);
            free(pool->command_buffers);
            free(pool);
            return GRAPHICS_ERROR_GENERIC;
        }
    }

    *out_pool = pool;
    return GRAPHICS_OK;
}

void graphics_command_pool_destroy(GraphicsSystem* system, GraphicsCommandPool* pool) {
    if (!pool) return;

    if (system && system->device != VK_NULL_HANDLE) {
        /* Destroy semaphores */
        if (pool->render_complete_semaphores) {
            for (uint32_t i = 0; i < pool->buffer_count; i++) {
                if (pool->render_complete_semaphores[i] != VK_NULL_HANDLE) {
                    vkDestroySemaphore(system->device, pool->render_complete_semaphores[i], NULL);
                }
            }
        }

        if (pool->image_acquired_semaphores) {
            for (uint32_t i = 0; i < pool->buffer_count; i++) {
                if (pool->image_acquired_semaphores[i] != VK_NULL_HANDLE) {
                    vkDestroySemaphore(system->device, pool->image_acquired_semaphores[i], NULL);
                }
            }
        }

        /* Destroy fences */
        if (pool->fences) {
            for (uint32_t i = 0; i < pool->buffer_count; i++) {
                if (pool->fences[i] != VK_NULL_HANDLE) {
                    vkDestroyFence(system->device, pool->fences[i], NULL);
                }
            }
        }

        /* Free command buffers */
        if (pool->command_buffers && pool->vk_pool != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(system->device, pool->vk_pool, pool->buffer_count, pool->command_buffers);
        }

        /* Destroy command pool */
        if (pool->vk_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(system->device, pool->vk_pool, NULL);
        }
    }

    if (pool->render_complete_semaphores) {
        free(pool->render_complete_semaphores);
    }
    if (pool->image_acquired_semaphores) {
        free(pool->image_acquired_semaphores);
    }
    if (pool->fences) {
        free(pool->fences);
    }
    if (pool->command_buffers) {
        free(pool->command_buffers);
    }
    free(pool);
}

GraphicsError graphics_command_pool_get_command_buffer(GraphicsSystem* system,
                                                        GraphicsCommandPool* pool,
                                                        uint32_t frame_index,
                                                        VkCommandBuffer* out_buffer) {
    if (!system || !pool || !out_buffer) {
        return GRAPHICS_ERROR_INVALID_ARG;
    }

    if (system->device == VK_NULL_HANDLE) {
        return GRAPHICS_ERROR_GENERIC;
    }

    uint32_t buffer_index = frame_index % pool->buffer_count;

    /* Wait for the GPU to finish with this command buffer (fence will be signaled
     * when the submitted command buffer completes on the GPU) */
    VkResult result = vkWaitForFences(system->device, 1, &pool->fences[buffer_index], VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS) {
        return GRAPHICS_ERROR_GENERIC;
    }

    /* Reset fence to unsignaled state for next submission */
    result = vkResetFences(system->device, 1, &pool->fences[buffer_index]);
    if (result != VK_SUCCESS) {
        return GRAPHICS_ERROR_GENERIC;
    }

    /* Reset command buffer for recording new commands */
    result = vkResetCommandBuffer(pool->command_buffers[buffer_index], 0);
    if (result != VK_SUCCESS) {
        return GRAPHICS_ERROR_GENERIC;
    }

    *out_buffer = pool->command_buffers[buffer_index];
    return GRAPHICS_OK;
}

VkSemaphore graphics_command_pool_get_render_complete_semaphore(GraphicsCommandPool* pool,
                                                                 uint32_t frame_index) {
    if (!pool) {
        return VK_NULL_HANDLE;
    }

    uint32_t buffer_index = frame_index % pool->buffer_count;
    return pool->render_complete_semaphores[buffer_index];
}

VkSemaphore graphics_command_pool_get_image_acquired_semaphore(GraphicsCommandPool* pool,
                                                               uint32_t frame_index) {
    if (!pool) {
        return VK_NULL_HANDLE;
    }

    uint32_t buffer_index = frame_index % pool->buffer_count;
    return pool->image_acquired_semaphores[buffer_index];
}

GraphicsError graphics_command_pool_submit(GraphicsSystem* system,
                                            GraphicsCommandPool* pool,
                                            uint32_t frame_index,
                                            VkSemaphore image_acquired_semaphore,
                                            VkCommandBuffer command_buffer) {
    if (!system || !pool) {
        return GRAPHICS_ERROR_INVALID_ARG;
    }

    if (system->device == VK_NULL_HANDLE || pool->submit_queue == VK_NULL_HANDLE) {
        return GRAPHICS_ERROR_GENERIC;
    }

    uint32_t buffer_index = frame_index % pool->buffer_count;

    /* Build submit info with proper synchronization:
     * - Wait for image_acquired_semaphore (image is ready for rendering)
     * - Signal render_complete_semaphore (rendering is done)
     * - Signal fence (GPU finished with this buffer, can reuse next frame) */

    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = (image_acquired_semaphore != VK_NULL_HANDLE) ? 1 : 0,
        .pWaitSemaphores = (image_acquired_semaphore != VK_NULL_HANDLE) ? &image_acquired_semaphore : NULL,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &pool->render_complete_semaphores[buffer_index],
    };

    /* Submit with fence to signal when complete */
    VkResult result = vkQueueSubmit(pool->submit_queue, 1, &submit_info, pool->fences[buffer_index]);
    if (result != VK_SUCCESS) {
        return GRAPHICS_ERROR_GENERIC;
    }

    return GRAPHICS_OK;
}

/* ============================================================================
 * Helper Functions Implementation
 * ============================================================================ */

GraphicsError graphics_create_simple_renderpass(GraphicsSystem* system,
                                                 VkFormat color_format,
                                                 VkRenderPass* out_renderpass) {
    if (!system || !out_renderpass) {
        return GRAPHICS_ERROR_INVALID_ARG;
    }

    VkDevice device = graphics_system_get_vk_device(system);
    if (device == VK_NULL_HANDLE) {
        return GRAPHICS_ERROR_GENERIC;
    }

    /* Create VkAttachmentDescription for color attachment */
    VkAttachmentDescription attachment = {
        .format = color_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    /* Create VkAttachmentReference for the color attachment */
    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    /* Create VkSubpassDescription */
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
    };

    /* Create VkRenderPassCreateInfo */
    VkRenderPassCreateInfo renderpass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
    };

    /* Create the renderpass */
    VkResult result = vkCreateRenderPass(device, &renderpass_info, NULL, out_renderpass);
    if (result != VK_SUCCESS) {
        return GRAPHICS_ERROR_GENERIC;
    }

    return GRAPHICS_OK;
}

GraphicsError graphics_create_swapchain_image_views(GraphicsSystem* system,
                                                     GraphicsSwapchain* swapchain,
                                                     VkImageView** out_image_views,
                                                     uint32_t* out_count) {
    if (!system || !swapchain || !out_image_views || !out_count) {
        return GRAPHICS_ERROR_INVALID_ARG;
    }

    VkDevice device = graphics_system_get_vk_device(system);
    if (device == VK_NULL_HANDLE) {
        return GRAPHICS_ERROR_GENERIC;
    }

    uint32_t image_count = graphics_swapchain_get_image_count(swapchain);
    if (image_count == 0) {
        return GRAPHICS_ERROR_GENERIC;
    }

    VkFormat format = graphics_swapchain_get_image_format(swapchain);
    if (format == VK_FORMAT_UNDEFINED) {
        return GRAPHICS_ERROR_GENERIC;
    }

    /* Allocate image views array */
    VkImageView* image_views = (VkImageView*)calloc(image_count, sizeof(VkImageView));
    if (!image_views) {
        return GRAPHICS_ERROR_OUT_OF_MEMORY;
    }

    /* For each swapchain image, create a VkImageView */
    for (uint32_t i = 0; i < image_count; i++) {
        VkImage image = graphics_swapchain_get_image(swapchain, i);
        if (image == VK_NULL_HANDLE) {
            free(image_views);
            return GRAPHICS_ERROR_GENERIC;
        }

        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };

        VkResult result = vkCreateImageView(device, &view_info, NULL, &image_views[i]);
        if (result != VK_SUCCESS) {
            /* Clean up previously created image views */
            for (uint32_t j = 0; j < i; j++) {
                vkDestroyImageView(device, image_views[j], NULL);
            }
            free(image_views);
            return GRAPHICS_ERROR_GENERIC;
        }
    }

    *out_image_views = image_views;
    *out_count = image_count;
    return GRAPHICS_OK;
}

GraphicsError graphics_create_swapchain_framebuffers(GraphicsSystem* system,
                                                      GraphicsSwapchain* swapchain,
                                                      VkRenderPass renderpass,
                                                      VkImageView* image_views,
                                                      VkFramebuffer** out_framebuffers,
                                                      uint32_t* out_count) {
    if (!system || !swapchain || renderpass == VK_NULL_HANDLE || !image_views ||
        !out_framebuffers || !out_count) {
        return GRAPHICS_ERROR_INVALID_ARG;
    }

    VkDevice device = graphics_system_get_vk_device(system);
    if (device == VK_NULL_HANDLE) {
        return GRAPHICS_ERROR_GENERIC;
    }

    uint32_t image_count = graphics_swapchain_get_image_count(swapchain);
    if (image_count == 0) {
        return GRAPHICS_ERROR_GENERIC;
    }

    uint32_t width, height;
    graphics_swapchain_get_dimensions(swapchain, &width, &height);

    /* Allocate framebuffers array */
    VkFramebuffer* framebuffers = (VkFramebuffer*)calloc(image_count, sizeof(VkFramebuffer));
    if (!framebuffers) {
        return GRAPHICS_ERROR_OUT_OF_MEMORY;
    }

    /* For each image view, create a VkFramebuffer */
    for (uint32_t i = 0; i < image_count; i++) {
        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = renderpass,
            .attachmentCount = 1,
            .pAttachments = &image_views[i],
            .width = width,
            .height = height,
            .layers = 1,
        };

        VkResult result = vkCreateFramebuffer(device, &framebuffer_info, NULL, &framebuffers[i]);
        if (result != VK_SUCCESS) {
            /* Clean up previously created framebuffers */
            for (uint32_t j = 0; j < i; j++) {
                vkDestroyFramebuffer(device, framebuffers[j], NULL);
            }
            free(framebuffers);
            return GRAPHICS_ERROR_GENERIC;
        }
    }

    *out_framebuffers = framebuffers;
    *out_count = image_count;
    return GRAPHICS_OK;
}
