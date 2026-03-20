
#include "hardware_layer.hpp"
#include "core.hpp"
#include "hardware_common_structs.hpp"
#include "hardware_layer_interface.hpp"
#include "logger.hpp"
#include "plugin_core.h"
#include "resource_registry.hpp"
#include "window_manager.hpp"

#include <cassert>
#include <cstring>
#include <exception>
#include <string>
#include <vector>
#include <memory>

#include <vulkan/vulkan.h>

#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_core.h>



// registring renderer
std::unique_ptr<HardwareLayer> registerLayerVulkan(
    Logger& rLogger, ResourceRegistry& rResReg, WindowManager& rWinMan, uint8_t engineVersionVariant,
    uint8_t engineVersionMajor, uint8_t engineVersionMinor, uint8_t engineVersionPatch
) {
    return std::make_unique<HardwareLayerVulkan>(rLogger, rResReg, rWinMan, engineVersionVariant, engineVersionMajor, engineVersionMinor, engineVersionPatch);
}
HardwareLayerManifest manifestVulkanHWL {
    GraphicsBackend::Vulkan,
    registerLayerVulkan,
    "Standart Vulkan HWL"
};
static_registry<HardwareLayerManifest, 0>::registrar registrar(manifestVulkanHWL);




template<typename T1, typename T2>
inline constexpr T1 loadPFN(T2 context, const char* name) {
    if constexpr (std::is_same_v<T2, VkDevice>) {
        return reinterpret_cast<T1>(vkGetDeviceProcAddr(context, name));
    } else {
        return reinterpret_cast<T1>(vkGetInstanceProcAddr(context, name));
    }
}

#define LOAD_PFN(func, ctx) loadPFN<PFN_##func>(ctx, #func);

#define SYM_LOAD_PFN(sym, func, ctx) sym.func = loadPFN<PFN_##func>(ctx, #func)



