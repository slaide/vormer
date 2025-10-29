#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <stdbool.h>
#include <platform/platform.h>
#include <vulkan/vulkan.h>

/* ============================================================================
 * Error Codes
 * ============================================================================ */

typedef enum {
    GRAPHICS_OK = 0,                        /* Operation succeeded */
    GRAPHICS_ERROR_GENERIC = 1,             /* Generic/unspecified error */
    GRAPHICS_ERROR_OUT_OF_MEMORY = 2,       /* Memory allocation failed */
    GRAPHICS_ERROR_INVALID_ARG = 3,         /* Invalid argument provided */
    GRAPHICS_ERROR_OUT_OF_DATE = 4,         /* Swapchain is out of date (needs recreation) */
} GraphicsError;

/* ============================================================================
 * Graphics System Initialization
 *
 * The graphics system encapsulates the Vulkan instance, physical device, and
 * logical device. After initialization, the underlying Vulkan objects are
 * exposed directly for resource creation (render passes, pipelines, buffers,
 * images, etc.) - there is no wrapping or abstraction layer beyond this point.
 *
 * This design provides the following benefits:
 * - Minimal abstraction for Vulkan-specific concepts (which are already well-designed)
 * - Full access to Vulkan's capabilities without API gaps
 * - Clear ownership: platform/window binding + queue management are abstracted,
 *   everything else uses Vulkan directly
 * ============================================================================ */

/* Opaque handle to the graphics system. Contains:
 * - VkInstance (Vulkan connection)
 * - VkPhysicalDevice (GPU selection)
 * - VkDevice (logical device interface)
 * - VkQueue handles for different queue types
 *
 * After graphics_system_create(), you can call graphics_system_get_vk_device(),
 * graphics_system_get_vk_instance(), etc. to access Vulkan objects directly. */
typedef struct GraphicsSystem GraphicsSystem;

/* Surface and swapchain are still abstracted since they involve platform-specific
 * window binding. After swapchain creation, you have VkImages and image indices
 * that you use with Vulkan directly. */
typedef struct GraphicsSurface GraphicsSurface;
typedef struct GraphicsSwapchain GraphicsSwapchain;

/* ============================================================================
 * Queue Request Mechanism
 *
 * When creating the graphics system, you specify what queue types you need
 * and what operations each queue should support. The device creation function
 * populates queue handles into these request structures.
 *
 * This allows flexible queue usage: you can request separate graphics/transfer/
 * compute queues, or share a single queue for all operations, depending on
 * GPU capabilities and your needs.
 * ============================================================================ */

/* Queue capability flags - specify what a queue should support */
typedef enum {
    GRAPHICS_QUEUE_GRAPHICS = 0x0001,   /* Can execute graphics commands */
    GRAPHICS_QUEUE_COMPUTE = 0x0002,    /* Can execute compute commands */
    GRAPHICS_QUEUE_TRANSFER = 0x0004,   /* Can execute transfer commands */
} GraphicsQueueCapability;

/* A queue request describes what queue type you need and what operations it
 * must support. After device creation, the 'queue' field is populated with
 * the actual Vulkan queue handle. */
typedef struct {
    /* Input: What operations this queue needs to support (bitwise OR of
     * GraphicsQueueCapability flags). The device creation function will find
     * a queue family that supports all requested capabilities. */
    uint32_t required_capabilities;

    /* Output: After graphics_system_create(), this contains the VkQueue handle
     * for this request. Initialize to VK_NULL_HANDLE before passing to create. */
    VkQueue queue;
} GraphicsQueueRequest;

/* ============================================================================
 * System Initialization
 * ============================================================================ */

typedef struct {
    /* Enable Vulkan validation layers (VK_LAYER_KHRONOS_validation).
     * Useful for development but adds overhead. */
    bool enable_validation;

    /* Application name for Vulkan profiling/debugging tools */
    const char* app_name;

    /* Requested queues. Typically you'll request at least a graphics queue.
     * You can request multiple queues with different capability sets.
     * The device creation function will populate the 'queue' field in each. */
    GraphicsQueueRequest* queue_requests;
    uint32_t queue_request_count;

    /* Platform instance (needed to query required Vulkan extensions for
     * the current platform and window system). */
    Platform* platform;
} GraphicsSystemCreateInfo;

