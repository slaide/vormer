#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <util.h>
#include <system.h>
#include <scene.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <xcb/xcb.h>
#include <xcb/xinput.h>

static xcb_atom_t xcb_get_atom(xcb_connection_t*con,const char*atom_name){
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(con, 0, strlen(atom_name), atom_name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(con, cookie, NULL);
    CHECK(reply!=nullptr,"failed to get atom for %s\n",atom_name);
    xcb_atom_t atom=reply->atom;
    free(reply);
    return atom;
}

// read file from filename into buffer, and write size read into filesize
static char*readfile(const char*filename,int *filesize){
    auto file=fopen(filename,"r");
    CHECK(file!=nullptr,"failed to open file %s\n",filename);
    fseek(file,0,SEEK_END);
    *filesize=ftell(file);
    fseek(file,0,SEEK_SET);
    printf("file %s has size %d\n",filename,*filesize);
    char*mem=malloc(*filesize);
    fread(mem,1,*filesize,file);
    fclose(file);
    return mem;
}

// https://docs.vulkan.org/refpages/latest/refpages/source/VkResult.html
static inline const char*string_from_VkResult(VkResult vkres){
    switch(vkres){
        case VK_SUCCESS:return "VK_SUCCESS";
        case VK_ERROR_COMPRESSION_EXHAUSTED_EXT:return "VK_ERROR_COMPRESSION_EXHAUSTED_EXT";
        case VK_ERROR_DEVICE_LOST:return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_MEMORY_MAP_FAILED:return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_INITIALIZATION_FAILED:return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_OUT_OF_HOST_MEMORY:return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_POOL_MEMORY:return "VK_ERROR_OUT_OF_POOL_MEMORY";
        case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
        case VK_ERROR_EXTENSION_NOT_PRESENT:return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT:return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        default:return "unknown";
    }
}
// https://docs.vulkan.org/refpages/latest/refpages/source/VkPresentModeKHR.html
static inline const char*string_from_VkPresentModeKHR(VkPresentModeKHR present_mode){
    switch(present_mode){
        case VK_PRESENT_MODE_IMMEDIATE_KHR:return "VK_PRESENT_MODE_IMMEDIATE_KHR";
        case VK_PRESENT_MODE_MAILBOX_KHR:return "VK_PRESENT_MODE_MAILBOX_KHR";
        case VK_PRESENT_MODE_FIFO_KHR:return "VK_PRESENT_MODE_FIFO_KHR";
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "VK_PRESENT_MODE_FIFO_RELAXED_KHR";

        default:return "unknown";
    }
}
// https://docs.vulkan.org/refpages/latest/refpages/source/VkFormat.html
static inline const char*string_from_VkFormat(VkFormat format){
    switch(format){
        case VK_FORMAT_UNDEFINED:return "VK_FORMAT_UNDEFINED";
        case VK_FORMAT_R4G4_UNORM_PACK8:return "VK_FORMAT_R4G4_UNORM_PACK8";

        case VK_FORMAT_R8G8B8A8_UNORM:return "VK_FORMAT_R8G8B8A8_UNORM";

        case VK_FORMAT_B8G8R8A8_UNORM:return "VK_FORMAT_B8G8R8A8_UNORM";

        case VK_FORMAT_B8G8R8A8_SRGB:return "VK_FORMAT_B8G8R8A8_SRGB";

        case VK_FORMAT_A4B4G4R4_UNORM_PACK16:return "VK_FORMAT_A4B4G4R4_UNORM_PACK16";
        case VK_FORMAT_A1B5G5R5_UNORM_PACK16:return "VK_FORMAT_A1B5G5R5_UNORM_PACK16";

        default:printf("unknown vkformat %d\n",format);return "unknown";
    }
}
static inline const char*string_from_VkColorSpaceKHR(VkColorSpaceKHR colorspace){
    switch(colorspace){
        case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:return "VK_COLOR_SPACE_SRGB_NONLINEAR_KHR";
        case VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT:return "VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT";
        case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT:return "VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT";
        case VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT:return "VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT";
        case VK_COLOR_SPACE_BT709_NONLINEAR_EXT:return "VK_COLOR_SPACE_BT709_NONLINEAR_EXT";
        case VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT:return "VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT";
        case VK_COLOR_SPACE_DISPLAY_NATIVE_AMD:return "VK_COLOR_SPACE_DISPLAY_NATIVE_AMD";
        default:return "unknown";
    }
}

/*

for future reference, a though experiment

struct RenderSubpass{
    struct RenderPass*render_pass;
    int subpass_index;
};
struct RenderPass{
    int num_subpasses;
    struct RenderSubpass*subpasses;
};
struct RenderSubpassCreateInfo{
    int _unused;
};
struct RenderPassCreateInfo{
    int num_subpasses;
    struct RenderSubpassCreateInfo*subpasses;
};
void RenderPass_create(struct RenderPassCreateInfo*info,struct RenderPass*render_pass){
    auto subpasses=(struct RenderSubpass*)calloc(info->num_subpasses,sizeof(struct RenderSubpass));

    *render_pass=(struct RenderPass){
        .num_subpasses=info->num_subpasses,
        .subpasses=subpasses
    };
}

struct GraphicsPipeline{
    struct RenderSubpass*render_subpass;
};
struct GraphicsPipelineCreateInfo{
    struct RenderSubpass*render_subpass;
};
void GraphicsPipeline_create(struct GraphicsPipelineCreateInfo*info,struct GraphicsPipeline*pipeline){
    *pipeline=(struct GraphicsPipeline){
        .render_subpass=info->render_subpass,
    };
}
 */

VkFence acquireImageFence=VK_NULL_HANDLE;
unsigned imageIndex;
unsigned queueFamily=-1;
void System_create(struct SystemCreateInfo*create_info,struct System*system){
    CHECK(create_info->initial_window_info!=nullptr,"no intial window create info supplied");

    // create system platform
    xcb_connection_t*con;
    if(1){
        con=xcb_connect(nullptr,nullptr);
        CHECK(con!=nullptr,"failed to xcb connect");
    }

    // create instance
    VkInstance instance;
    if(1){
        VkResult vkres;

        printf("---- instance begin ----\n");

        unsigned numInstaceLayers={};
        vkEnumerateInstanceLayerProperties(&numInstaceLayers,nullptr);
        VkLayerProperties *layerProperties=calloc(numInstaceLayers,sizeof(VkLayerProperties));
        vkEnumerateInstanceLayerProperties(&numInstaceLayers,layerProperties);
        for(int i=-1;i<(int)numInstaceLayers;i++){
            const char*layername=nullptr;
            if(i>=0){
                layername=layerProperties[i].layerName;

                printf("layer %d: %s\n",i,layername);
            }

            unsigned numInstanceExtensions={};
            vkEnumerateInstanceExtensionProperties(layername,&numInstanceExtensions,nullptr);
            VkExtensionProperties*extensionProperties=calloc(numInstanceExtensions,sizeof(VkExtensionProperties));
            vkEnumerateInstanceExtensionProperties(layername,&numInstanceExtensions,extensionProperties);
            for(unsigned j=0;j<numInstanceExtensions;j++){
                if(layername)printf("    ");
                printf("extension %d %s\n",j,extensionProperties[j].extensionName);
            }
            free(extensionProperties);
        }
        free(layerProperties);

        const char*instance_layers[]={
            "VK_LAYER_KHRONOS_validation"
        };
        const char*instance_extensions[]={
            "VK_KHR_surface",
            "VK_KHR_xcb_surface"
        };
        VkInstanceCreateInfo instance_create_info={
            .sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext=nullptr,
            .flags=0,
            .pApplicationInfo=nullptr,
            .enabledLayerCount=1,
            .ppEnabledLayerNames=instance_layers,
            .enabledExtensionCount=2,
            .ppEnabledExtensionNames=instance_extensions,
        };
        vkres=vkCreateInstance(&instance_create_info, nullptr, &instance);
        CHECK(vkres==VK_SUCCESS,"create instance failed\n");

        printf("---- instance end ----\n");
    }
    system->instance=instance;

    *system=(struct System){
        .interface=SYSTEM_INTERFACE_XCB,
        .xcb={
            .con=con,
            .num_open_windows=0,
            .windows=nullptr,
            .useXinput2=create_info->xcb_enableXinput2,
        },
    };

    struct Window window;
    Window_create(create_info->initial_window_info,&window);
    system->window=window;

    // create device
    VkDevice device;
    VkPhysicalDevice physical_device=VK_NULL_HANDLE;
    VkSurfaceKHR surface;
    VkQueue queue;
    if(1){
        VkResult vkres;

        unsigned numPhysicalDevices;
        vkEnumeratePhysicalDevices(instance,&numPhysicalDevices,nullptr);
        VkPhysicalDevice*physical_devices=calloc(numPhysicalDevices,sizeof(VkPhysicalDevice));
        vkEnumeratePhysicalDevices(instance,&numPhysicalDevices,physical_devices);
        for(int i=0;i<(int)numPhysicalDevices;i++){
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(physical_devices[i],&deviceProperties);
            printf("physical device %d %s\n",i,deviceProperties.deviceName);
            switch(deviceProperties.deviceType){
                case VK_PHYSICAL_DEVICE_TYPE_CPU:
                    printf("    VK_PHYSICAL_DEVICE_TYPE_CPU\n");
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                    physical_device=physical_devices[i];
                    printf("    VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU\n");
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                    physical_device=physical_devices[i];
                    printf("    VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU\n");
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                    physical_device=physical_devices[i];
                    printf("    VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU\n");
                    break;
                case VK_PHYSICAL_DEVICE_TYPE_OTHER:
                    printf("    VK_PHYSICAL_DEVICE_TYPE_OTHER\n");
                    break;
                default:
            }
        }
        free(physical_devices);

        CHECK(physical_device!=VK_NULL_HANDLE,"vulkan found no gpu\n");

        VkXcbSurfaceCreateInfoKHR surfaceCreateInfo={
            .sType=VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
            .pNext=nullptr,
            .flags=0,
            .connection=system->xcb.con,
            .window=system->xcb.windows[0]->id,
        };
        vkres=vkCreateXcbSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &surface);
        CHECK(vkres==VK_SUCCESS,"failed to create surface\n");

        unsigned numFamilies={};
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device,&numFamilies,nullptr);
        VkQueueFamilyProperties*queueFamilies=calloc(numFamilies,sizeof(VkQueueFamilyProperties));
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device,&numFamilies,queueFamilies);
        for(unsigned i=0;i<numFamilies;i++){
            printf("queue %d\n",i);
            printf("    n %d\n",queueFamilies[i].queueCount);
            bool
                supportsCompute=(queueFamilies[i].queueFlags&VK_QUEUE_COMPUTE_BIT)>0,
                supportsGraphics=(queueFamilies[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)>0,
                supportsTransfer=(queueFamilies[i].queueFlags&VK_QUEUE_TRANSFER_BIT)>0,
                supportsSparseBinding=(queueFamilies[i].queueFlags&VK_QUEUE_SPARSE_BINDING_BIT)>0;
            VkBool32
                supportsSurfacePresentation=false;
            
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device,i,surface,&supportsSurfacePresentation);

            printf("    compute %d\n", supportsCompute);
            printf("    graphics %d\n",supportsGraphics);
            printf("    transfer %d\n",supportsTransfer);
            printf("    sparse %d\n",  supportsSparseBinding);
            printf("    surface %d\n", supportsSurfacePresentation);

            if(supportsCompute && supportsGraphics && supportsTransfer && supportsSurfacePresentation)
                queueFamily=i;
        }
        free(queueFamilies);
        CHECK(queueFamily!=(unsigned)-1,"failed to find suitable queue family\n");

        unsigned numLayers={};
        vkEnumerateDeviceLayerProperties(physical_device, &numLayers, nullptr);
        VkLayerProperties*layerProperties=calloc(numLayers,sizeof(VkLayerProperties));
        vkEnumerateDeviceLayerProperties(physical_device, &numLayers, layerProperties);
        for(int i=-1;i<(int)numLayers;i++){
            const char*layerName=nullptr;
            if(i>=0){
                layerName=layerProperties[i].layerName;
                printf("layer %d %s\n",i,layerName);
            }

            unsigned numExtensions={};
            vkEnumerateDeviceExtensionProperties(physical_device, layerName, &numExtensions, nullptr);
            VkExtensionProperties*extensionProperties=calloc(numExtensions,sizeof(VkExtensionProperties));
            vkEnumerateDeviceExtensionProperties(physical_device, layerName, &numExtensions, extensionProperties);
            for(unsigned j=0;j<numExtensions;j++){
                if(layerName)printf("    ");
                printf("extension %d %s\n",j,extensionProperties[j].extensionName);
            }
            free(extensionProperties);
        }
        free(layerProperties);

        float queuePriorities[4]={1,1,1,1};
        VkDeviceQueueCreateInfo deviceQueueCreateInfos[1]={
            (VkDeviceQueueCreateInfo){
                .sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext=nullptr,
                .flags=0,
                .queueFamilyIndex=queueFamily,
                .queueCount=1,
                .pQueuePriorities=queuePriorities
            }
        };
        const char*deviceLayers[1]={
            "VK_LAYER_KHRONOS_validation"
        };
        const char*deviceExtensions[1]={
            "VK_KHR_swapchain"
        };
        VkDeviceCreateInfo device_create_info={
            .sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext=nullptr,
            .flags=0,
            .queueCreateInfoCount=1,
            .pQueueCreateInfos=deviceQueueCreateInfos,
            .enabledLayerCount=1,
            .ppEnabledLayerNames=deviceLayers,
            .enabledExtensionCount=1,
            .ppEnabledExtensionNames=deviceExtensions,
        };
        vkres=vkCreateDevice(physical_device,&device_create_info,nullptr,&device);
        CHECK(vkres==VK_SUCCESS,"create device failed\n");

        vkGetDeviceQueue(device, queueFamily, 0, &queue);
    }
    system->physical_device=physical_device;
    system->device=device;
    system->surface=surface;
    system->queue=queue;

    VkSwapchainKHR swapchain;
    VkFormat format;
    VkImage *swapchain_images;
    unsigned num_swapchain_images;
    if(1){
        VkResult vkres;

        VkSurfaceCapabilitiesKHR surfaceCapabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device,surface,&surfaceCapabilities);
        printf("surface caps:\n");
        printf(
            "    image count min %d max %d\n",
            surfaceCapabilities.minImageCount,
            surfaceCapabilities.maxImageCount
        );

        unsigned numSurfaceFormat={};
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device,surface,&numSurfaceFormat,nullptr);
        VkSurfaceFormatKHR *surface_formats=calloc(numSurfaceFormat,sizeof(VkSurfaceFormatKHR));
        vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device,surface,&numSurfaceFormat,surface_formats);
        printf("surface formats\n");
        for(int i=0;i<(int)numSurfaceFormat;i++){
            printf("    %d format %s colorspace %s\n",i,string_from_VkFormat(surface_formats[i].format),string_from_VkColorSpaceKHR(surface_formats[i].colorSpace));
        }
        format=surface_formats[0].format;
        VkColorSpaceKHR colorspace=surface_formats[0].colorSpace;
        free(surface_formats);

        unsigned numPresentModes={};
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device,surface,&numPresentModes,nullptr);
        VkPresentModeKHR *present_modes=calloc(numPresentModes,sizeof(VkPresentModeKHR));
        vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device,surface,&numPresentModes,present_modes);
        printf("present modes\n");
        for(int i=0;i<(int)numPresentModes;i++){
            printf("    %d %s\n",i,string_from_VkPresentModeKHR(present_modes[i]));
        }
        auto present_mode=present_modes[0];
        free(present_modes);

        VkSwapchainCreateInfoKHR create_swapchain_info={
            .sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext=nullptr,
            .flags=0,
            .surface=surface,
            .minImageCount=surfaceCapabilities.minImageCount,
            .imageFormat=format,
            .imageColorSpace=colorspace,
            .imageExtent=surfaceCapabilities.currentExtent,
            .imageArrayLayers=1,
            .imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .imageSharingMode=VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount=1,
            .pQueueFamilyIndices=&queueFamily,
            .preTransform=surfaceCapabilities.currentTransform,
            .compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode=present_mode,
            .clipped=VK_FALSE,
            .oldSwapchain=VK_NULL_HANDLE
        };
        vkres=vkCreateSwapchainKHR(device, &create_swapchain_info, nullptr, &swapchain);
        CHECK(vkres==VK_SUCCESS,"creating swapchain failed because %s\n",string_from_VkResult(vkres));

        vkGetSwapchainImagesKHR(device, swapchain, &num_swapchain_images, nullptr);
        swapchain_images=calloc(num_swapchain_images,sizeof(VkImage));
        vkGetSwapchainImagesKHR(device, swapchain, &num_swapchain_images, swapchain_images);
    }
    system->swapchain=swapchain;
    system->swapchain_format=format;
    system->swapchain_images=swapchain_images;
    system->swapchain_num_images=num_swapchain_images;

    system->swapchain_image_views=calloc(num_swapchain_images,sizeof(VkImageView));
    for(int i=0;i<system->swapchain_num_images;i++){
        VkResult vkres;

        VkImageViewCreateInfo swapchain_image_view_create_info={
            .sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext=nullptr,
            .flags=0,
            .image=system->swapchain_images[i],
            .viewType=VK_IMAGE_VIEW_TYPE_2D,
            .format=system->swapchain_format,
            .components={
                .r=VK_COMPONENT_SWIZZLE_IDENTITY,
                .g=VK_COMPONENT_SWIZZLE_IDENTITY,
                .b=VK_COMPONENT_SWIZZLE_IDENTITY,
                .a=VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange=(VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}
        };
        vkres=vkCreateImageView(system->device, &swapchain_image_view_create_info, nullptr, &system->swapchain_image_views[i]);
        CHECK(vkres==VK_SUCCESS,"failed to create image view\n");
    }

    // render pass
    VkRenderPass render_pass;
    if(1){
        VkResult vkres;

        VkAttachmentReference color_attachment_reference={
            .attachment=0,
            .layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };
        VkSubpassDescription render_subpass={
            .flags=0,
            .pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount=0,
            .pInputAttachments=nullptr,
            .colorAttachmentCount=1,
            .pColorAttachments=&color_attachment_reference,
            .pResolveAttachments=nullptr,
            .pDepthStencilAttachment=nullptr,
            .preserveAttachmentCount=0,
            .pPreserveAttachments=nullptr
        };
        VkAttachmentDescription color_attachment={
            .flags=0,
            .format=system->swapchain_format,
            .samples=VK_SAMPLE_COUNT_1_BIT,
            .loadOp=VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp=VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };
        VkRenderPassCreateInfo render_pass_create_info={
            .sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext=nullptr,
            .flags=0,
            .attachmentCount=1,
            .pAttachments=&color_attachment,
            .subpassCount=1,
            .pSubpasses=&render_subpass,
            .dependencyCount=0,
            .pDependencies=nullptr
        };
        vkres=vkCreateRenderPass(system->device, &render_pass_create_info, nullptr, &render_pass);
        CHECK(vkres==VK_SUCCESS,"failed to create renderpass\n");

        for(int i=0;i<system->swapchain_num_images;i++){
            VkFramebufferCreateInfo framebuffer_create_info={
                .sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .pNext=nullptr,
                .flags=0,
                .renderPass=render_pass,
                .attachmentCount=1,
                .pAttachments=&system->swapchain_image_views[i],
                .width=window.xcb->width,
                .height=window.xcb->height,
                .layers=1
            };
            vkres=vkCreateFramebuffer(system->device, &framebuffer_create_info, nullptr, &system->framebuffer[i]);
            CHECK(vkres==VK_SUCCESS,"failed to create framebuffer\n");
        }
    }
    system->render_pass=render_pass;

    // graphics pipeline
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;
    VkShaderModule vertex_shader_module,fragment_shader_module;
    if(1){
        VkResult vkres;

        VkShaderModuleCreateInfo shader_module_create_info={
            .sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext=nullptr,
            .flags=0,
            .codeSize=0,
            .pCode=nullptr
        };
        int filesize;

        shader_module_create_info.pCode=(unsigned*)readfile("resources/shader.frag.spv",&filesize);
        shader_module_create_info.codeSize=filesize;
        vkres=vkCreateShaderModule(system->device, &shader_module_create_info, nullptr, &fragment_shader_module);
        CHECK(vkres==VK_SUCCESS,"failed to create frag shader module\n");
        free((void*)shader_module_create_info.pCode);

        shader_module_create_info.pCode=(unsigned*)readfile("resources/shader.vert.spv",&filesize);
        shader_module_create_info.codeSize=filesize;
        vkres=vkCreateShaderModule(system->device, &shader_module_create_info, nullptr, &vertex_shader_module);
        CHECK(vkres==VK_SUCCESS,"failed to create vert shader module\n");
        free((void*)shader_module_create_info.pCode);

        VkPipelineShaderStageCreateInfo graphics_pipeline_stage[]={
            {
                .sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext=nullptr,
                .flags=0,
                .stage=VK_SHADER_STAGE_VERTEX_BIT,
                .module=vertex_shader_module,
                .pName="main",
                .pSpecializationInfo=nullptr
            },
            {
                .sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext=nullptr,
                .flags=0,
                .stage=VK_SHADER_STAGE_FRAGMENT_BIT,
                .module=fragment_shader_module,
                .pName="main",
                .pSpecializationInfo=nullptr
            }
        };
        VkPipelineVertexInputStateCreateInfo vertex_input_state={
            .sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext=nullptr,
            .flags=0,
            .vertexBindingDescriptionCount=0,
            .pVertexBindingDescriptions=nullptr,
            .vertexAttributeDescriptionCount=0,
            .pVertexAttributeDescriptions=nullptr
        };
        VkPipelineInputAssemblyStateCreateInfo input_assembly_state={
            .sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext=nullptr,
            .flags=0,
            .topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable=VK_FALSE
        };

        VkPipelineLayoutCreateInfo pipeline_layout_create_info={
            .sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext=nullptr,
            .flags=0,
            .setLayoutCount=0,
            .pSetLayouts=nullptr,
            .pushConstantRangeCount=0,
            .pPushConstantRanges=nullptr
        };
        vkres=vkCreatePipelineLayout(system->device, &pipeline_layout_create_info, nullptr, &pipeline_layout);
        CHECK(vkres==VK_SUCCESS,"failed to create pipeline layout\n");

        VkPipelineMultisampleStateCreateInfo multisample_state={
            .sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext=nullptr,
            .flags=0,
            .rasterizationSamples=VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable=VK_FALSE,
            .minSampleShading=1.0,
            .pSampleMask=nullptr,
            .alphaToCoverageEnable=VK_FALSE,
            .alphaToOneEnable=VK_FALSE
        };
        VkPipelineRasterizationStateCreateInfo rasterization_state={
            .sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext=nullptr,
            .flags=0,
            .depthClampEnable=VK_FALSE,
            .rasterizerDiscardEnable=VK_FALSE,
            .polygonMode=VK_POLYGON_MODE_FILL,
            .cullMode=VK_CULL_MODE_NONE,
            .frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasClamp=VK_FALSE,
            .depthBiasSlopeFactor=1.0,
            .lineWidth=1.0
        };
        VkPipelineViewportStateCreateInfo viewport_state={
            .sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext=nullptr,
            .flags=0,
            .viewportCount=1,
            .pViewports=&(VkViewport){.x=0,.y=0,.width=window.xcb->width,.height=window.xcb->height,.minDepth=0,.maxDepth=1.0},
            .scissorCount=1,
            .pScissors=&(VkRect2D){.offset={0,0},.extent={.height=window.xcb->height,.width=window.xcb->width}}
        };
        VkPipelineColorBlendAttachmentState color_blend_attachment={
            .blendEnable=VK_FALSE,
            // we dont blend, but we still have to write these components
            .colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT
        };
        VkPipelineColorBlendStateCreateInfo color_blend_state={
            .sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext=nullptr,
            .flags=0,
            .logicOpEnable=VK_FALSE,
            .logicOp=0,
            .attachmentCount=1,
            .pAttachments=&color_blend_attachment,
            .blendConstants={}
        };
        VkGraphicsPipelineCreateInfo graphics_pipeline_create_info={
            .sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext=nullptr,
            .flags=0,
            .stageCount=2,
            .pStages=graphics_pipeline_stage,
            .pVertexInputState=&vertex_input_state,
            .pInputAssemblyState=&input_assembly_state,
            .pTessellationState=nullptr,
            .pViewportState=&viewport_state,
            .pRasterizationState=&rasterization_state,
            .pMultisampleState=&multisample_state,
            .pDepthStencilState=nullptr,
            .pColorBlendState=&color_blend_state,
            .pDynamicState=nullptr,
            .layout=pipeline_layout,
            .renderPass=render_pass,
            .subpass=0,
            .basePipelineHandle=VK_NULL_HANDLE,
            .basePipelineIndex=0
        };
        vkres=vkCreateGraphicsPipelines(system->device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &pipeline);
        CHECK(vkres==VK_SUCCESS,"failed to create graphics pipeline\n");
    }
    system->fragment_shader=fragment_shader_module;
    system->vertex_shader=vertex_shader_module;
    system->pipeline_layout=pipeline_layout;
    system->pipeline=pipeline;

    VkSemaphoreCreateInfo semaphore_create_info={
        .sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext=nullptr,
        .flags=0
    };
    vkCreateSemaphore(device, &semaphore_create_info, nullptr, &system->acquireToClear);
    vkCreateSemaphore(device, &semaphore_create_info, nullptr, &system->clearToDraw);
    vkCreateSemaphore(device, &semaphore_create_info, nullptr, &system->drawToPresent);
    vkCreateSemaphore(device, &semaphore_create_info, nullptr, &system->presentToAcquire);
}
void System_destroy(struct System*system){
    vkDestroyFence(system->device, acquireImageFence, nullptr);
    vkDestroySemaphore(system->device,system->acquireToClear,nullptr);
    vkDestroySemaphore(system->device,system->clearToDraw,nullptr);
    vkDestroySemaphore(system->device,system->drawToPresent,nullptr);
    vkDestroySemaphore(system->device,system->presentToAcquire,nullptr);

    vkDestroyCommandPool(system->device, system->command_pool, nullptr);

    for(int i=0;i<system->swapchain_num_images;i++){
        vkDestroyFramebuffer(system->device, system->framebuffer[i], nullptr);
    }
    vkDestroyRenderPass(system->device, system->render_pass, nullptr);
    vkDestroyPipeline(system->device, system->pipeline, nullptr);
    vkDestroyPipelineLayout(system->device, system->pipeline_layout, nullptr);
    vkDestroyShaderModule(system->device, system->fragment_shader, nullptr);
    vkDestroyShaderModule(system->device, system->vertex_shader, nullptr);

    for(int i=0;i<system->swapchain_num_images;i++){
        vkDestroyImageView(system->device, system->swapchain_image_views[i], nullptr);
    }
    free(system->swapchain_image_views);

    vkDestroySwapchainKHR(system->device, system->swapchain, nullptr);

    vkDestroyDevice(system->device,nullptr);
    vkDestroyInstance(system->instance, nullptr);

    switch(system->interface){
        case SYSTEM_INTERFACE_XCB:
            xcb_disconnect(system->xcb.con);

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
static inline enum KEY xcbKey_to_vormerKey(int xcb_key){
    switch(xcb_key){
        case 9: return KEY_ESCAPE;

        case 22:return KEY_BACKSPACE;
        case 23:return KEY_TAB;

        case 36:return KEY_ENTER;
        case 37:return KEY_LCTRL;
        case 50:return KEY_LSHIFT;
        case 62:return KEY_RSHIFT;
        case 64:return KEY_LOPT;
        case 65:return KEY_SPACE;
        case 66:return KEY_CAPSLOCK;

        case 108:return KEY_ROPT;
        case 119:return KEY_DELETE;
        case 133:return KEY_LSUPER;
        case 134:return KEY_RSUPER;

        default: printf("unknown key %d\n",xcb_key);return KEY_UNKNOWN;
    }
}
// contents of image on acquisiton are ignored
static const int acquiredImage_accessMask=VK_ACCESS_NONE;
static const int acquiredImage_layout=VK_IMAGE_LAYOUT_UNDEFINED;
// prepare to cleear
static const int clearImage_accessMask=VK_ACCESS_MEMORY_WRITE_BIT;
static const int clearImage_layout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
// transition to be drawn on
static const int drawImage_accessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
static const int drawImage_layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
// transition to presentation
static const int presentImage_accessMask=VK_ACCESS_MEMORY_READ_BIT;
static const int presentImage_layout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
// this is used so often..
static const VkImageSubresourceRange color_subresource_range={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
// set these up here since they dont change and depend on each other
static VkImageMemoryBarrier
    image_barrier_acquireToClear={
        .sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext=nullptr,
        .srcAccessMask=acquiredImage_accessMask,
        .oldLayout=acquiredImage_layout,
        .dstAccessMask=clearImage_accessMask,
        .newLayout=clearImage_layout,
        .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
        .image=VK_NULL_HANDLE,// filled in dynamically
        .subresourceRange=color_subresource_range
    },
    image_barrier_clearToDraw={
        .sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext=nullptr,
        .srcAccessMask=clearImage_accessMask,
        .dstAccessMask=drawImage_accessMask,
        .oldLayout=clearImage_layout,
        .newLayout=drawImage_layout,
        .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
        .image=VK_NULL_HANDLE,
        .subresourceRange=color_subresource_range
    },
    image_barrier_drawToPresent={
        .sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext=nullptr,
        .srcAccessMask=drawImage_accessMask,
        .dstAccessMask=presentImage_accessMask,
        .oldLayout=drawImage_layout,
        .newLayout=presentImage_layout,
        .srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
        .image=VK_NULL_HANDLE,
        .subresourceRange=color_subresource_range
    };

static void System_drawNode(struct System*system,struct Node*node){
    if(!node)return;

    auto mesh=node_getMesh(node);
    auto material=node_getMaterial(node);

    if((mesh && material)){
        vkCmdBindPipeline(
            system->command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            system->pipeline
        );
        vkCmdDraw(
            system->command_buffer,
            3,
            1,
            0,
            0
        );
    }

    for(int i=0;i<node->num_children;i++){
        System_drawNode(system, node->children[i]);
    }
}

void System_stepFrame(struct System*system){
    // begin frame
    if(1){
        if(acquireImageFence==VK_NULL_HANDLE){
            VkFenceCreateInfo fence_create_info={
                .sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .pNext=nullptr,
                .flags=0
            };
            vkCreateFence(system->device, &fence_create_info, nullptr, &acquireImageFence);
        }
        if(system->command_pool==VK_NULL_HANDLE){
            VkCommandPoolCreateInfo command_pool_create_info={
                .sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext=nullptr,
                .flags=0,
                .queueFamilyIndex=queueFamily
            };
            vkCreateCommandPool(system->device, &command_pool_create_info, nullptr, &system->command_pool);
        }

        VkCommandBufferAllocateInfo command_buffer_allocate_info={
            .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext=nullptr,
            .commandPool=system->command_pool,
            .level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount=1
        };
        vkAllocateCommandBuffers(system->device, &command_buffer_allocate_info, &system->command_buffer);

        vkAcquireNextImageKHR(
            system->device, 
            system->swapchain, 
            UINT64_MAX, 
            0,//system->acquireToClear, 
            acquireImageFence, 
            &imageIndex
        );
        printf("acquired image %d\n",imageIndex);

        system->image_index=imageIndex;

        vkWaitForFences(system->device, 1, &acquireImageFence, VK_TRUE, UINT64_MAX);
        vkResetFences(system->device, 1, &acquireImageFence);

        VkCommandBufferBeginInfo command_buffer_begin_info={
            .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext=nullptr,
            .flags=0,
            .pInheritanceInfo=nullptr
        };
        vkBeginCommandBuffer(system->command_buffer, &command_buffer_begin_info);

        image_barrier_acquireToClear.image=system->swapchain_images[imageIndex];
        vkCmdPipelineBarrier(
            system->command_buffer, 
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 
            0, 
            0, 
            nullptr, 
            0, 
            nullptr, 
            1, 
            &image_barrier_acquireToClear
        );

        VkClearColorValue clear_color={
            .float32={1,0.3,0,1}
        };
        vkCmdClearColorImage(
            system->command_buffer, 
            system->swapchain_images[imageIndex], 
            image_barrier_acquireToClear.newLayout, 
            &clear_color, 
            1, 
            &color_subresource_range
        );

        image_barrier_clearToDraw.image=system->swapchain_images[imageIndex];
        vkCmdPipelineBarrier(
            system->command_buffer, 
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 
            0, 
            0, 
            nullptr, 
            0, 
            nullptr, 
            1, 
            &image_barrier_clearToDraw
        );
    }

        VkRenderPassBeginInfo render_pass_begin_info={
            .sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext=nullptr,
            .renderPass=system->render_pass,
            .framebuffer=system->framebuffer[system->image_index],
            .renderArea={
                .offset={0,0},
                .extent={
                    .height=system->window.xcb->height,
                    .width=system->window.xcb->width
                }
            },
            .clearValueCount=0,
            .pClearValues=nullptr
        };
        vkCmdBeginRenderPass(
            system->command_buffer,
            &render_pass_begin_info,
            VK_SUBPASS_CONTENTS_INLINE
        );

        System_drawNode(system,system->scene->root_2d);
        System_drawNode(system,system->scene->root_3d);

        vkCmdEndRenderPass(system->command_buffer);

    if(1){
        VkResult vkres;

        image_barrier_drawToPresent.image=system->swapchain_images[imageIndex];
        vkCmdPipelineBarrier(
            system->command_buffer, 
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 
            0, 
            0, 
            nullptr, 
            0, 
            nullptr, 
            1, 
            &image_barrier_drawToPresent
        );

        vkres=vkEndCommandBuffer(system->command_buffer);
        CHECK(vkres==VK_SUCCESS,"failed to end command buffer\n");

        VkSubmitInfo submit_info={
            .sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext=nullptr,
            .waitSemaphoreCount=0,
            .pWaitSemaphores=nullptr,
            .pWaitDstStageMask=0,
            .commandBufferCount=1,
            .pCommandBuffers=&system->command_buffer,
            .signalSemaphoreCount=0,
            .pSignalSemaphores=nullptr
        };
        vkres=vkQueueSubmit(system->queue, 1, &submit_info, VK_NULL_HANDLE);
        CHECK(vkres==VK_SUCCESS,"failed to submit queue\n");

        VkPresentInfoKHR present_info={
            .sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext=nullptr,
            .waitSemaphoreCount=0,
            .pWaitSemaphores=nullptr,
            .swapchainCount=1,
            .pSwapchains=&system->swapchain,
            .pImageIndices=&imageIndex,
            &vkres
        };
        vkres=vkQueuePresentKHR(system->queue, &present_info);
        CHECK(vkres==VK_SUCCESS,"failed to queue present\n");

        vkDeviceWaitIdle(system->device);
        vkFreeCommandBuffers(system->device, system->command_pool, 1, &system->command_buffer);
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

                *event=(struct Event){
                    .kind=EVENT_KIND_KEY_PRESS,
                    .key_press={
                        .key=xcbKey_to_vormerKey(xevent->detail),
                    },
                };
            }
            break;
        case XCB_KEY_RELEASE:
            {
                auto xevent=(xcb_key_release_event_t*)xcb_event;

                *event=(struct Event){
                    .kind=EVENT_KIND_KEY_RELEASE,
                    .key_release={
                        .key=xcbKey_to_vormerKey(xevent->detail),
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
                //printf("cursor entered the window\n");
            }
            break;
        case XCB_LEAVE_NOTIFY:
            {
                //printf("cursor left the window\n");
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
                    // printf("ignoring configure (2)\n");
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
            // _NET_WM_WINDOW_TYPE_NORMAL         <- centered, only "close" decoration, accepts input

            // _NET_WM_WINDOW_TYPE_DIALOG         <- centered, only "close" decoration
            // _NET_WM_WINDOW_TYPE_UTILITY        <- doesnt work
            // _NET_WM_WINDOW_TYPE_SPLASH         <- centered, no decoration, no input
            // _NET_WM_WINDOW_TYPE_TOOLBAR        <- doesnt work
            // _NET_WM_WINDOW_TYPE_MENU           <- offset, only "close" decoration
            // _NET_WM_WINDOW_TYPE_DROPDOWN_MENU  <- offset, no decoration
            // _NET_WM_WINDOW_TYPE_POPUP_MENU     <- offset, no decoration
            // _NET_WM_WINDOW_TYPE_TOOLTIP        <- centered, has decoration, accepts input
            // _NET_WM_WINDOW_TYPE_NOTIFICATION
            // _NET_WM_WINDOW_TYPE_COMBO
            // _NET_WM_WINDOW_TYPE_DND
            // _NET_WM_WINDOW_TYPE_DOCK
            const char*window_type="_NET_WM_WINDOW_TYPE_NORMAL";
            xcb_atom_t net_wm_window_type=xcb_get_atom(con,"_NET_WM_WINDOW_TYPE");
            xcb_atom_t net_wm_window_type_normal=xcb_get_atom(con,window_type);
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
                info->decoration,
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
