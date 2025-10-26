
#include "vulkan_backend.hpp"
#include "logger.hpp"
#include "io.hpp"

#include <cstring>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/trigonometric.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <glm/gtc/matrix_transform.hpp>



template<typename T1, typename T2>
inline constexpr T1 loadPFN(T2 context, const char* name) {
    if constexpr (std::is_same_v<T2, VkDevice>) {
        return reinterpret_cast<T1>(vkGetDeviceProcAddr(context, name));
    } else {
        return reinterpret_cast<T1>(vkGetInstanceProcAddr(context, name));
    }
}

#define LOAD_PFN(func, ctx) loadPFN<PFN_##func>(ctx, #func);



void BackendVk::init(const WindowManager* windowManager, const GlobalServiceLocator* serviceLocator) {

    mpServiceLocator = serviceLocator;
    mpWindowManager = windowManager;
    Settings& settings = serviceLocator->get<Settings>();
    Logger& log = mpServiceLocator->get<Logger>();
    mMaxFramesInFlight = 3;

    log.info(LogStyle::Timestamp1) << "Trying Vulkan backend...\n";

    // max api version
    {
        auto vkEnumerateInstanceVersion = LOAD_PFN(vkEnumerateInstanceVersion, nullptr);
        if (vkEnumerateInstanceVersion) {
            TOF(vkEnumerateInstanceVersion(&mApiVer));
        } else {
            mApiVer = VK_API_VERSION_1_0;
        }
    }

    // instance
    {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.apiVersion = mApiVer;
        appInfo.pEngineName = "Tempor Engine";
        appInfo.pApplicationName = "";

        // layers
        std::vector<const char*> layers;

        if (settings.vulkanBackend_debug_useKhronosValidationLayer) {
            layers.push_back("VK_LAYER_KHRONOS_validation");
        }

        uint32_t layerCount;
        TOF(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));
        std::vector<VkLayerProperties> layerProps(layerCount);
        TOF(vkEnumerateInstanceLayerProperties(&layerCount, layerProps.data()));
        for (const auto& layer : layers) {
            bool supported = false;
            for (const auto& prop : layerProps) {
                if (std::strcmp(prop.layerName, layer) == 0) {
                    supported = true;
                    break;
                }
            }
            if (!supported) throw Exception(ErrCode::NoSupportError, "No support for crucial vulkan instance layer: "s + layer);
        }

        // extensions
        std::vector<const char*> extensions = windowManager->getExtensionsVk();
        extensions.push_back("VK_EXT_debug_utils");

        uint32_t extCount;
        TOF(vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr));
        std::vector<VkExtensionProperties> extProps(extCount);
        TOF(vkEnumerateInstanceExtensionProperties(nullptr, &extCount, extProps.data()));
        for (const auto& ext : extensions) {
            bool supported = false;
            for (const auto& prop : extProps) {
                if (std::strcmp(prop.extensionName, ext) == 0) {
                    supported = true;
                    break;
                }
            }
            if (!supported) throw Exception(ErrCode::NoSupportError, "No support for crucial vulkan instance extension: "s + ext);
        }

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = extensions.size();
        createInfo.ppEnabledExtensionNames = extensions.data();
        createInfo.enabledLayerCount = layers.size();
        createInfo.ppEnabledLayerNames = layers.data();

        TOF(vkCreateInstance(&createInfo, nullptr, &mInstance));

        log.debug() << "Created instance\n";
    }

    // debug utils messenger
    if (settings.vulkanBackend_debug_useKhronosValidationLayer) {
        auto vkCreateDebugUtilsMessengerEXT = LOAD_PFN(vkCreateDebugUtilsMessengerEXT, mInstance);
        if (vkCreateDebugUtilsMessengerEXT) {

            VkDebugUtilsMessengerCreateInfoEXT createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            createInfo.messageType = 
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            createInfo.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            createInfo.pfnUserCallback = [](
                VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
                const VkDebugUtilsMessengerCallbackDataEXT* callback, void* userData
            ) -> VkBool32 {

                BackendVk* This = reinterpret_cast<BackendVk*>(userData);

                if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                    This->mpServiceLocator->get<Logger>().warn() << callback->pMessage << "\n";
                } else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                    This->mpServiceLocator->get<Logger>().error(LogStyle::Error1) << callback->pMessage << "\n";
                } else {
                    This->mpServiceLocator->get<Logger>() << callback->pMessage << "\n";
                }

                if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                    return VK_TRUE;
                }
                return VK_FALSE;
            };
            createInfo.pUserData = this;

            vkCreateDebugUtilsMessengerEXT(mInstance, &createInfo, nullptr, &mDebugMessenger);
        }
        log.debug() << "Created debug utils messenger\n";
    }

    // physical device
    {
        uint32_t count;
        TOF(vkEnumeratePhysicalDevices(mInstance, &count, nullptr));
        std::vector<VkPhysicalDevice> physicalDevices(count);
        TOF(vkEnumeratePhysicalDevices(mInstance, &count, physicalDevices.data()));

        // TODO: add proper physical device test

        mPhysicalDevice = physicalDevices[0];

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(mPhysicalDevice, &props);

        log.debug() << "Picked physical device: " << props.deviceName << "\n";
    }

    // device
    {
        // TODO: add queue analysys

        float priority = 1.0f;

        std::vector<VkDeviceQueueCreateInfo> queues;
        VkDeviceQueueCreateInfo queue{};
        queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue.queueFamilyIndex = 0;
        queue.queueCount = 1;
        queue.pQueuePriorities = &priority;
        queues.push_back(queue);

        std::vector<const char*> extensions = {
            "VK_KHR_swapchain"
        };

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = queues.size();
        createInfo.pQueueCreateInfos = queues.data();
        createInfo.ppEnabledExtensionNames = extensions.data();
        createInfo.enabledExtensionCount = extensions.size();

        TOF(vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice));

        vkGetDeviceQueue(mDevice, 0, 0, &mRenderQueue);

        log.debug() << "Created device\n";
    }

    // surface related
    {
        mSurface = windowManager->createSurfaceVk(mInstance);

        mSwapchain.construct(mPhysicalDevice, mDevice, mSurface, windowManager);

        mFrames.resize(mMaxFramesInFlight);
        for (auto& frame : mFrames) {
            frame.construct(mDevice, 0);
        }

        log.debug() << "Created surface\n";
    }

    // render pass
    {

        VkAttachmentDescription attachments[1] = {};
        VkAttachmentDescription& swapchainAttachment = attachments[0];
        swapchainAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        swapchainAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        swapchainAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        swapchainAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        swapchainAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        swapchainAttachment.format = mSwapchain.format();

        VkSubpassDescription subpasses[1] = {};
        VkSubpassDescription& subpass = subpasses[0];
        VkAttachmentReference swapchainReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &swapchainReference;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        VkSubpassDependency dependencies[1] = {};
        VkSubpassDependency& dependency = dependencies[0];
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

        VkRenderPassCreateInfo renderPassCreateInfo{};
        renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCreateInfo.attachmentCount = std::size(attachments);
        renderPassCreateInfo.pAttachments = attachments;
        renderPassCreateInfo.subpassCount = std::size(subpasses);
        renderPassCreateInfo.pSubpasses = subpasses;
        renderPassCreateInfo.dependencyCount = std::size(dependencies);
        renderPassCreateInfo.pDependencies = dependencies;

        TOF(vkCreateRenderPass(mDevice, &renderPassCreateInfo, nullptr, &mMainRenderPass));

        mFramebuffers.construct(mSwapchain, mDevice, mMainRenderPass);

        log.debug() << "Created render pass\n";
    }

    // debug lines pipeline
    {

        VkPushConstantRange pushConst{};
        pushConst.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConst.offset = 0;
        pushConst.size = sizeof(DebugLinesPushConst);  

        VkPipelineLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutCreateInfo.pPushConstantRanges = &pushConst;
        layoutCreateInfo.pushConstantRangeCount = 1;
        
        TOF(vkCreatePipelineLayout(mDevice, &layoutCreateInfo, nullptr, &mDebugLinesPipelineLayout));

        VkPipelineColorBlendAttachmentState colourBlendAttach{};
        colourBlendAttach.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colourBlendAttach.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colourBlend{};
        colourBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colourBlend.attachmentCount = 1;
        colourBlend.pAttachments = &colourBlendAttach;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        
        VkDynamicState dynamics[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.pDynamicStates = dynamics;
        dynamicState.dynamicStateCount = std::size(dynamics);

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineShaderStageCreateInfo stages[2] = {};

        VkShaderModule fragShader;
        {
            std::vector<uint32_t> fragCode;
            {
                auto fragCodeBin = serviceLocator->get<IOManager>().readAll("shaders/debug_lines.frag.spv");
                fragCode.resize(fragCodeBin.size() / 4);
                std::memcpy(fragCode.data(), fragCodeBin.data(), fragCodeBin.size());
            }
            VkShaderModuleCreateInfo fragModuleCreateInfo{};
            fragModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            fragModuleCreateInfo.codeSize = fragCode.size() * sizeof(uint32_t);
            fragModuleCreateInfo.pCode = fragCode.data();
            TOF(vkCreateShaderModule(mDevice, &fragModuleCreateInfo, nullptr, &fragShader));
        }

        VkPipelineShaderStageCreateInfo& fragStage = stages[1];
        fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragShader;
        fragStage.pName = "main";

        VkShaderModule vertShader;
        {
            std::vector<uint32_t> vertCode;
            {
                auto vertCodeBin = serviceLocator->get<IOManager>().readAll("shaders/debug_lines.vert.spv");
                vertCode.resize(vertCodeBin.size() / 4);
                std::memcpy(vertCode.data(), vertCodeBin.data(), vertCodeBin.size());
            }
            VkShaderModuleCreateInfo vertModuleCreateInfo{};
            vertModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            vertModuleCreateInfo.codeSize = vertCode.size() * sizeof(uint32_t);
            vertModuleCreateInfo.pCode = vertCode.data();
            TOF(vkCreateShaderModule(mDevice, &vertModuleCreateInfo, nullptr, &vertShader));
        }

        VkPipelineShaderStageCreateInfo& vertStage = stages[0];
        vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStage.module = vertShader;
        vertStage.pName = "main";

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
        viewportState.pViewports = nullptr;
        viewportState.pScissors = nullptr;

        VkVertexInputBindingDescription vertexBinding = DebugLineVertexVk::getBindDesc();
        auto vertexAttribs = DebugLineVertexVk::getAttrDesc();

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.pVertexBindingDescriptions = &vertexBinding;
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexAttributeDescriptions = vertexAttribs.data();
        vertexInput.vertexAttributeDescriptionCount = vertexAttribs.size();      

        VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.layout = mDebugLinesPipelineLayout;
        pipelineCreateInfo.pColorBlendState = &colourBlend;
        pipelineCreateInfo.pRasterizationState = &raster;
        pipelineCreateInfo.pMultisampleState = &multisampling;
        pipelineCreateInfo.pDynamicState = &dynamicState;
        pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
        pipelineCreateInfo.renderPass = mMainRenderPass;
        pipelineCreateInfo.subpass = 0;
        pipelineCreateInfo.pStages = stages;
        pipelineCreateInfo.stageCount = std::size(stages);
        pipelineCreateInfo.pViewportState = &viewportState;
        pipelineCreateInfo.pVertexInputState = &vertexInput;

        TOF(vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &mDebugLinesPipeline));

        vkDestroyShaderModule(mDevice, fragShader, nullptr);
        vkDestroyShaderModule(mDevice, vertShader, nullptr);


        mDebugLinesBuffer.allocate(
            mDevice, mPhysicalDevice, sizeof(DebugLineVertexVk) * 200,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        );
        mDebugLinesBuffer.mapMemory();

        log.debug() << "Created debug lines pipeline\n";
    }

    log.info(LogStyle::Timestamp1) << "Vulkan backend initialization finished\n";

}



