#include <bits/time.h>
#include <stdlib.h>
#include <stdio.h>
#include<string.h>

#include <time.h>
#include <util.h>
#include <system.h>
#include <time.h>
#include <vulkan/vulkan_core.h>

static inline void mysleep(float time_s){
    struct timespec requested_time={
        .tv_sec=(int)time_s,
        .tv_nsec=(int)((time_s-(float)((int)time_s))*1e9)
    };

    struct timespec start,end;
    clock_gettime(CLOCK_MONOTONIC,&start);

    nanosleep(&requested_time, nullptr);

    clock_gettime(CLOCK_MONOTONIC,&end);
    // printf("start %ld.%ld end %ld.%ld\n",start.tv_sec,start.tv_nsec,end.tv_sec,end.tv_nsec);
}

char*readfile(const char*filename,int *filesize){
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

int main(int argc,char**argv){
    discard argc;
    discard argv;

    struct System system;

    struct WindowCreateInfo window_create_info={
        .system=&system,
        
        .width=1280,
        .height=720,
        
        .title="window title",

        .decoration=false,
    };
    
    struct SystemCreateInfo system_create_info={
        .initial_window_info=&window_create_info
    };
    
    System_create(&system_create_info,&system);
    struct Window window=system.window;
    
    int running = 1;
    while(running){
        struct Event event;
        while((System_pollEvent(&system,&event),event.kind!=EVENT_KIND_NONE)){
            switch(event.kind){
                case EVENT_KIND_KEY_PRESS:
                    {
                        //printf("key press %d\n",event.key_press.key);
                        if(event.key_press.key==KEY_ESCAPE){
                            printf("pressed esc -> closing\n");
                            running=0;
                        }
                    }
                    break;
                case EVENT_KIND_KEY_RELEASE:
                    {
                        //printf("key release %d\n",event.key_release.key);
                    }
                    break;

                case EVENT_KIND_BUTTON_PRESS:
                    {
                        //printf("button press %d\n",event.button_press.button);
                    }
                    break;
                case EVENT_KIND_BUTTON_RELEASE:
                    {
                        //printf("button release %d\n",event.button_release.button);
                    }
                    break;

                case EVENT_KIND_POINTER_MOVE:
                    {
                        //printf("pointer moved to %f x %f y\n",event.pointer_move.x,event.pointer_move.y);
                    }
                    break;

                case EVENT_KIND_FOCUS_GAINED:
                    {
                        //printf("window gained focus\n");
                    }
                    break;
                case EVENT_KIND_FOCUS_LOST:
                    {
                        //printf("window lost focus\n");
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

        System_beginFrame(&system);

        // TODO render here

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
            .format=system.swapchain_format,
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
        VkRenderPass render_pass={};
        vkres=vkCreateRenderPass(system.device, &render_pass_create_info, nullptr, &render_pass);
        CHECK(vkres==VK_SUCCESS,"failed to create renderpass\n");
        VkImageView swapchain_image_view;
        VkImageViewCreateInfo swapchain_image_view_create_info={
            .sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext=nullptr,
            .flags=0,
            .image=system.swapchain_images[system.image_index],
            .viewType=VK_IMAGE_VIEW_TYPE_2D,
            .format=system.swapchain_format,
            .components={
                .r=VK_COMPONENT_SWIZZLE_IDENTITY,
                .g=VK_COMPONENT_SWIZZLE_IDENTITY,
                .b=VK_COMPONENT_SWIZZLE_IDENTITY,
                .a=VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange=(VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}
        };
        vkres=vkCreateImageView(system.device, &swapchain_image_view_create_info, nullptr, &swapchain_image_view);
        CHECK(vkres==VK_SUCCESS,"failed to create image view\n");
        VkFramebufferCreateInfo framebuffer_create_info={
            .sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext=nullptr,
            .flags=0,
            .renderPass=render_pass,
            .attachmentCount=1,
            .pAttachments=&swapchain_image_view,
            .width=window.xcb->width,
            .height=window.xcb->height,
            .layers=1
        };
        VkFramebuffer framebuffer={};
        vkres=vkCreateFramebuffer(system.device, &framebuffer_create_info, nullptr, &framebuffer);
        CHECK(vkres==VK_SUCCESS,"failed to create framebuffer\n");
        VkRenderPassBeginInfo render_pass_begin_info={
            .sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext=nullptr,
            .renderPass=render_pass,
            .framebuffer=framebuffer,
            .renderArea={
                .offset={},
                .extent={
                    .height=window.xcb->height,
                    .width=window.xcb->width
                }
            },
            .clearValueCount=0,
            .pClearValues=nullptr
        };
        vkCmdBeginRenderPass(
            system.command_buffer, 
            &render_pass_begin_info,
            VK_SUBPASS_CONTENTS_INLINE
        );
        VkShaderModule fragment_shader_module,vertex_shader_module;
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
        vkCreateShaderModule(system.device, &shader_module_create_info, nullptr, &fragment_shader_module);

        shader_module_create_info.pCode=(unsigned*)readfile("resources/shader.vert.spv",&filesize);
        shader_module_create_info.codeSize=filesize;
        vkCreateShaderModule(system.device, &shader_module_create_info, nullptr, &vertex_shader_module);

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
        VkPipelineLayout pipeline_layout={};
        vkres=vkCreatePipelineLayout(system.device, &pipeline_layout_create_info, nullptr, &pipeline_layout);
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
        VkPipeline pipeline={};
        vkres=vkCreateGraphicsPipelines(system.device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &pipeline);
        CHECK(vkres==VK_SUCCESS,"failed to create graphics pipeline\n");
        vkCmdBindPipeline(
            system.command_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipeline
        );
        vkCmdDraw(
            system.command_buffer,
            3,
            1,
            0,
            0
        );

        vkCmdEndRenderPass(system.command_buffer);
        
        System_endFrame(&system);

        mysleep(1./30);
    }

    Window_destroy(&window);

    System_destroy(&system);

    return EXIT_SUCCESS;
}