/* Initialize the graphics system: create Vulkan instance, select GPU, and
 * create logical device with requested queues. */
GraphicsError graphics_system_create(const GraphicsSystemCreateInfo* create_info,
                                      GraphicsSystem** out_system);

/* Destroy the graphics system and release all resources. Any Vulkan objects
 * created from this system's device (pipelines, buffers, images, etc.) must
 * be destroyed first. */
void graphics_system_destroy(GraphicsSystem* system);

/* Wait for the device to finish all pending GPU operations. Blocks until
 * the GPU is idle. Useful before checking results or destroying resources. */
GraphicsError graphics_system_wait_idle(GraphicsSystem* system);

/* Get the underlying Vulkan objects from the graphics system. These are
 * exposed so you can use Vulkan directly for resource creation. */
VkInstance graphics_system_get_vk_instance(GraphicsSystem* system);
VkPhysicalDevice graphics_system_get_vk_physical_device(GraphicsSystem* system);
VkDevice graphics_system_get_vk_device(GraphicsSystem* system);

/* ============================================================================
 * Surface and Swapchain
 *
 * These are still abstracted because they tightly couple Vulkan with the
 * platform windowing system (X11, Wayland, Win32, etc.). After swapchain
 * creation, you work directly with VkImages and indices.
 *
 * Color format is always sRGB (for proper color space rendering).
 *
 * Important: Swapchain images are allocated by the swapchain and should not
 * be created/destroyed manually. Image views and framebuffers wrapping these
 * images are created and owned by your code (using Vulkan directly).
 * ============================================================================ */

typedef struct {
    /* Platform window to create a Vulkan surface for */
    PlatformWindow* window;
} GraphicsSurfaceCreateInfo;

/* Create a Vulkan surface for a platform window. The surface represents the
 * connection between Vulkan rendering and the windowing system. */
GraphicsError graphics_surface_create(GraphicsSystem* system,
                                       const GraphicsSurfaceCreateInfo* create_info,
                                       GraphicsSurface** out_surface);

/* Destroy the surface. */
void graphics_surface_destroy(GraphicsSystem* system,
                              GraphicsSurface* surface);

/* Get the underlying VkSurfaceKHR from the surface wrapper */
VkSurfaceKHR graphics_surface_get_vk_surface(GraphicsSurface* surface);

/* ============================================================================
 * Swapchain
 *
 * The swapchain manages presentation: acquiring images to render into,
 * and presenting rendered images to the display with proper synchronization.
 * Vsync is always enabled - the swapchain presents in sync with the display
 * refresh cycle.
 *
 * The swapchain provides a list of VkImages that you can render into. When
 * presenting, you're presenting one of these swapchain images. The swapchain
 * handles acquiring the next available image (blocking if necessary) and
 * manages synchronization with the display.
 *
 * Color format is always sRGB. Depth attachments and other resources are your
 * responsibility to create using Vulkan directly.
 * ============================================================================ */

typedef struct {
    /* Surface to create swapchain for */
    GraphicsSurface* surface;

    /* Desired swapchain dimensions. Actual dimensions may differ based on
     * surface constraints (e.g., DPI scaling, fullscreen mode requirements). */
    uint32_t desired_width;
    uint32_t desired_height;

    /* Old swapchain, if recreating (e.g., on window resize). Set to NULL for
     * initial creation. Allows resource reuse. */
    GraphicsSwapchain* old_swapchain;
} GraphicsSwapchainCreateInfo;

/* Create a swapchain for presenting to the surface. The swapchain manages
 * multiple images (typically 2-3) that are rotated for display. */
GraphicsError graphics_swapchain_create(GraphicsSystem* system,
                                         const GraphicsSwapchainCreateInfo* create_info,
                                         GraphicsSwapchain** out_swapchain);