void BackendVk::destroy() noexcept {

    vkDeviceWaitIdle(mDevice);

    mDebugLinesBuffer.free();

    if (mDebugLinesPipeline) vkDestroyPipeline(mDevice, mDebugLinesPipeline, nullptr);

    if (mDebugLinesPipelineLayout) vkDestroyPipelineLayout(mDevice, mDebugLinesPipelineLayout, nullptr);

    mFramebuffers.destroy();

    if (mMainRenderPass) vkDestroyRenderPass(mDevice, mMainRenderPass, nullptr);

    for (auto& frame : mFrames) {
        frame.destroy();
    }

    mSwapchain.destroy();

    if (mSurface) vkDestroySurfaceKHR(mInstance, mSurface, nullptr);

    if (mDevice) vkDestroyDevice(mDevice, nullptr);

    if (mDebugMessenger) {
        auto vkDestroyDebugUtilsMessengerEXT = LOAD_PFN(vkDestroyDebugUtilsMessengerEXT, mInstance);
        if (vkDestroyDebugUtilsMessengerEXT) {
            vkDestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
        }
    }

    if (mInstance) vkDestroyInstance(mInstance, nullptr);

}



void BackendVk::beginRenderPass(const Scissor* pScissor, const Viewport* pViewport) {

    mFrameCounter = (mFrameCounter + 1) % mMaxFramesInFlight;

    Frame& frame = mFrames[mFrameCounter];

    TOF(vkWaitForFences(mDevice, 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX));

    TOF(vkResetCommandPool(mDevice, frame.commandPool, 0));

    VkResult result;

    if (mpWindowManager->getWidth() != mSwapchain.extent().width || mpWindowManager->getHeight() != mSwapchain.extent().height) {
        TOF(vkDeviceWaitIdle(mDevice));
        mSwapchain.construct(mPhysicalDevice, mDevice, mSurface, mpWindowManager);
        mFramebuffers.destroy();
        mFramebuffers.construct(mSwapchain, mDevice, mMainRenderPass);
    }

    result = vkAcquireNextImageKHR(mDevice, mSwapchain.swapchain(), UINT64_MAX, frame.imageAvailableSemaphore, VK_NULL_HANDLE, &mCurrentSwapchainImage);
    switch (result) {
        case VK_ERROR_OUT_OF_DATE_KHR:
        case VK_SUBOPTIMAL_KHR:
            TOF(vkDeviceWaitIdle(mDevice));
            mSwapchain.construct(mPhysicalDevice, mDevice, mSurface, mpWindowManager);
            mFramebuffers.destroy();
            mFramebuffers.construct(mSwapchain, mDevice, mMainRenderPass);
            return;
        case VK_SUCCESS: break;
        default: throw Exception(ErrCode::InternalError, "Failed to acquire swapchain image");
    }

    VkImage swapchainImage = mSwapchain.getImage(mCurrentSwapchainImage);

    VkCommandBufferBeginInfo commandBeginInfo{};
    commandBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    TOF(vkBeginCommandBuffer(frame.renderCommandBuffer(), &commandBeginInfo));

    VkClearValue clearValue{};
    clearValue.color = {{0.21f, 0.205f, 0.22f, 1.0f}};

    VkRenderPassBeginInfo renderBeginInfo{};
    renderBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderBeginInfo.renderArea.extent.width = mSwapchain.extent().width;
    renderBeginInfo.renderArea.extent.height = mSwapchain.extent().height;
    renderBeginInfo.renderArea.offset.x = 0;
    renderBeginInfo.renderArea.offset.y = 0;
    renderBeginInfo.renderPass = mMainRenderPass;
    renderBeginInfo.clearValueCount = 1;
    renderBeginInfo.pClearValues = &clearValue;
    renderBeginInfo.framebuffer = mFramebuffers.getFramebuffer(mCurrentSwapchainImage);

    vkCmdBeginRenderPass(frame.renderCommandBuffer(), &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);


    // scissor and viewport
    VkRect2D scissor;
    if (pScissor) {
        scissor.offset.x = pScissor->x;
        scissor.offset.y = pScissor->y;
        scissor.extent.width = pScissor->width;
        scissor.extent.height = pScissor->height;
    } else {
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = mSwapchain.extent().width;
        scissor.extent.height = mSwapchain.extent().height;
    }
    vkCmdSetScissor(frame.renderCommandBuffer(), 0, 1, &scissor);
    
    VkViewport viewport;
    if (pScissor) {
        viewport.x = pViewport->x;
        viewport.y = pViewport->y;
        viewport.width = pViewport->width;
        viewport.height = pViewport->height;
        viewport.minDepth = pViewport->minDepth;
        viewport.maxDepth = pViewport->maxDepth;
    } else {
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = mSwapchain.extent().width;
        viewport.height = mSwapchain.extent().height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
    }
    vkCmdSetViewport(frame.renderCommandBuffer(), 0, 1, &viewport);

}


