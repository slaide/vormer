# 3D Game Engine - Technical Design Document

## Table of Contents
1. [Overview](#1-overview)
2. [Project Structure](#2-project-structure)
3. [Module Architecture](#3-module-architecture)
   - [Platform Module](#31-platform-module)
   - [Graphics Module](#32-graphics-module-vulkan)
   - [Parsers Module](#33-parsers-module)
   - [Math Module](#34-math-module)
   - [Scripting Module](#35-scripting-module-lua)
4. [Scene System](#4-scene-system-ecs-inspired)
5. [Rendering Pipeline](#5-rendering-pipeline)
6. [Build System](#6-build-system)
7. [Dependencies Management](#7-dependencies-management)
8. [Future Considerations](#8-future-considerations)

---

## 1. Overview

This document outlines the architecture and design of a cross-platform 3D game engine written in GNU C23. The engine is designed with minimal external dependencies, providing direct platform integration and maximum control over the rendering pipeline.

### 1.1 Core Technologies
- **Language**: GNU C23
- **Build System**: Makefile
- **Graphics API**: Vulkan
- **Scripting**: Lua 5.4 with Teal type system
- **Target Platforms**: Linux (X11, Wayland), macOS, Android

### 1.2 Design Philosophy
- **Minimal Dependencies**: Direct platform integration without heavy middleware
- **Modular Architecture**: Clear separation of concerns with independent, testable modules
- **ECS-Inspired Rendering**: Flexible component-based scene graph system
- **Cross-Platform First**: Platform abstraction built from the ground up
- **Single-Pass Rendering**: Walk scene tree once, collecting all necessary data during traversal

---

## 2. Project Structure

```
vormer/
├── src/
│   ├── main.c
│   ├── core/              # Core engine systems
│   ├── platform/          # Platform abstraction layer
│   ├── graphics/          # Vulkan rendering backend
│   ├── parsers/           # File format loaders/writers
│   ├── math/              # 3D math library
│   ├── scene/             # Scene graph and ECS
│   └── scripting/         # Lua integration layer
├── include/               # Public headers (mirrors src structure)
│   ├── core/
│   ├── platform/
│   ├── graphics/
│   ├── parsers/
│   ├── math/
│   ├── scene/
│   └── scripting/
├── external/
│   ├── lua/               # Lua 5.4 (git submodule)
│   └── teal/              # Teal language compiler (git submodule)
├── resources/             # Runtime resources and final application
│   └── scripts/           # Teal/Lua game scripts
├── shaders/               # Vulkan SPIR-V shaders
├── Makefile
└── README.md
```

---

## 3. Module Architecture

### 3.1 Platform Module

**Purpose**: Abstract window creation, input handling, and event management across all target platforms.

#### Responsibilities
- Window lifecycle (create, destroy, resize, fullscreen, minimize/restore)
- Input event handling (keyboard, mouse, touch, gamepad)
- Platform event loop abstraction
- Vulkan surface creation for platform-specific windowing systems
- Window decoration handling (client-side vs server-side)
- DPI scaling and multi-monitor support
- Cursor management (visibility, capture, custom cursors)

#### Platform-Specific Implementations

| Platform | Implementation | Window Decorations | Notes |
|----------|---------------|-------------------|-------|
| **Linux X11** | XCB bindings | Server-side | Traditional X11 window management |
| **Linux Wayland** | libwayland-client | Client-side | Need to render window chrome ourselves |
| **macOS** | Cocoa/AppKit via Objective-C | Native | NSWindow with standard decorations |
| **Android** | ANativeWindow, ALooper | Fullscreen | No traditional window decorations |

#### Core Types

```c
typedef struct Platform Platform;
typedef struct PlatformWindow PlatformWindow;
```

#### Window Management API

```c
Platform* platform_init(void);
void platform_shutdown(Platform* platform);

typedef struct {
    const char* title;
    int width;
    int height;
    bool fullscreen;
    bool resizable;
    bool decorated;      // Window decorations (title bar, borders)
    bool vsync;
} WindowConfig;

PlatformWindow* platform_create_window(Platform* platform, const WindowConfig* config);
void platform_destroy_window(PlatformWindow* window);
void platform_set_window_title(PlatformWindow* window, const char* title);
void platform_get_window_size(PlatformWindow* window, int* width, int* height);
void platform_get_framebuffer_size(PlatformWindow* window, int* width, int* height);
void platform_set_window_size(PlatformWindow* window, int width, int height);
void platform_set_fullscreen(PlatformWindow* window, bool fullscreen);
void platform_show_window(PlatformWindow* window);
void platform_hide_window(PlatformWindow* window);
void platform_focus_window(PlatformWindow* window);

// DPI and scaling
float platform_get_content_scale(PlatformWindow* window);
void platform_get_monitor_dpi(PlatformWindow* window, float* xdpi, float* ydpi);
```

#### Event System

The event system must normalize input across platforms, handling differences in key codes, coordinate systems, and device capabilities.

```c
typedef enum {
    EVENT_NONE,
    
    // Window events
    EVENT_WINDOW_CLOSE,
    EVENT_WINDOW_RESIZE,
    EVENT_WINDOW_FOCUS_GAINED,
    EVENT_WINDOW_FOCUS_LOST,
    EVENT_WINDOW_MINIMIZED,
    EVENT_WINDOW_RESTORED,
    EVENT_WINDOW_MOVED,
    EVENT_WINDOW_DPI_CHANGED,
    
    // Keyboard events
    EVENT_KEY_PRESS,
    EVENT_KEY_RELEASE,
    EVENT_KEY_REPEAT,
    EVENT_TEXT_INPUT,        // UTF-8 text input for text fields
    
    // Mouse events
    EVENT_MOUSE_BUTTON_PRESS,
    EVENT_MOUSE_BUTTON_RELEASE,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_SCROLL,      // Supports both vertical and horizontal
    EVENT_MOUSE_ENTER,       // Mouse entered window
    EVENT_MOUSE_LEAVE,       // Mouse left window
    
    // Touch events (mobile)
    EVENT_TOUCH_BEGIN,
    EVENT_TOUCH_MOVE,
    EVENT_TOUCH_END,
    EVENT_TOUCH_CANCEL,
    
    // Gamepad events
    EVENT_GAMEPAD_CONNECTED,
    EVENT_GAMEPAD_DISCONNECTED,
    EVENT_GAMEPAD_BUTTON_PRESS,
    EVENT_GAMEPAD_BUTTON_RELEASE,
    EVENT_GAMEPAD_AXIS_MOTION,
    
    // System events
    EVENT_QUIT
} EventType;

typedef enum {
    // Printable keys
    KEY_SPACE = 32,
    KEY_APOSTROPHE = 39,
    KEY_COMMA = 44,
    KEY_MINUS = 45,
    KEY_PERIOD = 46,
    KEY_SLASH = 47,
    KEY_0 = 48, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    KEY_SEMICOLON = 59,
    KEY_EQUAL = 61,
    KEY_A = 65, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
    KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
    KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
    KEY_LEFT_BRACKET = 91,
    KEY_BACKSLASH = 92,
    KEY_RIGHT_BRACKET = 93,
    KEY_GRAVE_ACCENT = 96,
    
    // Function keys
    KEY_ESCAPE = 256,
    KEY_ENTER,
    KEY_TAB,
    KEY_BACKSPACE,
    KEY_INSERT,
    KEY_DELETE,
    KEY_RIGHT,
    KEY_LEFT,
    KEY_DOWN,
    KEY_UP,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_HOME,
    KEY_END,
    KEY_CAPS_LOCK,
    KEY_SCROLL_LOCK,
    KEY_NUM_LOCK,
    KEY_PRINT_SCREEN,
    KEY_PAUSE,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    
    // Keypad
    KEY_KP_0 = 320, KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_4,
    KEY_KP_5, KEY_KP_6, KEY_KP_7, KEY_KP_8, KEY_KP_9,
    KEY_KP_DECIMAL,
    KEY_KP_DIVIDE,
    KEY_KP_MULTIPLY,
    KEY_KP_SUBTRACT,
    KEY_KP_ADD,
    KEY_KP_ENTER,
    KEY_KP_EQUAL,
    
    // Modifiers
    KEY_LEFT_SHIFT = 340,
    KEY_LEFT_CONTROL,
    KEY_LEFT_ALT,
    KEY_LEFT_SUPER,      // Windows/Command key
    KEY_RIGHT_SHIFT,
    KEY_RIGHT_CONTROL,
    KEY_RIGHT_ALT,
    KEY_RIGHT_SUPER
} KeyCode;

typedef enum {
    MOUSE_BUTTON_LEFT = 0,
    MOUSE_BUTTON_RIGHT = 1,
    MOUSE_BUTTON_MIDDLE = 2,
    MOUSE_BUTTON_4 = 3,
    MOUSE_BUTTON_5 = 4
} MouseButton;

typedef enum {
    GAMEPAD_BUTTON_A = 0,        // Cross on PlayStation
    GAMEPAD_BUTTON_B,            // Circle on PlayStation
    GAMEPAD_BUTTON_X,            // Square on PlayStation
    GAMEPAD_BUTTON_Y,            // Triangle on PlayStation
    GAMEPAD_BUTTON_LEFT_BUMPER,
    GAMEPAD_BUTTON_RIGHT_BUMPER,
    GAMEPAD_BUTTON_BACK,
    GAMEPAD_BUTTON_START,
    GAMEPAD_BUTTON_GUIDE,
    GAMEPAD_BUTTON_LEFT_THUMB,
    GAMEPAD_BUTTON_RIGHT_THUMB,
    GAMEPAD_BUTTON_DPAD_UP,
    GAMEPAD_BUTTON_DPAD_RIGHT,
    GAMEPAD_BUTTON_DPAD_DOWN,
    GAMEPAD_BUTTON_DPAD_LEFT
} GamepadButton;

typedef enum {
    GAMEPAD_AXIS_LEFT_X = 0,
    GAMEPAD_AXIS_LEFT_Y,
    GAMEPAD_AXIS_RIGHT_X,
    GAMEPAD_AXIS_RIGHT_Y,
    GAMEPAD_AXIS_LEFT_TRIGGER,
    GAMEPAD_AXIS_RIGHT_TRIGGER
} GamepadAxis;

typedef struct {
    int mods;                    // Bitfield: SHIFT=1, CTRL=2, ALT=4, SUPER=8
} KeyboardModifiers;

typedef struct Event {
    EventType type;
    double timestamp;            // Event timestamp in seconds
    
    union {
        // Window events
        struct {
            int width, height;
        } window_resize;
        
        struct {
            int x, y;
        } window_move;
        
        struct {
            float xscale, yscale;
        } window_dpi;
        
        // Keyboard events
        struct {
            KeyCode key;
            int scancode;        // Platform-specific scancode for physical key
            KeyboardModifiers mods;
        } keyboard;
        
        struct {
            char text[32];       // UTF-8 encoded text
        } text_input;
        
        // Mouse events
        struct {
            MouseButton button;
            int x, y;            // Window coordinates
            KeyboardModifiers mods;
        } mouse_button;
        
        struct {
            int x, y;            // Window coordinates
            int dx, dy;          // Delta from last position
        } mouse_move;
        
        struct {
            float x, y;          // Scroll delta (x for horizontal, y for vertical)
            bool precise;        // True for high-precision (trackpad), false for wheel
        } mouse_scroll;
        
        // Touch events
        struct {
            int64_t touch_id;
            float x, y;          // Normalized coordinates [0, 1]
            float pressure;      // [0, 1] if supported
        } touch;
        
        // Gamepad events
        struct {
            int gamepad_id;      // Gamepad device ID
            const char* name;    // Gamepad name
        } gamepad_connection;
        
        struct {
            int gamepad_id;
            GamepadButton button;
        } gamepad_button;
        
        struct {
            int gamepad_id;
            GamepadAxis axis;
            float value;         // [-1, 1] for sticks, [0, 1] for triggers
        } gamepad_axis;
    };
} Event;

bool platform_poll_event(Platform* platform, Event* event);
void platform_wait_events(Platform* platform);  // Block until event available
```

#### Input State Queries

For game loops, it's often convenient to query current input state directly rather than process events.

```c
bool platform_is_key_down(Platform* platform, KeyCode key);
bool platform_is_mouse_button_down(Platform* platform, MouseButton button);
void platform_get_mouse_position(Platform* platform, int* x, int* y);
void platform_get_mouse_delta(Platform* platform, int* dx, int* dy);

// Gamepad state queries
bool platform_is_gamepad_connected(Platform* platform, int gamepad_id);
bool platform_is_gamepad_button_down(Platform* platform, int gamepad_id, GamepadButton button);
float platform_get_gamepad_axis(Platform* platform, int gamepad_id, GamepadAxis axis);
const char* platform_get_gamepad_name(Platform* platform, int gamepad_id);
```

#### Cursor Management

```c
typedef enum {
    CURSOR_ARROW,
    CURSOR_IBEAM,
    CURSOR_CROSSHAIR,
    CURSOR_HAND,
    CURSOR_HRESIZE,
    CURSOR_VRESIZE,
    CURSOR_HIDDEN
} CursorType;

void platform_set_cursor(PlatformWindow* window, CursorType cursor);
void platform_set_cursor_capture(PlatformWindow* window, bool capture);
```

#### Clipboard

```c
const char* platform_get_clipboard_text(Platform* platform);
void platform_set_clipboard_text(Platform* platform, const char* text);
```

#### Vulkan Integration

```c
VkSurfaceKHR platform_create_vulkan_surface(PlatformWindow* window, VkInstance instance);
const char** platform_get_required_vulkan_extensions(uint32_t* count);
```

#### Platform-Specific Considerations

**Window Decorations**:
- **X11**: Server-side decorations handled by window manager
- **Wayland**: Client-side decorations required; need to render title bar, borders, buttons
- **macOS**: Native decorations via NSWindow
- **Android**: Fullscreen by default, no decorations

**Coordinate Systems**:
- **X11**: Origin at top-left, Y increases downward
- **Wayland**: Origin at top-left, Y increases downward
- **macOS**: Origin at bottom-left, Y increases upward (need to flip for consistency)
- **Android**: Origin at top-left, Y increases downward

**DPI Scaling**:
- **X11**: Xft.dpi or manual detection
- **Wayland**: Per-output scale factor
- **macOS**: Retina displays with backing scale factor
- **Android**: Density-independent pixels (dp)

**Scrolling**:
- **X11**: Discrete scroll events (button 4/5)
- **Wayland**: Continuous scroll with pixel/discrete modes
- **macOS**: High-precision trackpad scrolling with momentum
- **Android**: Touch gestures

---

### 3.2 Graphics Module (Vulkan)

**Purpose**: Manage all rendering operations using the Vulkan API.

#### Responsibilities
- Vulkan instance and device initialization
- Swapchain management and presentation
- Command buffer recording and submission
- Graphics pipeline creation and caching
- Descriptor set management
- Resource management (buffers, textures, samplers)
- Shader module loading (SPIR-V)
- Render pass orchestration
- Synchronization (fences, semaphores)
- Memory allocation and management

#### Core Types

```c
typedef struct GraphicsContext GraphicsContext;
typedef struct Mesh Mesh;
typedef struct Material Material;
typedef struct Texture Texture;
typedef struct Shader Shader;
typedef struct RenderPass RenderPass;
typedef struct Pipeline Pipeline;
typedef struct UniformBuffer UniformBuffer;
```

#### Context Management

```c
GraphicsContext* graphics_init(PlatformWindow* window);
void graphics_shutdown(GraphicsContext* ctx);
void graphics_wait_idle(GraphicsContext* ctx);
void graphics_resize(GraphicsContext* ctx, int width, int height);
```

#### Frame Lifecycle

```c
bool graphics_begin_frame(GraphicsContext* ctx);
void graphics_end_frame(GraphicsContext* ctx);
```

#### Resource Management

```c
// Vertex data
typedef struct {
    vec3 position;
    vec3 normal;
    vec2 texcoord;
    vec4 color;     // Optional vertex color
} Vertex;

Mesh* graphics_create_mesh(
    GraphicsContext* ctx,
    const Vertex* vertices,
    uint32_t vertex_count,
    const uint32_t* indices,
    uint32_t index_count
);
void graphics_destroy_mesh(GraphicsContext* ctx, Mesh* mesh);

// Textures
typedef enum {
    TEXTURE_FORMAT_RGBA8,
    TEXTURE_FORMAT_RGB8,
    TEXTURE_FORMAT_R8,
    TEXTURE_FORMAT_RGBA16F,
    TEXTURE_FORMAT_DEPTH24_STENCIL8
} TextureFormat;

typedef enum {
    TEXTURE_FILTER_NEAREST,
    TEXTURE_FILTER_LINEAR
} TextureFilter;

typedef enum {
    TEXTURE_WRAP_REPEAT,
    TEXTURE_WRAP_MIRRORED_REPEAT,
    TEXTURE_WRAP_CLAMP_TO_EDGE
} TextureWrap;

typedef struct {
    TextureFilter min_filter;
    TextureFilter mag_filter;
    TextureWrap wrap_u;
    TextureWrap wrap_v;
    bool generate_mipmaps;
} TextureConfig;

Texture* graphics_create_texture(
    GraphicsContext* ctx,
    uint32_t width,
    uint32_t height,
    TextureFormat format,
    const void* pixels,
    const TextureConfig* config
);
void graphics_destroy_texture(GraphicsContext* ctx, Texture* texture);

// Shaders
Shader* graphics_load_shader(
    GraphicsContext* ctx,
    const char* vert_path,
    const char* frag_path
);
void graphics_destroy_shader(GraphicsContext* ctx, Shader* shader);

// Materials
Material* graphics_create_material(GraphicsContext* ctx, Shader* shader);
void graphics_set_material_texture(Material* material, const char* name, Texture* texture);
void graphics_set_material_float(Material* material, const char* name, float value);
void graphics_set_material_vec3(Material* material, const char* name, vec3 value);
void graphics_set_material_vec4(Material* material, const char* name, vec4 value);
void graphics_destroy_material(GraphicsContext* ctx, Material* material);
```

#### Rendering Commands

```c
void graphics_draw_mesh(
    GraphicsContext* ctx,
    Mesh* mesh,
    Material* material,
    mat4 model,
    mat4 view,
    mat4 projection
);

void graphics_set_viewport(GraphicsContext* ctx, int x, int y, int width, int height);
void graphics_set_scissor(GraphicsContext* ctx, int x, int y, int width, int height);
void graphics_clear_color(GraphicsContext* ctx, float r, float g, float b, float a);
void graphics_clear_depth(GraphicsContext* ctx, float depth);
```

#### Render Context

During scene traversal, we accumulate rendering data into a render context that is then submitted.

```c
typedef struct RenderContext {
    // Accumulated draw calls
    struct {
        Mesh* mesh;
        Material* material;
        mat4 model_matrix;
    }* draw_calls;
    uint32_t draw_call_count;
    
    // Lights collected during traversal
    struct {
        Light* light;
        mat4 transform;
    }* lights;
    uint32_t light_count;
    
    // Cameras
    mat4 view;
    mat4 projection;
} RenderContext;

RenderContext* render_context_create(void);
void render_context_destroy(RenderContext* ctx);
void render_context_clear(RenderContext* ctx);
void render_context_add_draw_call(RenderContext* ctx, Mesh* mesh, Material* material, mat4 model);
void render_context_add_light(RenderContext* ctx, Light* light, mat4 transform);
void render_context_set_camera(RenderContext* ctx, mat4 view, mat4 projection);
void render_context_submit(GraphicsContext* gfx, RenderContext* ctx);
```

---

### 3.3 Parsers Module

**Purpose**: Handle loading and saving of various file formats needed by the engine.

#### Responsibilities
- Image loading and saving (JPEG, PNG, TGA, BMP)
- 3D model loading (OBJ, glTF/GLB)
- JSON parsing and serialization
- Scene serialization/deserialization
- Configuration file parsing

#### Core Types

```c
typedef struct Image Image;
typedef struct Model Model;
typedef struct JsonValue JsonValue;
```

#### Image I/O

```c
typedef struct Image {
    uint32_t width;
    uint32_t height;
    uint32_t channels;     // 1=grayscale, 3=RGB, 4=RGBA
    uint8_t* data;
} Image;

Image* image_load(const char* path);
Image* image_load_from_memory(const uint8_t* buffer, size_t size);
bool image_save(const char* path, const Image* image, int quality);  // quality 0-100 for JPEG
void image_free(Image* image);

// Image manipulation
Image* image_resize(const Image* image, uint32_t new_width, uint32_t new_height);
Image* image_convert_format(const Image* image, uint32_t new_channels);
void image_flip_vertical(Image* image);
```

#### Model I/O

```c
typedef struct Model {
    // Mesh data
    Vertex* vertices;
    uint32_t vertex_count;
    uint32_t* indices;
    uint32_t index_count;
    
    // Submeshes (for multi-material models)
    struct {
        uint32_t index_offset;
        uint32_t index_count;
        uint32_t material_index;
    }* submeshes;
    uint32_t submesh_count;
    
    // Materials
    struct {
        char* name;
        vec4 base_color;
        float metallic;
        float roughness;
        char* albedo_texture_path;
        char* normal_texture_path;
        char* metallic_roughness_texture_path;
    }* materials;
    uint32_t material_count;
    
    // Skeleton (for animated models)
    struct {
        char* name;
        int parent_index;     // -1 for root
        mat4 inverse_bind_matrix;
    }* bones;
    uint32_t bone_count;
} Model;

Model* model_load(const char* path);
void model_free(Model* model);
```

#### JSON Parsing

```c
typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

typedef struct JsonValue JsonValue;

JsonValue* json_parse(const char* json_string);
JsonValue* json_parse_file(const char* path);
char* json_stringify(const JsonValue* value, bool pretty);
bool json_save_file(const char* path, const JsonValue* value, bool pretty);
void json_free(JsonValue* value);

// Type queries
JsonType json_get_type(const JsonValue* value);
bool json_is_null(const JsonValue* value);
bool json_is_bool(const JsonValue* value);
bool json_is_number(const JsonValue* value);
bool json_is_string(const JsonValue* value);
bool json_is_array(const JsonValue* value);
bool json_is_object(const JsonValue* value);

// Value accessors
bool json_get_bool(const JsonValue* value);
double json_get_number(const JsonValue* value);
const char* json_get_string(const JsonValue* value);

// Array operations
size_t json_array_size(const JsonValue* array);
JsonValue* json_array_get(const JsonValue* array, size_t index);

// Object operations
size_t json_object_size(const JsonValue* object);
JsonValue* json_object_get(const JsonValue* object, const char* key);
bool json_object_has(const JsonValue* object, const char* key);
const char* json_object_get_key(const JsonValue* object, size_t index);

// Builders
JsonValue* json_create_null(void);
JsonValue* json_create_bool(bool value);
JsonValue* json_create_number(double value);
JsonValue* json_create_string(const char* value);
JsonValue* json_create_array(void);
JsonValue* json_create_object(void);

void json_array_append(JsonValue* array, JsonValue* value);
void json_object_set(JsonValue* object, const char* key, JsonValue* value);
```

---

### 3.4 Math Module

**Purpose**: Provide all mathematical operations needed for 3D graphics and game logic.

#### Core Types

```c
typedef struct { float x, y; } vec2;
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y, z, w; } vec4;
typedef struct { float m[9]; } mat3;   // Column-major 3x3
typedef struct { float m[16]; } mat4;  // Column-major 4x4
typedef struct { float x, y, z, w; } quat;
```

#### Vector Operations

```c
// vec2
vec2 vec2_create(float x, float y);
vec2 vec2_add(vec2 a, vec2 b);
vec2 vec2_sub(vec2 a, vec2 b);
vec2 vec2_mul(vec2 v, float s);
vec2 vec2_div(vec2 v, float s);
float vec2_dot(vec2 a, vec2 b);
float vec2_length(vec2 v);
float vec2_length_squared(vec2 v);
vec2 vec2_normalize(vec2 v);
vec2 vec2_lerp(vec2 a, vec2 b, float t);
float vec2_distance(vec2 a, vec2 b);
float vec2_angle(vec2 a, vec2 b);

// vec3
vec3 vec3_create(float x, float y, float z);
vec3 vec3_add(vec3 a, vec3 b);
vec3 vec3_sub(vec3 a, vec3 b);
vec3 vec3_mul(vec3 v, float s);
vec3 vec3_div(vec3 v, float s);
float vec3_dot(vec3 a, vec3 b);
vec3 vec3_cross(vec3 a, vec3 b);
float vec3_length(vec3 v);
float vec3_length_squared(vec3 v);
vec3 vec3_normalize(vec3 v);
vec3 vec3_lerp(vec3 a, vec3 b, float t);
float vec3_distance(vec3 a, vec3 b);
float vec3_angle(vec3 a, vec3 b);
vec3 vec3_reflect(vec3 v, vec3 normal);
vec3 vec3_project(vec3 v, vec3 onto);

// vec4
vec4 vec4_create(float x, float y, float z, float w);
vec4 vec4_add(vec4 a, vec4 b);
vec4 vec4_sub(vec4 a, vec4 b);
vec4 vec4_mul(vec4 v, float s);
vec4 vec4_div(vec4 v, float s);
float vec4_dot(vec4 a, vec4 b);
float vec4_length(vec4 v);
vec4 vec4_normalize(vec4 v);
vec4 vec4_lerp(vec4 a, vec4 b, float t);
```

#### Matrix Operations

```c
// mat4 basic operations
mat4 mat4_identity(void);
mat4 mat4_multiply(mat4 a, mat4 b);
mat4 mat4_transpose(mat4 m);
mat4 mat4_inverse(mat4 m);
vec4 mat4_multiply_vec4(mat4 m, vec4 v);
vec3 mat4_multiply_vec3(mat4 m, vec3 v, float w);  // Transform point (w=1) or vector (w=0)

// Transformation matrices
mat4 mat4_translate(vec3 translation);
mat4 mat4_rotate(quat rotation);
mat4 mat4_rotate_axis(vec3 axis, float angle);
mat4 mat4_rotate_euler(float pitch, float yaw, float roll);
mat4 mat4_scale(vec3 scale);
mat4 mat4_compose(vec3 translation, quat rotation, vec3 scale);

// Decomposition
void mat4_decompose(mat4 m, vec3* translation, quat* rotation, vec3* scale);

// Camera matrices
mat4 mat4_perspective(float fov_radians, float aspect, float near, float far);
mat4 mat4_orthographic(float left, float right, float bottom, float top, float near, float far);
mat4 mat4_look_at(vec3 eye, vec3 center, vec3 up);

// Utility
mat4 mat4_frustum(float left, float right, float bottom, float top, float near, float far);
```

#### Quaternion Operations

```c
quat quat_identity(void);
quat quat_create(float x, float y, float z, float w);
quat quat_from_axis_angle(vec3 axis, float angle);
quat quat_from_euler(float pitch, float yaw, float roll);
quat quat_from_mat4(mat4 m);
quat quat_multiply(quat a, quat b);
quat quat_normalize(quat q);
quat quat_conjugate(quat q);
quat quat_inverse(quat q);
quat quat_slerp(quat a, quat b, float t);
quat quat_nlerp(quat a, quat b, float t);  // Faster but less accurate
float quat_dot(quat a, quat b);
float quat_length(quat q);
mat4 quat_to_mat4(quat q);
vec3 quat_rotate_vec3(quat q, vec3 v);
void quat_to_axis_angle(quat q, vec3* axis, float* angle);
void quat_to_euler(quat q, float* pitch, float* yaw, float* roll);
```

#### Utility Functions

```c
float math_radians(float degrees);
float math_degrees(float radians);
float math_clamp(float value, float min, float max);
float math_lerp(float a, float b, float t);
float math_min(float a, float b);
float math_max(float a, float b);
float math_abs(float x);
float math_sign(float x);
float math_floor(float x);
float math_ceil(float x);
float math_round(float x);
float math_sqrt(float x);
float math_pow(float base, float exponent);
float math_sin(float x);
float math_cos(float x);
float math_tan(float x);
float math_asin(float x);
float math_acos(float x);
float math_atan(float x);
float math_atan2(float y, float x);
```

#### Geometry Utilities

```c
// Bounding boxes
typedef struct {
    vec3 min;
    vec3 max;
} BoundingBox;

BoundingBox bbox_create(vec3 min, vec3 max);
BoundingBox bbox_from_points(const vec3* points, uint32_t count);
BoundingBox bbox_transform(BoundingBox bbox, mat4 transform);
bool bbox_contains_point(BoundingBox bbox, vec3 point);
bool bbox_intersects(BoundingBox a, BoundingBox b);
vec3 bbox_center(BoundingBox bbox);
vec3 bbox_extents(BoundingBox bbox);

// Rays
typedef struct {
    vec3 origin;
    vec3 direction;  // Should be normalized
} Ray;

Ray ray_create(vec3 origin, vec3 direction);
vec3 ray_at(Ray ray, float t);
bool ray_intersects_bbox(Ray ray, BoundingBox bbox, float* t_min, float* t_max);
bool ray_intersects_sphere(Ray ray, vec3 center, float radius, float* t);
bool ray_intersects_plane(Ray ray, vec3 plane_point, vec3 plane_normal, float* t);

// Planes
typedef struct {
    vec3 normal;
    float distance;
} Plane;

Plane plane_create(vec3 normal, float distance);
Plane plane_from_point_normal(vec3 point, vec3 normal);
float plane_distance_to_point(Plane plane, vec3 point);
vec3 plane_project_point(Plane plane, vec3 point);
```

---

### 3.5 Scripting Module (Lua)

**Purpose**: Embed Lua 5.4 for game logic scripting and rapid prototyping.

#### Core Types

```c
typedef struct ScriptEngine ScriptEngine;
typedef struct ScriptRef ScriptRef;  // Reference to a Lua value
```

#### Engine Management

```c
ScriptEngine* script_init(void);
void script_shutdown(ScriptEngine* engine);
void script_garbage_collect(ScriptEngine* engine);
```

#### Script Loading

```c
bool script_load_file(ScriptEngine* engine, const char* path);
bool script_load_string(ScriptEngine* engine, const char* code, const char* chunk_name);
bool script_reload_file(ScriptEngine* engine, const char* path);  // Hot reload
```

#### Function Calling

```c
// Simple calls
bool script_call_function(ScriptEngine* engine, const char* func_name);

// Calls with arguments and return values
typedef struct ScriptCallContext ScriptCallContext;
ScriptCallContext* script_begin_call(ScriptEngine* engine, const char* func_name);
void script_push_nil(ScriptCallContext* ctx);
void script_push_bool(ScriptCallContext* ctx, bool value);
void script_push_number(ScriptCallContext* ctx, double value);
void script_push_string(ScriptCallContext* ctx, const char* value);
void script_push_vec3(ScriptCallContext* ctx, vec3 value);
void script_push_node(ScriptCallContext* ctx, Node* node);
bool script_call(ScriptCallContext* ctx, int num_results);
bool script_get_bool(ScriptCallContext* ctx, int index);
double script_get_number(ScriptCallContext* ctx, int index);
const char* script_get_string(ScriptCallContext* ctx, int index);
vec3 script_get_vec3(ScriptCallContext* ctx, int index);
void script_end_call(ScriptCallContext* ctx);
```

#### C Function Registration

```c
typedef int (*ScriptCFunction)(ScriptEngine* engine);

void script_register_function(ScriptEngine* engine, const char* name, ScriptCFunction func);

// Within a ScriptCFunction, use these to interact with the Lua stack:
int script_get_arg_count(ScriptEngine* engine);
bool script_arg_is_nil(ScriptEngine* engine, int index);
bool script_arg_is_bool(ScriptEngine* engine, int index);
bool script_arg_is_number(ScriptEngine* engine, int index);
bool script_arg_is_string(ScriptEngine* engine, int index);
bool script_arg_get_bool(ScriptEngine* engine, int index);
double script_arg_get_number(ScriptEngine* engine, int index);
const char* script_arg_get_string(ScriptEngine* engine, int index);

void script_return_nil(ScriptEngine* engine);
void script_return_bool(ScriptEngine* engine, bool value);
void script_return_number(ScriptEngine* engine, double value);
void script_return_string(ScriptEngine* engine, const char* value);
```

#### Global Variables

```c
void script_set_global_nil(ScriptEngine* engine, const char* name);
void script_set_global_bool(ScriptEngine* engine, const char* name, bool value);
void script_set_global_number(ScriptEngine* engine, const char* name, double value);
void script_set_global_string(ScriptEngine* engine, const char* name, const char* value);

bool script_get_global_bool(ScriptEngine* engine, const char* name);
double script_get_global_number(ScriptEngine* engine, const char* name);
const char* script_get_global_string(ScriptEngine* engine, const char* name);
```

#### Engine API Exposure

The following C APIs should be exposed to Lua scripts:

**Node Management**:
- Create, destroy, find nodes
- Add/remove children
- Get/set transform (position, rotation, scale)
- Add/remove components

**Input Queries**:
- Keyboard state (is key down/pressed/released)
- Mouse state (position, button state)
- Gamepad state (button, axis)

**Math Operations**:
- Vector/matrix/quaternion operations
- All math utility functions

**Resource Loading**:
- Load models, textures, sounds
- Query resource status

**Time**:
- Delta time
- Total elapsed time
- Frame count

---

## 4. Scene System (ECS-Inspired)

### 4.1 Core Concepts

The scene system uses a **component-based architecture** where:
- **Nodes** form a hierarchical tree structure
- **Properties** are typed components attached to nodes
- The **Scene** maintains separate 2D and 3D root nodes
- **Single-pass traversal**: Walk the tree once, collecting all necessary data (transforms, renderables, lights)

### 4.2 Scene Structure

```c
typedef uint64_t NodeID;

typedef struct Scene {
    Node* root_3d;          // Root of 3D scene tree
    Node* root_2d;          // Root of 2D scene tree (UI, HUD)
    Camera* camera_3d;      // 3D perspective camera
    Camera* camera_2d;      // 2D orthographic camera
    
    // Scene metadata
    char* name;
    vec3 ambient_light;
    
    // Internal node lookup
    // (hash table: NodeID -> Node* for fast lookups)
} Scene;

Scene* scene_create(const char* name);
void scene_destroy(Scene* scene);
Node* scene_find_node(Scene* scene, NodeID id);
Node* scene_find_node_by_name(Scene* scene, const char* name);
```

### 4.3 Node Definition

```c
typedef struct Node {
    NodeID id;              // Unique identifier
    char* name;             // Optional debug name (can be NULL)
    
    // Hierarchy
    Node* parent;           // Parent node (NULL for root)
    Node** children;        // Dynamic array of children
    uint32_t child_count;
    
    // Components
    Property** properties;  // Dynamic array of properties
    uint32_t property_count;
} Node;

Node* node_create(NodeID id, const char* name);
void node_destroy(Node* node);
Node* node_clone(const Node* node);  // Deep copy
```

### 4.4 Property System

```c
typedef enum {
    PROPERTY_TRANSFORM,
    PROPERTY_RENDERABLE,
    PROPERTY_LIGHT,
    PROPERTY_SCRIPT,
    PROPERTY_CAMERA,
    PROPERTY_RIGID_BODY,
    PROPERTY_COLLIDER,
    PROPERTY_AUDIO_SOURCE,
    PROPERTY_PARTICLE_EMITTER,
    // Extensible...
} PropertyType;

typedef struct Property {
    PropertyType type;
    void* data;
} Property;
```

#### Transform Component

```c
typedef struct Transform {
    vec3 position;
    quat rotation;
    vec3 scale;
} Transform;

Transform* transform_create(vec3 position, quat rotation, vec3 scale);
void transform_destroy(Transform* transform);
mat4 transform_to_matrix(const Transform* transform);
Transform transform_interpolate(const Transform* a, const Transform* b, float t);
```

#### Renderable Component

```c
typedef struct Renderable {
    Mesh* mesh;
    Material* material;
    bool cast_shadows;
    bool receive_shadows;
    bool visible;
} Renderable;

Renderable* renderable_create(Mesh* mesh, Material* material);
void renderable_destroy(Renderable* renderable);
```

#### Light Component

```c
typedef enum {
    LIGHT_DIRECTIONAL,
    LIGHT_POINT,
    LIGHT_SPOT
} LightType;

typedef struct Light {
    LightType type;
    vec3 color;
    float intensity;
    bool cast_shadows;
    
    union {
        struct {
            vec3 direction;
        } directional;
        
        struct {
            float radius;
            float constant;
            float linear;
            float quadratic;
        } point;
        
        struct {
            vec3 direction;
            float radius;
            float inner_cone_angle;
            float outer_cone_angle;
        } spot;
    };
} Light;

Light* light_create_directional(vec3 direction, vec3 color, float intensity);
Light* light_create_point(float radius, vec3 color, float intensity);
Light* light_create_spot(vec3 direction, float radius, float angle, vec3 color, float intensity);
void light_destroy(Light* light);
```

#### Script Component

```c
typedef struct Script {
    char* script_path;
    ScriptEngine* engine;
    ScriptRef* lua_table;    // Reference to script's state table
    
    bool has_init;
    bool has_update;
    bool has_destroy;
} Script;

Script* script_create(ScriptEngine* engine, const char* path);
void script_destroy(Script* script);
void script_call_init(Script* script, Node* node);
void script_call_update(Script* script, Node* node, float delta_time);
void script_call_destroy(Script* script, Node* node);
```

#### Camera Component

```c
typedef enum {
    CAMERA_PERSPECTIVE,
    CAMERA_ORTHOGRAPHIC
} CameraType;

typedef struct Camera {
    CameraType type;
    
    union {
        struct {
            float fov;
            float aspect;
            float near;
            float far;
        } perspective;
        
        struct {
            float left;
            float right;
            float bottom;
            float top;
            float near;
            float far;
        } orthographic;
    };
    
    vec4 clear_color;
    bool clear_color_enabled;
    bool clear_depth_enabled;
} Camera;

Camera* camera_create_perspective(float fov, float aspect, float near, float far);
Camera* camera_create_orthographic(float left, float right, float bottom, float top, float near, float far);
void camera_destroy(Camera* camera);
mat4 camera_get_view_matrix(const Camera* camera, vec3 position, quat rotation);
mat4 camera_get_projection_matrix(const Camera* camera);
```

### 4.5 Node Operations

```c
// Hierarchy
void node_add_child(Node* parent, Node* child);
void node_remove_child(Node* parent, Node* child);
void node_set_parent(Node* child, Node* parent);
Node* node_get_child(Node* parent, uint32_t index);
Node* node_find_child(Node* parent, const char* name);

// Properties
void node_add_property(Node* node, PropertyType type, void* data);
void node_remove_property(Node* node, PropertyType type);
Property* node_get_property(Node* node, PropertyType type);
bool node_has_property(Node* node, PropertyType type);

// Convenience accessors
Transform* node_get_transform(Node* node);
Renderable* node_get_renderable(Node* node);
Light* node_get_light(Node* node);
Script* node_get_script(Node* node);
Camera* node_get_camera(Node* node);

// Transform helpers
void node_set_position(Node* node, vec3 position);
void node_set_rotation(Node* node, quat rotation);
void node_set_scale(Node* node, vec3 scale);
vec3 node_get_position(Node* node);
quat node_get_rotation(Node* node);
vec3 node_get_scale(Node* node);

// World-space queries
vec3 node_get_world_position(Node* node);
quat node_get_world_rotation(Node* node);
mat4 node_get_world_transform(Node* node);

// Utility
void node_set_enabled(Node* node, bool enabled);
bool node_is_enabled(Node* node);
```

### 4.6 Scene Serialization

```c
bool scene_save(const Scene* scene, const char* path);
Scene* scene_load(const char* path);

// Node serialization
JsonValue* node_to_json(const Node* node);
Node* node_from_json(const JsonValue* json);
```

---

## 5. Rendering Pipeline

### 5.1 Single-Pass Traversal

The rendering system walks the scene tree **once**, collecting all necessary data during traversal:
- World transforms for each node
- Renderable meshes with their materials
- Lights with their world positions/directions
- Any other data needed for rendering

This approach is efficient and avoids redundant tree traversals.

### 5.2 Render Context

```c
typedef struct RenderContext {
    // Draw calls
    struct DrawCall {
        Mesh* mesh;
        Material* material;
        mat4 model_matrix;
    }* draw_calls;
    uint32_t draw_call_count;
    
    // Lights
    struct LightData {
        Light* light;
        vec3 position;       // World position
        vec3 direction;      // World direction (for directional/spot)
    }* lights;
    uint32_t light_count;
    
    // Camera
    mat4 view;
    mat4 projection;
    
    // Culling data
    Plane frustum_planes[6];
} RenderContext;

RenderContext* render_context_create(void);
void render_context_destroy(RenderContext* ctx);
void render_context_clear(RenderContext* ctx);
void render_context_submit(GraphicsContext* gfx, RenderContext* ctx);
```

### 5.3 Scene Traversal

```c
typedef struct TraversalState {
    mat4 parent_transform;
    RenderContext* render_ctx;
} TraversalState;

void scene_traverse(Node* node, TraversalState* state);
```

The traversal function visits each node once, accumulating transforms and collecting components:
- Compute world transform by multiplying parent transform with local transform
- If node has a Renderable, add it to draw calls
- If node has a Light, add it to lights list
- Recurse to children with updated world transform

### 5.4 Frustum Culling

```c
typedef struct Frustum {
    Plane planes[6];  // Left, right, bottom, top, near, far
} Frustum;

Frustum frustum_create_from_matrix(mat4 view_projection);
bool frustum_contains_bbox(const Frustum* frustum, BoundingBox bbox);
bool frustum_contains_sphere(const Frustum* frustum, vec3 center, float radius);
```

During traversal, test each renderable's bounding box against the frustum before adding to draw list.

### 5.5 Rendering Order

1. **Begin Frame**: Acquire swapchain image, begin command buffer
2. **3D Scene**: 
   - Traverse 3D tree with perspective camera
   - Collect draw calls and lights
   - Sort draw calls (opaque front-to-back, transparent back-to-front)
   - Submit to GPU
3. **2D Scene**:
   - Traverse 2D tree with orthographic camera
   - Collect draw calls
   - Render on top of 3D scene (no depth test or with separate depth buffer)
4. **End Frame**: Submit commands, present

---

## 6. Build System

### 6.1 Makefile

```makefile
CC = gcc
CFLAGS = -std=gnu2x -Wall -Wextra -Wpedantic -O2 -g
INCLUDES = -Isrc -Iexternal/lua
LDFLAGS = -lm
LIBS = -lvulkan

# Platform detection
UNAME := $(shell uname -s)

ifeq ($(UNAME),Linux)
    CFLAGS += -DPLATFORM_LINUX
    ifdef USE_WAYLAND
        CFLAGS += -DUSE_WAYLAND
        LIBS += -lwayland-client
    else
        CFLAGS += -DUSE_X11
        LIBS += -lxcb -lxcb-keysyms
    endif
endif

ifeq ($(UNAME),Darwin)
    CFLAGS += -DPLATFORM_MACOS
    LDFLAGS += -framework Cocoa -framework QuartzCore -framework Metal
    LIBS += -L/usr/local/lib -lMoltenVK
endif

SRC_DIRS = src src/engine/core src/engine/platform src/engine/graphics \
           src/engine/parsers src/engine/math src/engine/scene src/engine/scripting

SRCS = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
OBJS = $(SRCS:.c=.o)

LUA_DIR = external/lua
LUA_LIB = $(LUA_DIR)/liblua.a

TARGET = game_engine

all: $(LUA_LIB) $(TARGET)

$(TARGET): $(OBJS) $(LUA_LIB)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(LUA_LIB):
	$(MAKE) -C $(LUA_DIR) generic

clean:
	rm -f $(OBJS) $(TARGET)
	$(MAKE) -C $(LUA_DIR) clean

.PHONY: all clean
```

### 6.2 Platform-Specific Builds

**Linux X11**: `make`  
**Linux Wayland**: `make USE_WAYLAND=1`  
**macOS**: `make` (requires Vulkan SDK with MoltenVK)  
**Android**: Use CMake with NDK toolchain

---

## 7. Dependencies Management

### 7.1 External Dependencies

| Dependency | Purpose | Integration |
|-----------|---------|-------------|
| **Lua 5.4** | Scripting runtime | Git submodule, statically linked |
| **Teal** | Typed Lua dialect compiler | Git submodule |
| **Vulkan SDK** | Graphics API | System-installed, dynamically linked |
| **Platform APIs** | Window/Input | OS-provided |

### 7.2 Git Submodules

```bash
git submodule add https://github.com/lua/lua.git external/lua
git submodule add https://github.com/teal-language/teal.git external/teal
git submodule update --init --recursive
```

### 7.3 Platform Setup

**Linux**:
```bash
sudo apt install vulkan-tools libvulkan-dev vulkan-validationlayers
sudo apt install libxcb1-dev libxcb-keysyms1-dev  # For X11
sudo apt install libwayland-dev                    # For Wayland
```

**macOS**:
```bash
brew install vulkan-loader molten-vk
```

**Android**: Vulkan supported on API level 24+

---

## 8. Future Considerations

### 8.1 Planned Features

- Physics integration
- Audio system (3D spatial audio)
- Animation system (skeletal, blending, IK)
- Particle systems
- PBR materials
- Shadow mapping
- Post-processing effects
- Scene editor
- Networking

### 8.2 Optimizations

- Frustum culling
- Occlusion culling
- LOD system
- Instanced rendering
- Multi-threaded command recording
- Texture streaming
- Memory pooling

---

This is a living document that will evolve as development progresses.