/* Destroy the swapchain. Any image views or framebuffers wrapping swapchain
 * images should be destroyed first. */
void graphics_swapchain_destroy(GraphicsSystem* system,
                                GraphicsSwapchain* swapchain);

/* Get the actual dimensions of the swapchain. May differ from desired dimensions. */
void graphics_swapchain_get_dimensions(GraphicsSwapchain* swapchain,
                                        uint32_t* out_width,
                                        uint32_t* out_height);

/* Get the number of images in the swapchain (typically 2 or 3) */
uint32_t graphics_swapchain_get_image_count(GraphicsSwapchain* swapchain);

/* Get a swapchain image by index. Returns a VkImage you can create image views
 * and framebuffers for. The returned image is owned by the swapchain and should
 * not be destroyed. */
VkImage graphics_swapchain_get_image(GraphicsSwapchain* swapchain,
                                      uint32_t index);

/* Get the image format of swapchain images (always sRGB color format).
 * Useful for creating image views and render passes. */
VkFormat graphics_swapchain_get_image_format(GraphicsSwapchain* swapchain);

/* Acquire the next image from the swapchain for rendering. This blocks until
 * an image is available (respecting vsync).
 * image_acquired_semaphore: Semaphore to signal when image is ready (created once, reused). */
GraphicsError graphics_swapchain_acquire_image(GraphicsSystem* system,
                                                GraphicsSwapchain* swapchain,
                                                uint32_t* out_image_index,
                                                VkSemaphore image_acquired_semaphore);

/* Present a rendered image to the display. Submits the image for presentation
 * and handles synchronization with the display's refresh cycle (vsync).
 * render_complete_semaphore: Semaphore signaled when rendering to the image is complete. */
GraphicsError graphics_swapchain_present(GraphicsSystem* system,
                                          GraphicsSwapchain* swapchain,
                                          uint32_t image_index,
                                          VkSemaphore render_complete_semaphore);

/* ============================================================================
 * Command Buffer Pool and Frame Synchronization
 *
 * The command buffer pool manages per-frame command buffers with proper
 * synchronization. It maintains a ring of command buffers (typically 2-3)
 * matching the number of swapchain images, plus fences and semaphores for
 * GPU/CPU synchronization.
 *
 * Each frame:
 * 1. Wait for the frame's fence (CPU blocks until GPU finished with this buffer)
 * 2. Reset the command buffer
 * 3. Record rendering commands
 * 4. Submit and wait for image_acquired semaphore from swapchain
 * 5. Signal rendering_complete semaphore for presentation
 *
 * This allows new commands to be recorded every frame while ensuring the GPU
 * has finished using previous commands before we reuse the buffer.
 * ============================================================================ */

typedef struct GraphicsCommandPool GraphicsCommandPool;

typedef struct {
    /* Number of command buffers to allocate (typically 2-3, matching swapchain).
     * This should match the number of swapchain images for triple buffering. */
    uint32_t buffer_count;

    /* The queue family this pool will submit to (from graphics system queue requests) */
    uint32_t queue_family_index;
} GraphicsCommandPoolCreateInfo;

/* Create a command buffer pool with frame synchronization primitives. */
GraphicsError graphics_command_pool_create(GraphicsSystem* system,
                                            const GraphicsCommandPoolCreateInfo* create_info,
                                            GraphicsCommandPool** out_pool);

/* Destroy the command buffer pool and release all resources. */
void graphics_command_pool_destroy(GraphicsSystem* system,
                                    GraphicsCommandPool* pool);

/* Get the next command buffer for this frame. Blocks until the GPU has
 * finished using the previous command buffer at this index. The buffer
 * is automatically reset and ready for command recording.
 *
 * frame_index: Current frame number (0, 1, 2, ...). Only the least significant
 *              bits are used (module buffer_count), so you can use a simple
 *              frame counter and it will wrap automatically. */
GraphicsError graphics_command_pool_get_command_buffer(GraphicsSystem* system,
                                                        GraphicsCommandPool* pool,
                                                        uint32_t frame_index,
                                                        VkCommandBuffer* out_buffer);