void BackendVk::renderDebugLines(const std::vector<DebugLineVertex>& debugLinesVertices) {

    Frame& frame = mFrames[mFrameCounter];

    // debug lines
    mDebugLinesBuffer.resize(debugLinesVertices.size() * sizeof(DebugLineVertex));
    std::memcpy(mDebugLinesBuffer.map, debugLinesVertices.data(), debugLinesVertices.size() * sizeof(DebugLineVertex));

    vkCmdBindPipeline(frame.renderCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, mDebugLinesPipeline);

    VkDeviceSize debugLinesBufferOffset = 0;
    vkCmdBindVertexBuffers(frame.renderCommandBuffer(), 0, 1, &mDebugLinesBuffer.buffer, &debugLinesBufferOffset);

    glm::mat4 proj;
    float screenAspect = static_cast<float>(mSwapchain.extent().width) / static_cast<float>(mSwapchain.extent().height);
    float scale = 1.0f;
    // changing zNear and zFar order to make camera look in +Z direction
    proj = glm::ortho(-screenAspect * scale, screenAspect * scale, -scale, scale, 10000.0f, 0.01f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    DebugLinesPushConst push{};
    push.mvp = proj * view;
    vkCmdPushConstants(frame.renderCommandBuffer(), mDebugLinesPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DebugLinesPushConst), &push);

    vkCmdDraw(frame.renderCommandBuffer(), debugLinesVertices.size(), 1, 0, 0);

}