HardwareLayerVulkan::HardwareLayerVulkan(
    Logger& rLogger, ResourceRegistry& rResReg, WindowManager& rWinMan, uint8_t engineVersionVariant,
    uint8_t engineVersionMajor, uint8_t engineVersionMinor, uint8_t engineVersionPatch
) : mrLogger(rLogger), mrResReg(rResReg), mrWinMan(rWinMan)
{
    
    VkResult result;
    mMaxFramesInFlight = 3;

    SYM_LOAD_PFN(mSym, vkEnumerateInstanceVersion, nullptr);
    SYM_LOAD_PFN(mSym, vkEnumerateInstanceLayerProperties, nullptr);
    SYM_LOAD_PFN(mSym, vkEnumerateInstanceExtensionProperties, nullptr);
    SYM_LOAD_PFN(mSym, vkCreateInstance, nullptr);

    if (mSym.vkEnumerateInstanceVersion) {
        result = mSym.vkEnumerateInstanceVersion(&mApiVer);
        if (result != VK_SUCCESS) {
            mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "vkEnumerateInstanceVersion failed [" << result << "]\n";
            throw TPR_UNKNOWN_ERROR;
        }
    } else {
        mApiVer = VK_API_VERSION_1_0;
    }

    // instance
    {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.engineVersion = VK_MAKE_API_VERSION(engineVersionVariant, engineVersionMajor,engineVersionMinor, engineVersionPatch);
        appInfo.apiVersion = mApiVer;
        appInfo.pEngineName = "Tempor Engine";
        appInfo.pApplicationName = "";

        // layers
        std::vector<const char*> layers;

        // TODO: add settings registry or smth
        layers.push_back("VK_LAYER_KHRONOS_validation");

        uint32_t layerCount;
        result = mSym.vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        if (result != VK_SUCCESS) {
            mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "vkEnumerateInstanceVersion failed [" << result << "]\n";
            throw TPR_UNKNOWN_ERROR;
        }
        std::vector<VkLayerProperties> layerProps(layerCount);
        result = mSym.vkEnumerateInstanceLayerProperties(&layerCount, layerProps.data());
        if (result != VK_SUCCESS) {
            mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "vkEnumerateInstanceVersion failed [" << result << "]\n";
            throw TPR_UNKNOWN_ERROR;
        }
        for (const auto& layer : layers) {
            bool supported = false;
            for (const auto& prop : layerProps) {
                if (std::strcmp(prop.layerName, layer) == 0) {
                    supported = true;
                    break;
                }
            }
            if (!supported) {
                mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "No support for crucial vulkan instance layer: " << layer << "\n";
                throw TPR_NOT_SUPPORTED;
            }
        }

        TprWindow tmpWindow;
        TprWindowCreateInfo tmpWindowCreateInfo{};
        tmpWindowCreateInfo.name = "tmp tempor window";
        tmpWindowCreateInfo.prefferedWidth = 0;
        tmpWindowCreateInfo.prefferedHeight = 0;
        tmpWindowCreateInfo.flags = TPR_CREATE_WINDOW_HIDDEN_FLAG_BIT;
        mrLogger.trace() << logPrxPHWL() << "Opening a hidden temporary window\n";
        auto exp = mrWinMan.openWindow(&tmpWindowCreateInfo);
        if (!exp.has_value()) {
            mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "Failed to open temporary window\n";
            throw exp.error();
        }
        tmpWindow = exp.value();

        mrLogger.trace() << logPrxPHWL() << "Getting Vulkan Instance extension list\n";
        // extensions
        auto extExp = mrWinMan.getExtensionsVk(tmpWindow);
        if (!extExp.has_value()) throw extExp.error();
        std::vector<const char*> extensions = extExp.value();
        mInstanceExtensions.insert(mInstanceExtensions.end(), extensions.begin(), extensions.end());
        
        // TODO: settings registry!
        extensions.push_back("VK_EXT_debug_utils");

        mrWinMan.closeWindow(tmpWindow);

        uint32_t extCount;
        result = mSym.vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
        if (result != VK_SUCCESS) {
            mrLogger.error(TPR_LOG_STYLE_ERROR1) << "vkEnumerateInstanceExtensionProperties failed [" << result << "]\n";
            throw TPR_UNKNOWN_ERROR;
        }
        std::vector<VkExtensionProperties> extProps(extCount);
        result = mSym.vkEnumerateInstanceExtensionProperties(nullptr, &extCount, extProps.data());
        if (result != VK_SUCCESS) {
            mrLogger.error(TPR_LOG_STYLE_ERROR1) << "vkEnumerateInstanceExtensionProperties failed [" << result << "]\n";
            throw TPR_UNKNOWN_ERROR;
        }
        for (auto ext : extensions) {
            bool supported = false;
            for (const auto& prop : extProps) {
                if (std::strcmp(prop.extensionName, ext) == 0) {
                    supported = true;
                    break;
                }
            }
            if (!supported) {
                mrLogger.error(TPR_LOG_STYLE_ERROR1) << "No support for crucial vulkan instance extension: " << ext << "\n";
                throw TPR_NOT_SUPPORTED;
            }
        }

        VkInstanceCreateInfo instanceCreateInfo{};
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pApplicationInfo = &appInfo;
        instanceCreateInfo.enabledExtensionCount = extensions.size();
        instanceCreateInfo.ppEnabledExtensionNames = extensions.data();
        instanceCreateInfo.enabledLayerCount = layers.size();
        instanceCreateInfo.ppEnabledLayerNames = layers.data();

        TOF(vkCreateInstance(&instanceCreateInfo, nullptr, &mInstance));

        mrLogger.debug() << logPrxPHWL() << "Created instance\n";
    }

    SYM_LOAD_PFN(mSym, vkCreateDebugUtilsMessengerEXT, mInstance);
    SYM_LOAD_PFN(mSym, vkEnumeratePhysicalDevices, mInstance);
    SYM_LOAD_PFN(mSym, vkGetPhysicalDeviceProperties, mInstance);
    SYM_LOAD_PFN(mSym, vkCreateDevice, mInstance);
    SYM_LOAD_PFN(mSym, vkGetPhysicalDeviceMemoryProperties, mInstance);
    SYM_LOAD_PFN(mSym, vkDestroySurfaceKHR, mInstance);
    SYM_LOAD_PFN(mSym, vkGetPhysicalDeviceSurfaceFormatsKHR, mInstance);
    SYM_LOAD_PFN(mSym, vkGetPhysicalDeviceSurfacePresentModesKHR, mInstance);
    SYM_LOAD_PFN(mSym, vkGetPhysicalDeviceSurfaceCapabilitiesKHR, mInstance);

    // debug utils messenger
    // TODO: settings registry!
    {
        if (mSym.vkCreateDebugUtilsMessengerEXT) {
            mrLogger.debug() << logPrxPHWL() << "vkCreateDebugUtilsMessengerEXT is available\n";

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

                HardwareLayerVulkan* This = reinterpret_cast<HardwareLayerVulkan*>(userData);

                if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                    This->mrLogger.warn(TPR_LOG_STYLE_WARN1) << callback->pMessage << "\n";
                } else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                    This->mrLogger.error(TPR_LOG_STYLE_ERROR1) << callback->pMessage << "\n";
                } else {
                    This->mrLogger << callback->pMessage << "\n";
                }

                if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                    return VK_TRUE;
                }
                return VK_FALSE;
            };
            createInfo.pUserData = this;

            mSym.vkCreateDebugUtilsMessengerEXT(mInstance, &createInfo, nullptr, &mDebugMessenger);

            mrLogger.debug() << logPrxPHWL() << "Created debug utils messenger\n";

        } else {
            mrLogger.debug() << logPrxPHWL() << "vkCreateDebugUtilsMessengerEXT is not available\n";
        }
    }

    // physical device
    {
        uint32_t count;
        result = mSym.vkEnumeratePhysicalDevices(mInstance, &count, nullptr);
        if (result != VK_SUCCESS) {
            mrLogger.error(TPR_LOG_STYLE_ERROR1) << "vkEnumeratePhysicalDevices failed [" << result << "]";
            throw TPR_UNKNOWN_ERROR;
        }
        std::vector<VkPhysicalDevice> physicalDevices(count);
        result = mSym.vkEnumeratePhysicalDevices(mInstance, &count, physicalDevices.data());
        if (result != VK_SUCCESS) {
            mrLogger.error(TPR_LOG_STYLE_ERROR1) << "vkEnumeratePhysicalDevices failed [" << result << "]";
            throw TPR_UNKNOWN_ERROR;
        }

        // TODO: add proper physical device test

        mPhysicalDevice = physicalDevices[0];

        VkPhysicalDeviceProperties props;
        mSym.vkGetPhysicalDeviceProperties(mPhysicalDevice, &props);

        mrLogger.debug() << logPrxPHWL() << "Picked physical device " << props.deviceName << "\n";
    }

    // device
    {
        // TODO: add queue analysis

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

        result = mSym.vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice);
        if (result != VK_SUCCESS) {
            mrLogger.error(TPR_LOG_STYLE_ERROR1) << "vkCreateDevice failed [" << result << "]\n";
            throw result;
        }

        mrLogger.debug() << logPrxPHWL() << "Created device\n";
    }

    SYM_LOAD_PFN(mSym, vkCreateSemaphore, mDevice);
    SYM_LOAD_PFN(mSym, vkCreateFence, mDevice);
    SYM_LOAD_PFN(mSym, vkCreateCommandPool, mDevice);
    SYM_LOAD_PFN(mSym, vkAllocateCommandBuffers, mDevice);
    SYM_LOAD_PFN(mSym, vkGetDeviceQueue, mDevice);
    SYM_LOAD_PFN(mSym, vkCreateBuffer, mDevice);
    SYM_LOAD_PFN(mSym, vkGetBufferMemoryRequirements, mDevice);
    SYM_LOAD_PFN(mSym, vkAllocateMemory, mDevice);
    SYM_LOAD_PFN(mSym, vkBindBufferMemory, mDevice);
    SYM_LOAD_PFN(mSym, vkMapMemory, mDevice);
    SYM_LOAD_PFN(mSym, vkUnmapMemory, mDevice);
    SYM_LOAD_PFN(mSym, vkFreeMemory, mDevice);
    SYM_LOAD_PFN(mSym, vkDestroyBuffer, mDevice);
    SYM_LOAD_PFN(mSym, vkDestroyCommandPool, mDevice);
    SYM_LOAD_PFN(mSym, vkDestroyFence, mDevice);
    SYM_LOAD_PFN(mSym, vkDestroySemaphore, mDevice);
    SYM_LOAD_PFN(mSym, vkDestroySwapchainKHR, mDevice);
    SYM_LOAD_PFN(mSym, vkCreateSwapchainKHR, mDevice);
    SYM_LOAD_PFN(mSym, vkGetSwapchainImagesKHR, mDevice);
    SYM_LOAD_PFN(mSym, vkCreateImageView, mDevice);
    SYM_LOAD_PFN(mSym, vkGetImageMemoryRequirements, mDevice);
    SYM_LOAD_PFN(mSym, vkBindImageMemory, mDevice);
    SYM_LOAD_PFN(mSym, vkDestroyImageView, mDevice);
    SYM_LOAD_PFN(mSym, vkDestroyImage, mDevice);
    SYM_LOAD_PFN(mSym, vkCreateFramebuffer, mDevice);
    SYM_LOAD_PFN(mSym, vkDestroyFramebuffer, mDevice);

    mSym.vkGetDeviceQueue(mDevice, 0, 0, &mRenderQueue);

    // buffers
    {
        allocateBuffer(mDebugLinesBuffer, sizeof(DebugLineVertexVk) * 200,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        mapBufferMemory(mDebugLinesBuffer);
        mrLogger.debug() << logPrxPHWL() << "Created debug lines buffer\n";
    }

}