/* Get the semaphore that signals when rendering is complete and the image
 * is ready for presentation. This is signaled when the command buffer is
 * submitted to the queue. */
VkSemaphore graphics_command_pool_get_render_complete_semaphore(GraphicsCommandPool* pool,
                                                                 uint32_t frame_index);

/* Get the semaphore that signals when a swapchain image has been acquired
 * and is ready for rendering. Used with vkAcquireNextImageKHR. */
VkSemaphore graphics_command_pool_get_image_acquired_semaphore(GraphicsCommandPool* pool,
                                                                uint32_t frame_index);

/* Submit the command buffer for the current frame and queue it for presentation.
 * This handles synchronization with the image_acquired semaphore from the swapchain.
 *
 * image_acquired_semaphore: The semaphore signaled by swapchain_acquire_image(),
 *                           indicating the swapchain image is ready for rendering.
 * command_buffer: The command buffer to submit (from get_command_buffer()). */
GraphicsError graphics_command_pool_submit(GraphicsSystem* system,
                                            GraphicsCommandPool* pool,
                                            uint32_t frame_index,
                                            VkSemaphore image_acquired_semaphore,
                                            VkCommandBuffer command_buffer);

/* ============================================================================
 * Helper Functions for Common Tasks
 *
 * These are convenience functions for creating commonly-needed Vulkan objects.
 * You can use Vulkan directly for anything not covered here.
 * ============================================================================ */

/* Create a simple single-attachment renderpass for rendering to a swapchain image.
 * The renderpass clears the image at the start and stores the result.
 *
 * color_format: The format of the color attachment (typically from swapchain) */
GraphicsError graphics_create_simple_renderpass(GraphicsSystem* system,
                                                 VkFormat color_format,
                                                 VkRenderPass* out_renderpass);

/* Create image views for all swapchain images.
 * The views are allocated in a single array; you must free it when done. */
GraphicsError graphics_create_swapchain_image_views(GraphicsSystem* system,
                                                     GraphicsSwapchain* swapchain,
                                                     VkImageView** out_image_views,
                                                     uint32_t* out_count);

/* Create framebuffers for each swapchain image.
 * The framebuffers are allocated in a single array; you must free it when done.
 * All framebuffers use the same renderpass and dimensions. */
GraphicsError graphics_create_swapchain_framebuffers(GraphicsSystem* system,
                                                      GraphicsSwapchain* swapchain,
                                                      VkRenderPass renderpass,
                                                      VkImageView* image_views,
                                                      VkFramebuffer** out_framebuffers,
                                                      uint32_t* out_count);

/* ============================================================================
 * Direct Vulkan Usage
 *
 * Beyond initialization and swapchain management, all graphics work uses Vulkan
 * directly. Create render passes, pipelines, buffers, images, image views,
 * framebuffers, command pools, and command buffers using the standard Vulkan API.
 *
 * The VkDevice and VkQueue handles from the graphics system are everything you
 * need. Consider creating helper functions for common tasks (shader loading,
 * buffer allocation, command buffer recording, etc.) as needed.
 *
 * Key Vulkan objects you'll create:
 * - VkRenderPass: describes attachment layout and transitions (supports subpasses,
 *   external dependencies, and complex attachment transitions)
 * - VkPipeline: graphics/compute pipeline (shaders, rasterization state, etc.)
 * - VkFramebuffer: binds images to render pass attachment slots
 * - VkBuffer: GPU memory for vertex, index, uniform, storage data
 * - VkImage: textures and depth attachments
 * - VkImageView: typed view into image data
 * - VkCommandPool: allocates command buffers
 * - VkCommandBuffer: records GPU commands (rendering, copying, etc.)
 * - VkDescriptorSetLayout/Pool/Set: bindings for buffers, images, samplers
 * - VkSampler: texture filtering and addressing
 * - VkSemaphore/VkFence: synchronization primitives
 * ============================================================================ */

#endif /* GRAPHICS_H */