void BackendVk::endRenderPass() {

    VkResult result;

    Frame& frame = mFrames[mFrameCounter];

    vkCmdEndRenderPass(frame.renderCommandBuffer());

    TOF(vkEndCommandBuffer(frame.renderCommandBuffer()));

    TOF(vkResetFences(mDevice, 1, &frame.inFlightFence));

    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.renderCommandBuffer();
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frame.imageAvailableSemaphore;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &mSwapchain.getSemaphore(mCurrentSwapchainImage);
    submitInfo.pWaitDstStageMask = &waitStageMask;

    TOF(vkQueueSubmit(mRenderQueue, 1, &submitInfo, frame.inFlightFence));

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pImageIndices = &mCurrentSwapchainImage;
    presentInfo.pSwapchains = &mSwapchain.swapchain();
    presentInfo.swapchainCount = 1;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &mSwapchain.getSemaphore(mCurrentSwapchainImage);
    
    result = vkQueuePresentKHR(mRenderQueue, &presentInfo);
    switch (result) {
        case VK_ERROR_OUT_OF_DATE_KHR:
        case VK_SUBOPTIMAL_KHR:
            TOF(vkDeviceWaitIdle(mDevice));
            mSwapchain.construct(mPhysicalDevice, mDevice, mSurface, mpWindowManager);
            mFramebuffers.destroy();
            mFramebuffers.construct(mSwapchain, mDevice, mMainRenderPass);
            return;
        case VK_SUCCESS: break;
        default: throw Exception(ErrCode::InternalError, "Failed to present");
    }

}




