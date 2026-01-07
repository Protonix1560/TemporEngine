
#include "renderer_vulkan.hpp"
#include "core.hpp"
#include "logger.hpp"
#include "plugin_core.h"
#include "vfs.hpp"
#include "window_manager.hpp"

#include <cstring>
#include <string>
#include <vector>
#include <memory>

#include <vulkan/vulkan_core.h>

#include <glm/gtc/matrix_transform.hpp>



// registring renderer
std::unique_ptr<IRenderer> registerRendererVk() {
    return std::make_unique<RendererVulkan>();
}
FactoryListRegistrar<IRenderer> registrar(registerRendererVk);



template<typename T1, typename T2>
inline constexpr T1 loadPFN(T2 context, const char* name) {
    if constexpr (std::is_same_v<T2, VkDevice>) {
        return reinterpret_cast<T1>(vkGetDeviceProcAddr(context, name));
    } else {
        return reinterpret_cast<T1>(vkGetInstanceProcAddr(context, name));
    }
}

#define LOAD_PFN(func, ctx) loadPFN<PFN_##func>(ctx, #func);



void RendererVulkan::init() {

    Settings& settings = gGetServiceLocator()->get<Settings>();
    Logger& log = gGetServiceLocator()->get<Logger>();
    mMaxFramesInFlight = 3;

    log.info(TPR_LOG_STYLE_STARTSTAMP1) << "Trying RendererVulkan...\n";

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

        if (settings.vulkanBackendDebugUseKhronosValidationLayer) {
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

        TprWindow tmpHandle;
        TprWindowCreateInfo tmpWindowCreateInfo{};
        tmpWindowCreateInfo.name = "tmp tempor window";
        tmpWindowCreateInfo.prefferedWidth = 0;
        tmpWindowCreateInfo.prefferedHeight = 0;
        tmpWindowCreateInfo.flags = TPR_CREATE_WINDOW_HIDDEN_FLAG_BIT;
        log.debug() << LOG_RENDERER_NAME ": Opening a hidden temporary window\n";
        if (gGetServiceLocator()->get<WindowManager>().openWindow(&tmpHandle, &tmpWindowCreateInfo) < 0) {
            throw Exception(ErrCode::InternalError, LOG_RENDERER_NAME ": Failed to open tmp window");
        }

        log.trace() << LOG_RENDERER_NAME ": Getting Vulkan Instance extension list\n";
        // extensions
        std::vector<const char*> extensions = gGetServiceLocator()->get<WindowManager>().getExtensionsVk(tmpHandle);
        mInstanceExtensions.clear();
        mInstanceExtensions.insert(mInstanceExtensions.end(), extensions.begin(), extensions.end());
        if (settings.vulkanBackendDebugUseKhronosValidationLayer) {
            extensions.push_back("VK_EXT_debug_utils");
        }

        gGetServiceLocator()->get<WindowManager>().closeWindow(tmpHandle);

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
            if (!supported) throw Exception(ErrCode::NoSupportError, LOG_RENDERER_NAME ": No support for crucial vulkan instance extension: "s + ext);
        }

        VkInstanceCreateInfo instanceCreateInfo{};
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pApplicationInfo = &appInfo;
        instanceCreateInfo.enabledExtensionCount = extensions.size();
        instanceCreateInfo.ppEnabledExtensionNames = extensions.data();
        instanceCreateInfo.enabledLayerCount = layers.size();
        instanceCreateInfo.ppEnabledLayerNames = layers.data();

        TOF(vkCreateInstance(&instanceCreateInfo, nullptr, &mInstance));

        log.debug() << LOG_RENDERER_NAME ": Created instance\n";
    }

    // debug utils messenger
    if (settings.vulkanBackendDebugUseKhronosValidationLayer) {
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

                RendererVulkan* This = reinterpret_cast<RendererVulkan*>(userData);

                if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                    gGetServiceLocator()->get<Logger>().warn(TPR_LOG_STYLE_WARN1) << callback->pMessage << "\n";
                } else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                    gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1) << callback->pMessage << "\n";
                } else {
                    gGetServiceLocator()->get<Logger>() << callback->pMessage << "\n";
                }

                if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                    return VK_TRUE;
                }
                return VK_FALSE;
            };
            createInfo.pUserData = this;

            vkCreateDebugUtilsMessengerEXT(mInstance, &createInfo, nullptr, &mDebugMessenger);
        }
        log.debug() << LOG_RENDERER_NAME ": Created debug utils messenger\n";
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

        log.debug() << LOG_RENDERER_NAME ": Picked physical device: " << props.deviceName << "\n";
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

        log.debug() << LOG_RENDERER_NAME ": Created device\n";
    }

    // buffers
    {
        mDebugLinesBuffer.allocate(
            mDevice, mPhysicalDevice, sizeof(DebugLineVertexVk) * 200,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        mDebugLinesBuffer.mapMemory();
        log.debug() << LOG_RENDERER_NAME ": Created debug lines buffer\n";

        mGUIVertexBuffer.allocate(
            mDevice, mPhysicalDevice, 64,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        mGUIVertexBuffer.mapMemory();
        log.debug() << LOG_RENDERER_NAME ": Created gui vertex buffer\n";

        mGUIIndexBuffer.allocate(
            mDevice, mPhysicalDevice, 64,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        mGUIIndexBuffer.mapMemory();
        log.debug() << LOG_RENDERER_NAME ": Created gui index buffer\n";
    }

    log.info(TPR_LOG_STYLE_ENDSTAMP1) << LOG_RENDERER_NAME ": Vulkan backend initialization finished\n";

}



void RenderPass::construct(VkDevice device, Swapchain& swapchain) {

    auto& logger = gGetServiceLocator()->get<Logger>();

    mDevice = device;

    // render pass
    {

        VkAttachmentDescription attachments[2] = {};
        VkAttachmentDescription& swapchainAttachment = attachments[0];
        swapchainAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        swapchainAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        swapchainAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        swapchainAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        swapchainAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        swapchainAttachment.format = swapchain.chainFormat();

        VkAttachmentDescription& depthAttachment = attachments[1];
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.format = swapchain.depthFormat();

        VkSubpassDescription subpasses[1] = {};
        VkSubpassDescription& subpass = subpasses[0];
        VkAttachmentReference swapchainReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthReference = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &swapchainReference;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.pDepthStencilAttachment = &depthReference;

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

        TOF(vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &mRenderPass));

        logger.debug() << LOG_RENDERER_NAME ": Created render pass\n";

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
        ROFile packetFrag = gGetServiceLocator()->get<VFSManager>().openRO("shaders/vulkan/debug_lines.frag.spv");
        ROSpan spanFrag = packetFrag.read(4);
        if (spanFrag.size() > UINT32_MAX) throw Exception(ErrCode::IOError, "Shader size is greater that 4GiB");
        VkShaderModuleCreateInfo fragModuleCreateInfo{};
        fragModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        fragModuleCreateInfo.codeSize = static_cast<uint32_t>(spanFrag.size());
        fragModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(spanFrag.data());
        TOF(vkCreateShaderModule(mDevice, &fragModuleCreateInfo, nullptr, &fragShader));
        
        VkPipelineShaderStageCreateInfo& fragStage = stages[1];
        fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragShader;
        fragStage.pName = "main";

        VkShaderModule vertShader;
        ROFile packetVert = gGetServiceLocator()->get<VFSManager>().openRO("shaders/vulkan/debug_lines.vert.spv");
        ROSpan spanVert = packetVert.read(4);
        if (spanVert.size() > UINT32_MAX) throw Exception(ErrCode::IOError, "Shader size is greater that 4GiB");
        VkShaderModuleCreateInfo vertModuleCreateInfo{};
        vertModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vertModuleCreateInfo.codeSize = static_cast<uint32_t>(spanVert.size());
        vertModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(spanVert.data());
        TOF(vkCreateShaderModule(mDevice, &vertModuleCreateInfo, nullptr, &vertShader));

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
        
        VkPipelineDepthStencilStateCreateInfo depthState{};
        depthState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthState.depthBoundsTestEnable = VK_FALSE;
        depthState.depthTestEnable = VK_FALSE;
        depthState.depthWriteEnable = VK_FALSE;
        depthState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depthState.stencilTestEnable = VK_FALSE;
        depthState.minDepthBounds = 0.0f;
        depthState.maxDepthBounds = 1.0f;

        VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.layout = mDebugLinesPipelineLayout;
        pipelineCreateInfo.pColorBlendState = &colourBlend;
        pipelineCreateInfo.pRasterizationState = &raster;
        pipelineCreateInfo.pMultisampleState = &multisampling;
        pipelineCreateInfo.pDynamicState = &dynamicState;
        pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
        pipelineCreateInfo.renderPass = mRenderPass;
        pipelineCreateInfo.subpass = 0;
        pipelineCreateInfo.pStages = stages;
        pipelineCreateInfo.stageCount = std::size(stages);
        pipelineCreateInfo.pViewportState = &viewportState;
        pipelineCreateInfo.pVertexInputState = &vertexInput;
        pipelineCreateInfo.pDepthStencilState = &depthState;

        TOF(vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &mDebugLinesPipeline));

        vkDestroyShaderModule(mDevice, fragShader, nullptr);
        vkDestroyShaderModule(mDevice, vertShader, nullptr);

        logger.debug() << LOG_RENDERER_NAME ": Created debug lines pipeline\n";
    }

    // gui pipeline
    {

        VkPushConstantRange pushConst{};
        pushConst.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConst.offset = 0;
        pushConst.size = sizeof(GUIPushConst);  

        VkPipelineLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutCreateInfo.pPushConstantRanges = &pushConst;
        layoutCreateInfo.pushConstantRangeCount = 1;
        
        TOF(vkCreatePipelineLayout(mDevice, &layoutCreateInfo, nullptr, &mGUIPipelineLayout));

        VkPipelineColorBlendAttachmentState colourBlendAttach{};
        colourBlendAttach.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colourBlendAttach.blendEnable = VK_TRUE;
        colourBlendAttach.srcColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
        colourBlendAttach.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colourBlendAttach.colorBlendOp = VK_BLEND_OP_ADD;
        colourBlendAttach.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colourBlendAttach.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colourBlendAttach.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colourBlend{};
        colourBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colourBlend.attachmentCount = 1;
        colourBlend.pAttachments = &colourBlendAttach;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
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
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineShaderStageCreateInfo stages[2] = {};

        VkShaderModule fragShader;
        ROFile packetFrag = gGetServiceLocator()->get<VFSManager>().openRO("shaders/vulkan/gui.frag.spv");
        ROSpan spanFrag = packetFrag.read(4);
        if (spanFrag.size() > UINT32_MAX) throw Exception(ErrCode::IOError, "Shader size is greater that 4GiB");
        VkShaderModuleCreateInfo fragModuleCreateInfo{};
        fragModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        fragModuleCreateInfo.codeSize = static_cast<uint32_t>(spanFrag.size());
        fragModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(spanFrag.data());
        TOF(vkCreateShaderModule(mDevice, &fragModuleCreateInfo, nullptr, &fragShader));
        
        VkPipelineShaderStageCreateInfo& fragStage = stages[1];
        fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragShader;
        fragStage.pName = "main";

        VkShaderModule vertShader;
        ROFile packetVert = gGetServiceLocator()->get<VFSManager>().openRO("shaders/vulkan/gui.vert.spv");
        ROSpan spanVert = packetVert.read(4);
        if (spanVert.size() > UINT32_MAX) throw Exception(ErrCode::IOError, "Shader size is greater that 4GiB");
        VkShaderModuleCreateInfo vertModuleCreateInfo{};
        vertModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vertModuleCreateInfo.codeSize = static_cast<uint32_t>(spanVert.size());
        vertModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(spanVert.data());
        TOF(vkCreateShaderModule(mDevice, &vertModuleCreateInfo, nullptr, &vertShader));

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

        VkVertexInputBindingDescription vertexBinding = GUIRectVertex::getBindDesc();
        auto vertexAttribs = GUIRectVertex::getAttrDesc();

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.pVertexBindingDescriptions = &vertexBinding;
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexAttributeDescriptions = vertexAttribs.data();
        vertexInput.vertexAttributeDescriptionCount = vertexAttribs.size(); 
        
        VkPipelineDepthStencilStateCreateInfo depthState{};
        depthState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthState.depthBoundsTestEnable = VK_FALSE;
        depthState.depthTestEnable = VK_TRUE;
        depthState.depthWriteEnable = VK_TRUE;
        depthState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depthState.stencilTestEnable = VK_FALSE;
        depthState.minDepthBounds = 0.0f;
        depthState.maxDepthBounds = 1.0f;     

        VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.layout = mGUIPipelineLayout;
        pipelineCreateInfo.pColorBlendState = &colourBlend;
        pipelineCreateInfo.pRasterizationState = &raster;
        pipelineCreateInfo.pMultisampleState = &multisampling;
        pipelineCreateInfo.pDynamicState = &dynamicState;
        pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
        pipelineCreateInfo.renderPass = mRenderPass;
        pipelineCreateInfo.subpass = 0;
        pipelineCreateInfo.pStages = stages;
        pipelineCreateInfo.stageCount = std::size(stages);
        pipelineCreateInfo.pViewportState = &viewportState;
        pipelineCreateInfo.pVertexInputState = &vertexInput;
        pipelineCreateInfo.pDepthStencilState = &depthState;

        TOF(vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &mGUIPipeline));

        vkDestroyShaderModule(mDevice, fragShader, nullptr);
        vkDestroyShaderModule(mDevice, vertShader, nullptr);

        logger.debug() << LOG_RENDERER_NAME ": Created GUI pipeline\n";
    }

}