HardwareLayerVulkan::~HardwareLayerVulkan() noexcept {

    vkDeviceWaitIdle(mDevice);

    freeBuffer(mDebugLinesBuffer);

    for (auto& [index, ctx] : mWindowContexts) {
        unregisterWindow(ctx.windowHandle);
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



void RenderPass::construct(Logger& rLogger, ResourceRegistry& rResReg, VkDevice device, WindowContext& windowContext) {

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
        swapchainAttachment.format = windowContext.chainImageFormat;

        VkAttachmentDescription& depthAttachment = attachments[1];
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.format = windowContext.depthImageFormat;

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

        rLogger.debug() << logPrxPHWL() + "Created render pass\n";

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
        TprResource fragRes = rResReg.openResource("shaders/vulkan/debug_lines.frag.spv", 0, 4).value();
        if (rResReg.sizeofResource(fragRes).value() > UINT32_MAX) throw Exception(ErrCode::IOError, "Shader size is greater that 4GiB");
        VkShaderModuleCreateInfo fragModuleCreateInfo{};
        fragModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        fragModuleCreateInfo.codeSize = static_cast<uint32_t>(rResReg.sizeofResource(fragRes).value());
        fragModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(rResReg.getResourceConstPointer(fragRes).value());
        TOF(vkCreateShaderModule(mDevice, &fragModuleCreateInfo, nullptr, &fragShader));
        
        VkPipelineShaderStageCreateInfo& fragStage = stages[1];
        fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragShader;
        fragStage.pName = "main";

        VkShaderModule vertShader;
        TprResource vertRes = rResReg.openResource("shaders/vulkan/debug_lines.vert.spv", 0, 4).value();
        if (rResReg.sizeofResource(vertRes).value() > UINT32_MAX) throw Exception(ErrCode::IOError, "Shader size is greater that 4GiB");
        VkShaderModuleCreateInfo vertModuleCreateInfo{};
        vertModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vertModuleCreateInfo.codeSize = static_cast<uint32_t>(rResReg.sizeofResource(vertRes).value());
        vertModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(rResReg.getResourceConstPointer(vertRes).value());
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

        rLogger.debug() << logPrxPHWL() + "Created debug lines pipeline\n";
    }

}



void RenderPass::destroy() noexcept {
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



TprResult HardwareLayerVulkan::registerWindow(TprWindow handle) noexcept {

    TprResult result;

    WindowContext ctx = {};

    try {

        // checking if instance has all required extensions
        auto extExp = mrWinMan.getExtensionsVk(handle);
        if (!extExp.has_value()) return extExp.error();
        std::vector<const char*> requiredExtensions = extExp.value();
        for (const auto& reqExt : requiredExtensions) {
            for (const auto& preExt : mInstanceExtensions) {
                if (std::strcmp(reqExt, preExt) == 0) goto found_match;
            }
            // loop ended, didn't find a matching name
            return TPR_INSUFFICIENT_INIT;
            found_match: ;
        }

        result = constructWindowContext(ctx, handle);
        if (result < 0) return result;

        for (const auto& [otherWindow, otherCtx] : mWindowContexts) {
            if (
                &otherCtx != &ctx && 
                otherCtx.chainImageFormat == ctx.chainImageFormat &&
                otherCtx.depthImageFormat == ctx.depthImageFormat
            ) {
                // can borrow the render pass from already existing window
                mrLogger.debug() << logPrxPHWL() + "Sharing already existing render pass\n";
                ctx.renderPass = otherCtx.renderPass;
                goto have_valid_render_pass;
            }
        }

        // need to create it's own
        // because no existing windows have the exact same formats choosed
        mrLogger.debug() << logPrxPHWL() + "Creating a new render pass\n";
        ctx.renderPass = std::make_shared<RenderPass>();
        ctx.renderPass->construct(mrLogger, mrResReg, mDevice, ctx);

        have_valid_render_pass: ;

        ctx.windowHandle = handle;

    } catch (const Exception& e) {
        mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() + "Expected exception [" << e.code() << "]: " << e.what() << "\n";
        return TPR_UNKNOWN_ERROR;
    } catch (const std::exception& e) {
        mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() + "Unxpected exception: " << e.what() << "\n";
        return TPR_UNKNOWN_ERROR;
    } catch (...) {
        mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() + "Unknowm exception\n";
        return TPR_UNKNOWN_ERROR;
    }

    mWindowContexts.emplace(get_basic_handle_index(handle), ctx);

    return TPR_SUCCESS;
}


void HardwareLayerVulkan::unregisterWindow(TprWindow handle) noexcept {
    if (mInstance != VK_NULL_HANDLE) {
        try {

            TOF(vkDeviceWaitIdle(mDevice));
            auto& ctx = mWindowContexts[get_basic_handle_index(handle)];
            for (auto& frame : ctx.frames) {
                destroyFrame(frame);
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
}


void HardwareLayerVulkan::render(const RenderGraph& graph) {

    mFrameCounter = (mFrameCounter + 1) % mMaxFramesInFlight;
    VkResult result;

    for (auto& [handle, conf] : graph.windows) {

        auto& ctx = mWindowContexts[get_basic_handle_index(handle)];
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
            auto exp = mrWinMan.hasWindowResized(handle);
            if (!exp.has_value()) {
                throw Exception(ErrCode::InternalError, logPrxPHWL() + "WindowManager::hasWindowResized returned error code "s + std::to_string(exp.error()));
            }
            resized = exp.value();
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
                    mrLogger << "SWAPCHAIN ACQUIRED\n";
                    TOF(vkDeviceWaitIdle(mDevice));
                    ctx.swapchain.construct(&mrLogger, &mrWinMan, mPhysicalDevice, mDevice, ctx.surface, handle);
                    ctx.framebuffers.destroy();
                    ctx.framebuffers.construct(&mrLogger, ctx.swapchain, mDevice, ctx.renderPass->mRenderPass);
                    return;
                case VK_SUBOPTIMAL_KHR: break;
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
                    mrLogger << "SWAPCHAIN PRESENT\n";
                    TOF(vkDeviceWaitIdle(mDevice));
                    ctx.swapchain.construct(&mrLogger, &mrWinMan, mPhysicalDevice, mDevice, ctx.surface, handle);
                    ctx.framebuffers.destroy();
                    ctx.framebuffers.construct(&mrLogger, ctx.swapchain, mDevice, ctx.renderPass->mRenderPass);
                    return;
                case VK_SUCCESS: break;
                default: throw Exception(ErrCode::InternalError, "Failed to present");
            }
        }
    }

}



void HardwareLayerVulkan::update() {

}