void BackendVk::update() {

}





void Frame::construct(VkDevice device, uint32_t queueFamilyIndex) {

    mDevice = device;

    VkSemaphoreCreateInfo imageAvailableSemaphoreCreateInfo{};
    imageAvailableSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    TOF(vkCreateSemaphore(device, &imageAvailableSemaphoreCreateInfo, nullptr, &imageAvailableSemaphore));

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    TOF(vkCreateFence(device, &fenceCreateInfo, nullptr, &inFlightFence));

    VkCommandPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.queueFamilyIndex = queueFamilyIndex;
    TOF(vkCreateCommandPool(device, &poolCreateInfo, nullptr, &commandPool));

    VkCommandBufferAllocateInfo commandAllocInfo{};
    commandAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandAllocInfo.commandBufferCount = std::size(mCommandBuffers);
    commandAllocInfo.commandPool = commandPool;
    commandAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    TOF(vkAllocateCommandBuffers(device, &commandAllocInfo, mCommandBuffers));

}


void Frame::destroy() noexcept {

    if (commandPool) vkDestroyCommandPool(mDevice, commandPool, nullptr);

    if (inFlightFence) vkDestroyFence(mDevice, inFlightFence, nullptr);

    if (imageAvailableSemaphore) vkDestroySemaphore(mDevice, imageAvailableSemaphore, nullptr);

}