void RenderPass::destroy() noexcept {
    if (mGUIPipeline) {
        vkDestroyPipeline(mDevice, mGUIPipeline, nullptr);
        mGUIPipeline = VK_NULL_HANDLE;
    }
    if (mGUIPipelineLayout) {
        vkDestroyPipelineLayout(mDevice, mGUIPipelineLayout, nullptr);
        mGUIPipelineLayout = VK_NULL_HANDLE;
    }
    if (mDebugLinesPipeline) {
        vkDestroyPipeline(mDevice, mDebugLinesPipeline, nullptr);
        mDebugLinesPipeline = VK_NULL_HANDLE;
    }
    if (mDebugLinesPipelineLayout) {
        vkDestroyPipelineLayout(mDevice, mDebugLinesPipelineLayout, nullptr);
        mDebugLinesPipelineLayout = VK_NULL_HANDLE;
    }
    if (mRenderPass) {
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
        mRenderPass = VK_NULL_HANDLE;
    }
}



TprResult RendererVulkan::registerWindow(TprWindow handle) noexcept {
    try {

        WindowContext& ctx = mWindowContexts.emplace(getBasicHandleIndex(handle), WindowContext{}).first->second;

        // checking if instance has all required extensions
        std::vector<const char*> requiredExtensions = gGetServiceLocator()->get<WindowManager>().getExtensionsVk(handle);
        for (const auto& reqExt : requiredExtensions) {
            for (const auto& preExt : mInstanceExtensions) {
                if (std::strcmp(reqExt, preExt) == 0) goto found_match;
            }
            // loop ended, didn't find a matching name
            return TPR_INSUFFICIENT_INIT;
            found_match: ;
        }

        ctx.surface = gGetServiceLocator()->get<WindowManager>().createSurfaceVk(handle, mInstance);
        ctx.frames.resize(mMaxFramesInFlight);
        for (auto& frame : ctx.frames) {
            frame.construct(mDevice, 0);
        }
        ctx.swapchain.construct(mPhysicalDevice, mDevice, ctx.surface, handle);
        for (const auto& [otherWindow, otherCtx] : mWindowContexts) {
            if (
                &otherCtx != &ctx && 
                otherCtx.swapchain.chainFormat() == ctx.swapchain.chainFormat() &&
                otherCtx.swapchain.depthFormat() == ctx.swapchain.depthFormat()
            ) {
                // can borrow the render pass from already existing window
                gGetServiceLocator()->get<Logger>().debug() << LOG_RENDERER_NAME ": Sharing already existing render pass\n";
                ctx.renderPass = otherCtx.renderPass;
                goto have_valid_render_pass;
            }
        }

        // need to create it's own
        // because no existing windows have the exact same formats choosed
        gGetServiceLocator()->get<Logger>().debug() << LOG_RENDERER_NAME ": Creating a new render pass\n";
        ctx.renderPass = std::make_shared<RenderPass>();
        ctx.renderPass->construct(mDevice, ctx.swapchain);

        have_valid_render_pass: ;

        ctx.framebuffers.construct(ctx.swapchain, mDevice, ctx.renderPass->mRenderPass);
        ctx.handle = handle;

    } catch (const Exception& e) {
        gGetServiceLocator()->get<Logger>().error(TPR_LOG_STYLE_ERROR1) << LOG_RENDERER_NAME ": Expected exception [" << e.code() << "]: " << e.what() << "\n";
        return TPR_UNKNOWN_ERROR;
    } catch (...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}


void RendererVulkan::unregisterWindow(TprWindow handle) noexcept {
    try {

        TOF(vkDeviceWaitIdle(mDevice));
        auto& ctx = mWindowContexts[getBasicHandleIndex(handle)];
        for (auto& frame : ctx.frames) {
            frame.destroy();
        }
        ctx.framebuffers.destroy();
        if (ctx.renderPass) {
            ctx.renderPass->destroy();
            ctx.renderPass.reset();
        }
        ctx.swapchain.destroy();
        if (ctx.surface) {
            vkDestroySurfaceKHR(mInstance, ctx.surface, nullptr);
            ctx.surface = VK_NULL_HANDLE;
        }

    } catch (...) {}
}


void RendererVulkan::shutdown() noexcept {

    vkDeviceWaitIdle(mDevice);

    mGUIIndexBuffer.free();
    mGUIVertexBuffer.free();
    mDebugLinesBuffer.free();

    for (auto& [index, ctx] : mWindowContexts) {
        unregisterWindow(ctx.handle);
    }

    if (mDevice) vkDestroyDevice(mDevice, nullptr);

    if (mDebugMessenger) {
        auto vkDestroyDebugUtilsMessengerEXT = LOAD_PFN(vkDestroyDebugUtilsMessengerEXT, mInstance);
        if (vkDestroyDebugUtilsMessengerEXT) {
            vkDestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
        }
    }

    if (mInstance) vkDestroyInstance(mInstance, nullptr);

}


void FramebuffersHolder::construct(Swapchain& swapchain, VkDevice device, VkRenderPass renderPass) {

    mDevice = device;
    auto& logger = gGetServiceLocator()->get<Logger>();

    mFramebuffers.assign(swapchain.imageCount(), VK_NULL_HANDLE);

    for (uint32_t i = 0; i < swapchain.imageCount(); i++) {

        VkImageView attachments[] = {
            swapchain.getChainImageView(i),
            swapchain.getDepthImageView(i)
        };

        VkFramebufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.width = swapchain.extent().width;
        createInfo.height = swapchain.extent().height;
        createInfo.attachmentCount = std::size(attachments);
        createInfo.pAttachments = attachments;
        createInfo.layers = 1;
        createInfo.renderPass = renderPass;

        TOF(vkCreateFramebuffer(device, &createInfo, nullptr, &mFramebuffers[i]));
    
    }

}


void FramebuffersHolder::destroy() noexcept {
    for (auto& framebuffer : mFramebuffers) {
        if (framebuffer) {
            vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
            framebuffer = VK_NULL_HANDLE;
        }
    }
}



// void RendererVulkan::renderDebugLines(const std::vector<DebugLineVertex>& debugLinesVertices, CameraProject cameraProject) {

//     if (debugLinesVertices.empty()) return;

//     Frame& frame = mFrames[mFrameCounter];

//     // debug lines
//     mDebugLinesBuffer.resize(debugLinesVertices.size() * sizeof(DebugLineVertex));
//     std::memcpy(mDebugLinesBuffer.map, debugLinesVertices.data(), debugLinesVertices.size() * sizeof(DebugLineVertex));

//     vkCmdBindPipeline(frame.renderCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, mDebugLinesPipeline);

//     VkDeviceSize debugLinesBufferOffset = 0;
//     vkCmdBindVertexBuffers(frame.renderCommandBuffer(), 0, 1, &mDebugLinesBuffer.buffer, &debugLinesBufferOffset);

//     glm::mat4 proj;
//     float screenAspect = static_cast<float>(mSwapchain.extent().width) / static_cast<float>(mSwapchain.extent().height);
//     if (cameraProject == CameraProject::Ortho) {
//         float scale = 1.0f;
//         // changing zNear and zFar order to make camera look in +Z direction
//         proj = glm::ortho(-screenAspect * scale, screenAspect * scale, -scale, scale, 10000.0f, 0.01f);
//     } else {
//         proj = glm::perspective(glm::radians(45.0f), screenAspect, 0.01f, 10000.0f);
//     }
//     glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
//     DebugLinesPushConst push{};
//     push.mvp = proj * view;
//     vkCmdPushConstants(frame.renderCommandBuffer(), mDebugLinesPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DebugLinesPushConst), &push);

//     vkCmdDraw(frame.renderCommandBuffer(), debugLinesVertices.size(), 1, 0, 0);

// }



// void RendererVulkan::renderGUI(const GUIDrawDesc& desc) {

//     Frame& frame = mFrames[mFrameCounter];

//     if (!desc.rects.empty()) {
//         mGUIVertexBuffer.resize(desc.rects.size() * sizeof(GUIRectVertex) * 4);
//         mGUIIndexBuffer.resize(desc.rects.size() * sizeof(uint32_t) * 6);
        
//         for (uint32_t i = 0; i < desc.rects.size(); i++) {
//             const GUIDrawDesc::Rect& rect = desc.rects[i];
//             GUIRectVertex v[4];
//             v[0].pos = glm::vec3(rect.x, rect.y, i + 1.0f);
//             v[0].colour = rect.bgColour;
//             v[1].pos = glm::vec3(rect.x + rect.w, rect.y, i + 1.0f);
//             v[1].colour = rect.bgColour;
//             v[2].pos = glm::vec3(rect.x + rect.w, rect.y + rect.h, i + 1.0f);
//             v[2].colour = rect.bgColour;
//             v[3].pos = glm::vec3(rect.x, rect.y + rect.h, i + 1.0f);
//             v[3].colour = rect.bgColour;
//             std::memcpy( static_cast<char*>(mGUIVertexBuffer.map) + i * sizeof(v), v, sizeof(v));

//             uint32_t indices[6] = {i * 4, i * 4 + 1, i * 4 + 2, i * 4, i * 4 + 2, i * 4 + 3};
//             std::memcpy(static_cast<char*>(mGUIIndexBuffer.map) + i * sizeof(indices), indices, sizeof(indices));
//         }
//     }

//     vkCmdBindPipeline(frame.renderCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, mGUIPipeline);

//     VkDeviceSize vertexOffset = 0;
//     vkCmdBindVertexBuffers(frame.renderCommandBuffer(), 0, 1, &mGUIVertexBuffer.buffer, &vertexOffset);

//     vkCmdBindIndexBuffer(frame.renderCommandBuffer(), mGUIIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

//     glm::mat4 proj;
//     float screenAspect = static_cast<float>(mSwapchain.extent().width) / static_cast<float>(mSwapchain.extent().height);
//     proj = glm::ortho(-screenAspect, screenAspect, -1.0f, 1.0f, 1000.0f, 0.01f);
//     glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
//     DebugLinesPushConst push{};
//     push.mvp = proj * view;
//     vkCmdPushConstants(frame.renderCommandBuffer(), mDebugLinesPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DebugLinesPushConst), &push);

//     vkCmdDrawIndexed(frame.renderCommandBuffer(), desc.rects.size() * 6, 1, 0, 0, 0);

// }



void RendererVulkan::render(const RenderGraph& graph) {

    mFrameCounter = (mFrameCounter + 1) % mMaxFramesInFlight;
    VkResult result;
    auto& logger = gGetServiceLocator()->get<Logger>();
    auto& windowManager = gGetServiceLocator()->get<WindowManager>();

    for (auto& [handle, conf] : graph.windows) {

        auto& ctx = mWindowContexts[getBasicHandleIndex(handle)];
        Frame& frame = ctx.frames[mFrameCounter];

        uint32_t swapchainImageIndex;
        VkImage swapchainImage;

        // render pass begin
        {

            TOF(vkWaitForFences(mDevice, 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX));

            TOF(vkResetCommandPool(mDevice, frame.commandPool, 0));

            // auto resizing the swapchain if size changed
            // Wayland sometimes doesn't invalidate the VkSurface even if it's size has changed so a manual recreation is nessesary
            TprBool8 resized;
            if (auto r = windowManager.hasWindowResized(handle, &resized) < 0) {
                throw Exception(ErrCode::InternalError, LOG_RENDERER_NAME ": WindowManager::hasWindowResized returned error code "s + std::to_string(r));
            }
            // if (resized) {
            //     TOF(vkDeviceWaitIdle(mDevice));
            //     ctx.swapchain.construct(mPhysicalDevice, mDevice, ctx.surface, handle);
            //     ctx.renderPass->mFramebuffers.destroy();
            //     ctx.renderPass->mFramebuffers.construct(ctx.swapchain, mDevice, ctx.renderPass->mRenderPass);
            // }

            // acquiring swapchain image
            result = vkAcquireNextImageKHR(mDevice, ctx.swapchain.swapchain(), UINT64_MAX, frame.imageAvailableSemaphore, VK_NULL_HANDLE, &swapchainImageIndex);
            switch (result) {
                case VK_ERROR_OUT_OF_DATE_KHR:
                case VK_SUBOPTIMAL_KHR:
                    TOF(vkDeviceWaitIdle(mDevice));
                    ctx.swapchain.construct(mPhysicalDevice, mDevice, ctx.surface, handle);
                    ctx.framebuffers.destroy();
                    ctx.framebuffers.construct(ctx.swapchain, mDevice, ctx.renderPass->mRenderPass);
                    return;
                case VK_SUCCESS: break;
                default: throw Exception(ErrCode::InternalError, "Failed to acquire swapchain image");
            }
            swapchainImage = ctx.swapchain.getChainImage(swapchainImageIndex);

            // render command buffer begin
            VkCommandBufferBeginInfo commandBeginInfo{};
            commandBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            commandBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            TOF(vkBeginCommandBuffer(frame.renderCommandBuffer(), &commandBeginInfo));

            // render pass begin
            VkClearValue chainClearValues[2];
            chainClearValues[0].color = {{0.21f, 0.205f, 0.22f, 1.0f}};
            chainClearValues[1].depthStencil = {1.0f, 0};
            VkRenderPassBeginInfo renderPassBeginInfo{};
            renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassBeginInfo.renderArea.extent.width = ctx.swapchain.extent().width;
            renderPassBeginInfo.renderArea.extent.height = ctx.swapchain.extent().height;
            renderPassBeginInfo.renderArea.offset.x = 0;
            renderPassBeginInfo.renderArea.offset.y = 0;
            renderPassBeginInfo.renderPass = ctx.renderPass->mRenderPass;
            renderPassBeginInfo.clearValueCount = std::size(chainClearValues);
            renderPassBeginInfo.pClearValues = chainClearValues;
            renderPassBeginInfo.framebuffer = ctx.framebuffers.getFramebuffer(swapchainImageIndex);
            vkCmdBeginRenderPass(frame.renderCommandBuffer(), &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            // scissor
            VkRect2D scissor;
            scissor.offset.x = conf.scissor.x;
            scissor.offset.y = conf.scissor.y;
            scissor.extent.width = conf.scissor.width;
            scissor.extent.height = conf.scissor.height;
            vkCmdSetScissor(frame.renderCommandBuffer(), 0, 1, &scissor);

            // viewport
            VkViewport viewport;
            viewport.x = conf.viewport.x;
            viewport.y = conf.viewport.y;
            viewport.width = conf.viewport.width;
            viewport.height = conf.viewport.height;
            viewport.minDepth = conf.viewport.minDepth;
            viewport.maxDepth = conf.viewport.maxDepth;
            vkCmdSetViewport(frame.renderCommandBuffer(), 0, 1, &viewport);

        }


        // render pass end
        {

            vkCmdEndRenderPass(frame.renderCommandBuffer());

            TOF(vkEndCommandBuffer(frame.renderCommandBuffer()));

            TOF(vkResetFences(mDevice, 1, &frame.inFlightFence));

            // submitting render queue
            VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &frame.renderCommandBuffer();
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &frame.imageAvailableSemaphore;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &ctx.swapchain.getSemaphore(swapchainImageIndex);
            submitInfo.pWaitDstStageMask = &waitStageMask;
            TOF(vkQueueSubmit(mRenderQueue, 1, &submitInfo, frame.inFlightFence));

            // submitting present queue
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.pImageIndices = &swapchainImageIndex;
            presentInfo.pSwapchains = &ctx.swapchain.swapchain();
            presentInfo.swapchainCount = 1;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &ctx.swapchain.getSemaphore(swapchainImageIndex);
            result = vkQueuePresentKHR(mRenderQueue, &presentInfo);
            switch (result) {
                case VK_ERROR_OUT_OF_DATE_KHR:
                case VK_SUBOPTIMAL_KHR:
                    TOF(vkDeviceWaitIdle(mDevice));
                    ctx.swapchain.construct(mPhysicalDevice, mDevice, ctx.surface, handle);
                    ctx.framebuffers.destroy();
                    ctx.framebuffers.construct(ctx.swapchain, mDevice, ctx.renderPass->mRenderPass);
                    return;
                case VK_SUCCESS: break;
                default: throw Exception(ErrCode::InternalError, "Failed to present");
            }
        }
    }

}



void RendererVulkan::update() {

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

    if (commandPool) {
        vkDestroyCommandPool(mDevice, commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
    }

    if (inFlightFence) {
        vkDestroyFence(mDevice, inFlightFence, nullptr);
        inFlightFence = VK_NULL_HANDLE;
    }

    if (imageAvailableSemaphore) {
        vkDestroySemaphore(mDevice, imageAvailableSemaphore, nullptr);
        imageAvailableSemaphore = VK_NULL_HANDLE;
    }

}
