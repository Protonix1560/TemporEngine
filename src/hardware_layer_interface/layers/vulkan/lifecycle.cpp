
#include "hardware_layer.hpp"
#include "core.hpp"
#include "hardware_common_structs.hpp"
#include "hardware_layer_interface.hpp"
#include "logger.hpp"
#include "plugin_core.h"
#include "resource_registry.hpp"
#include "settings.hpp"
#include "window_manager.hpp"
#include "plugin_core.h"
#include "scene_graph.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <exception>
#include <string>
#include <vector>
#include <memory>
#include <unordered_set>

#include <vulkan/vulkan.h>

#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_core.h>



// registring renderer
expected<PHardwareLayer, TprResult> registerLayerVulkan(
    Logger logger, ResourceRegistry& rResReg, WindowManager& rWinMan, Settings& rSettings, SceneGraph& rScGr, TprComponent componentRenderable,
    uint8_t engineVersionVariant, uint8_t engineVersionMajor, uint8_t engineVersionMinor, uint8_t engineVersionPatch
) {
    try {
        return std::unique_ptr<HardwareLayer>(std::make_unique<HardwareLayerVulkan>(
            logger, rResReg, rWinMan, rSettings, rScGr, componentRenderable, engineVersionVariant, engineVersionMajor, engineVersionMinor, engineVersionPatch
        ));
    } catch (TprResult r) {
        return unexpected(r);
    }
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
    } else if constexpr (std::is_same_v<T2, VkInstance> || std::is_same_v<T2, std::nullptr_t>) {
        return reinterpret_cast<T1>(vkGetInstanceProcAddr(context, name));
    } else {
        static_assert(dependent_false_v<T2>, "context must be either VkDevice, VkInstance or std::nullptr_t");
    }
}

#define LOAD_PFN(func, ctx) loadPFN<PFN_##func>(ctx, #func)

#define SYM_LOAD_PFN(sym, func, ctx) sym.func = loadPFN<PFN_##func>(ctx, #func)



HardwareLayerVulkan::HardwareLayerVulkan(
    Logger logger, ResourceRegistry& rResReg, WindowManager& rWinMan, Settings& rSettings, SceneGraph& rScGr, TprComponent componentRenderable,
    uint8_t engineVersionVariant, uint8_t engineVersionMajor, uint8_t engineVersionMinor, uint8_t engineVersionPatch
) : mLogger(logger), mrResReg(rResReg), mrWinMan(rWinMan), mrSettings(rSettings), mrScGr(rScGr), mComponentRenderable(componentRenderable)
{
    
    VkResult vkResult;

    auto inFlightFrames = mrSettings.createSettingIntegerOr("maxFramesInFlight", 3);
    if (inFlightFrames < 0) inFlightFrames = 3;
    if (inFlightFrames > UINT32_MAX) inFlightFrames = 3;
    mMaxFramesInFlight = inFlightFrames;

    SYM_LOAD_PFN(mSym, vkEnumerateInstanceVersion, VK_NULL_HANDLE);
    SYM_LOAD_PFN(mSym, vkEnumerateInstanceLayerProperties, VK_NULL_HANDLE);
    SYM_LOAD_PFN(mSym, vkEnumerateInstanceExtensionProperties, VK_NULL_HANDLE);
    SYM_LOAD_PFN(mSym, vkCreateInstance, VK_NULL_HANDLE);

    if (mSym.vkEnumerateInstanceVersion) {
        vkResult = mSym.vkEnumerateInstanceVersion(&mApiVer);
        if (vkResult != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkEnumerateInstanceVersion failed [" << vkResult << "]\n";
            throw TPR_UNKNOWN_ERROR;
        }
    } else {
        mApiVer = VK_API_VERSION_1_0;
    }

    bool createDebugMessenger = false;

    // instance
    {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
        appInfo.engineVersion = VK_MAKE_API_VERSION(engineVersionVariant, engineVersionMajor,engineVersionMinor, engineVersionPatch);
        appInfo.apiVersion = mApiVer;
        appInfo.pEngineName = "Tempor Engine";
        appInfo.pApplicationName = "Standart Vulkan HWL";

        // layers
        std::vector<const char*> layers;
        
        if (mrSettings.createSettingBoolOr("standartVulkanHWL.enableKhronosValidationLayer", false)) {
            layers.push_back("VK_LAYER_KHRONOS_validation");
        }

        uint32_t layerCount;
        vkResult = mSym.vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        if (vkResult != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkEnumerateInstanceVersion failed [" << vkResult << "]\n";
            throw TPR_UNKNOWN_ERROR;
        }
        std::vector<VkLayerProperties> layerProps(layerCount);
        vkResult = mSym.vkEnumerateInstanceLayerProperties(&layerCount, layerProps.data());
        if (vkResult != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkEnumerateInstanceVersion failed [" << vkResult << "]\n";
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
                mLogger.error(TPR_LOG_STYLE_ERROR1) << "No support for crucial vulkan instance layer: " << layer << "\n";
                throw TPR_NOT_SUPPORTED;
            }
        }

        TprWindow tmpWindow;
        TprWindowCreateInfo tmpWindowCreateInfo{};
        tmpWindowCreateInfo.name = "tmp tempor window";
        tmpWindowCreateInfo.prefferedWidth = 0;
        tmpWindowCreateInfo.prefferedHeight = 0;
        tmpWindowCreateInfo.flags = TPR_CREATE_WINDOW_HIDDEN_FLAG_BIT;
        mLogger.trace() << "Opening a hidden temporary window\n";
        auto tmpWindowExp = mrWinMan.openWindow(&tmpWindowCreateInfo);
        if (!tmpWindowExp.has_value()) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "Failed to open a temporary window\n";
            throw tmpWindowExp.error();
        }
        tmpWindow = tmpWindowExp.value();

        mLogger.trace() << "Getting Vulkan Instance extension list\n";
        // extensions
        auto extExp = mrWinMan.getExtensionsVk(tmpWindow);
        if (!extExp.has_value()) throw extExp.error();
        std::vector<const char*> extensions = extExp.value();
        if (mrSettings.createSettingBoolOr("standartVulkanHWL.enableDebugUtils", false)) {
            createDebugMessenger = true;
            extensions.push_back("VK_EXT_debug_utils");
        }
        mInstanceExtensions.insert(mInstanceExtensions.end(), extensions.begin(), extensions.end());
        mrWinMan.closeWindow(tmpWindow);

        uint32_t extCount;
        vkResult = mSym.vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
        if (vkResult != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkEnumerateInstanceExtensionProperties failed [" << vkResult << "]\n";
            throw TPR_UNKNOWN_ERROR;
        }
        std::vector<VkExtensionProperties> extProps(extCount);
        vkResult = mSym.vkEnumerateInstanceExtensionProperties(nullptr, &extCount, extProps.data());
        if (vkResult != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkEnumerateInstanceExtensionProperties failed [" << vkResult << "]\n";
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
                mLogger.error(TPR_LOG_STYLE_ERROR1) << "No support for crucial vulkan instance extension: " << ext << "\n";
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

        vkResult = mSym.vkCreateInstance(&instanceCreateInfo, nullptr, &mInstance);
        if (vkResult != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkCreateInstance failed [" << vkResult << "]\n";
            throw TPR_UNKNOWN_ERROR;
        }

        mLogger.debug() << "Created instance\n";
    }

    SYM_LOAD_PFN(mSym, vkEnumeratePhysicalDevices, mInstance);
    SYM_LOAD_PFN(mSym, vkGetPhysicalDeviceProperties, mInstance);
    SYM_LOAD_PFN(mSym, vkCreateDevice, mInstance);
    SYM_LOAD_PFN(mSym, vkGetPhysicalDeviceMemoryProperties, mInstance);
    SYM_LOAD_PFN(mSym, vkDestroySurfaceKHR, mInstance);
    SYM_LOAD_PFN(mSym, vkGetPhysicalDeviceSurfaceFormatsKHR, mInstance);
    SYM_LOAD_PFN(mSym, vkGetPhysicalDeviceSurfacePresentModesKHR, mInstance);
    SYM_LOAD_PFN(mSym, vkGetPhysicalDeviceSurfaceCapabilitiesKHR, mInstance);
    SYM_LOAD_PFN(mSym, vkDestroyDevice, mInstance);
    SYM_LOAD_PFN(mSym, vkDestroyInstance, mInstance);

    // debug utils messenger
    {
        if (createDebugMessenger) {
            SYM_LOAD_PFN(mSym, vkCreateDebugUtilsMessengerEXT, mInstance);
            SYM_LOAD_PFN(mSym, vkDestroyDebugUtilsMessengerEXT, mInstance);

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
                    This->mLogger.warn(TPR_LOG_STYLE_WARN1) << callback->pMessage << "\n";
                } else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                    This->mLogger.error(TPR_LOG_STYLE_ERROR1) << callback->pMessage << "\n";
                } else {
                    This->mLogger.info() << callback->pMessage << "\n";
                }

                if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                    return VK_TRUE;
                }
                return VK_FALSE;
            };
            createInfo.pUserData = this;

            mSym.vkCreateDebugUtilsMessengerEXT(mInstance, &createInfo, nullptr, &mDebugMessenger);

            mLogger.debug() << "Created debug utils messenger\n";
        }
    }

    // physical device
    {
        uint32_t count;
        vkResult = mSym.vkEnumeratePhysicalDevices(mInstance, &count, nullptr);
        if (vkResult != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkEnumeratePhysicalDevices failed [" << vkResult << "]";
            throw TPR_UNKNOWN_ERROR;
        }
        std::vector<VkPhysicalDevice> physicalDevices(count);
        vkResult = mSym.vkEnumeratePhysicalDevices(mInstance, &count, physicalDevices.data());
        if (vkResult != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkEnumeratePhysicalDevices failed [" << vkResult << "]";
            throw TPR_UNKNOWN_ERROR;
        }

        // TODO: add proper physical device test

        mPhysicalDevice = physicalDevices[0];

        VkPhysicalDeviceProperties props;
        mSym.vkGetPhysicalDeviceProperties(mPhysicalDevice, &props);

        mLogger.debug() << "Picked physical device " << props.deviceName << "\n";
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

        uint32_t extCount;
        vkResult = vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &extCount, nullptr);
        if (vkResult != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkEnumerateDeviceExtensionProperties failed [" << vkResult << "]\n";
            throw TPR_UNKNOWN_ERROR;
        }
        std::vector<VkExtensionProperties> props(extCount);
        vkResult = vkEnumerateDeviceExtensionProperties(mPhysicalDevice, nullptr, &extCount, props.data());
        if (vkResult != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkEnumerateDeviceExtensionProperties failed [" << vkResult << "]\n";
            throw TPR_UNKNOWN_ERROR;
        }

        for (const auto& ext : extensions) {
            bool found = false;
            for (const auto& prop : props) {
                if (std::strcmp(prop.extensionName, ext) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                mLogger.error(TPR_LOG_STYLE_ERROR1) << "No support for crucial vulkan device extension: " << ext << "\n";
                throw TPR_NOT_SUPPORTED;
            }
        }

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = queues.size();
        createInfo.pQueueCreateInfos = queues.data();
        createInfo.ppEnabledExtensionNames = extensions.data();
        createInfo.enabledExtensionCount = extensions.size();

        vkResult = mSym.vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice);
        if (vkResult != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkCreateDevice failed [" << vkResult << "]\n";
            throw TPR_UNKNOWN_ERROR;
        }

        mLogger.debug() << "Created device\n";
    }

    // Most vulkan symbols
    {
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
        SYM_LOAD_PFN(mSym, vkCreateImage, mDevice);
        SYM_LOAD_PFN(mSym, vkCreateRenderPass, mDevice);
        SYM_LOAD_PFN(mSym, vkCreatePipelineLayout, mDevice);
        SYM_LOAD_PFN(mSym, vkCreateShaderModule, mDevice);
        SYM_LOAD_PFN(mSym, vkCreateGraphicsPipelines, mDevice);
        SYM_LOAD_PFN(mSym, vkResetCommandBuffer, mDevice);
        SYM_LOAD_PFN(mSym, vkBeginCommandBuffer, mDevice);
        SYM_LOAD_PFN(mSym, vkEndCommandBuffer, mDevice);
        SYM_LOAD_PFN(mSym, vkWaitForFences, mDevice);
        SYM_LOAD_PFN(mSym, vkResetCommandPool, mDevice);
        SYM_LOAD_PFN(mSym, vkAcquireNextImageKHR, mDevice);
        SYM_LOAD_PFN(mSym, vkCmdBeginRenderPass, mDevice);
        SYM_LOAD_PFN(mSym, vkCmdSetScissor, mDevice);
        SYM_LOAD_PFN(mSym, vkCmdSetViewport, mDevice);
        SYM_LOAD_PFN(mSym, vkCmdEndRenderPass, mDevice);
        SYM_LOAD_PFN(mSym, vkResetFences, mDevice);
        SYM_LOAD_PFN(mSym, vkQueueSubmit, mDevice);
        SYM_LOAD_PFN(mSym, vkQueuePresentKHR, mDevice);
        SYM_LOAD_PFN(mSym, vkDeviceWaitIdle, mDevice);
        SYM_LOAD_PFN(mSym, vkCmdCopyBuffer, mDevice);
        SYM_LOAD_PFN(mSym, vkDestroyDevice, mDevice);
        SYM_LOAD_PFN(mSym, vkCreateDescriptorSetLayout, mDevice);
        SYM_LOAD_PFN(mSym, vkDestroyPipelineLayout, mDevice);
        SYM_LOAD_PFN(mSym, vkDestroyPipeline, mDevice);
        SYM_LOAD_PFN(mSym, vkDestroyRenderPass, mDevice);
        SYM_LOAD_PFN(mSym, vkDestroyDescriptorSetLayout, mDevice);
        SYM_LOAD_PFN(mSym, vkDestroyShaderModule, mDevice);
        SYM_LOAD_PFN(mSym, vkCreateDescriptorPool, mDevice);
        SYM_LOAD_PFN(mSym, vkDestroyDescriptorPool, mDevice);
        SYM_LOAD_PFN(mSym, vkAllocateDescriptorSets, mDevice);
        SYM_LOAD_PFN(mSym, vkUpdateDescriptorSets, mDevice);
        SYM_LOAD_PFN(mSym, vkCmdBindDescriptorSets, mDevice);
        SYM_LOAD_PFN(mSym, vkCmdBindPipeline, mDevice);
        SYM_LOAD_PFN(mSym, vkCmdDrawIndexed, mDevice);
        SYM_LOAD_PFN(mSym, vkCmdBindIndexBuffer, mDevice);
        SYM_LOAD_PFN(mSym, vkCmdBindVertexBuffers, mDevice);
    }

    // render queue
    mSym.vkGetDeviceQueue(mDevice, 0, 0, &mRenderQueue);

    // stuff
    {
        VkDescriptorSetLayoutBinding bindings[] = {
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
            },
            {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
            },
        };

        VkDescriptorSetLayoutCreateInfo setInfo{};
        setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        setInfo.bindingCount = std::size(bindings);
        setInfo.pBindings = bindings;
        vkResult = mSym.vkCreateDescriptorSetLayout(mDevice, &setInfo, nullptr, &mObjectDataSetLayout);
        if (vkResult != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1)
                << "vkCreateDescriptorSetLayout failed [" << vkResult << "]\n";
            throw TPR_UNKNOWN_ERROR;
        }

        VkPipelineLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutCreateInfo.pSetLayouts = &mObjectDataSetLayout;
        layoutCreateInfo.setLayoutCount = 1;
        vkResult = mSym.vkCreatePipelineLayout(mDevice, &layoutCreateInfo, nullptr, &mBasicPipelinelayout);
        if (vkResult != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1)
                << "vkCreatePipelineLayout at basic pipeline layout creation failed [" << vkResult << "]\n";
            throw TPR_UNKNOWN_ERROR;
        }
    }

    // allocator
    mAlloc.emplace(createAllocator());

    // Geometry-related stuff
    {
        auto size = mrSettings.createSettingIntegerOr("standartVulkanHWL.geometryBufferSize", 16777216);
        if (size <= 0) size = 16777216;
        if (size > UINT32_MAX) size = 16777216;
        mGeometryBufferSize = size;
    }

    // Object-related stuff
    {
        auto resExp = mrResReg.openResource(size_t{0}, 0, sizeof(TprComponentChunk));
        if (!resExp.has_value()) throw resExp.error();
        mRenderableChunkFetchResource = resExp.value();

        auto objBufferExp = createBuffer(
            0, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        );
        if (!objBufferExp.has_value()) throw objBufferExp.error();
        mObjectBuffer.emplace(std::move(objBufferExp.value()));

        auto indicesBufferExp = createBuffer(
            0, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        );
        if (!indicesBufferExp.has_value()) throw indicesBufferExp.error();
        mObjectIndicesBuffer.emplace(std::move(indicesBufferExp.value()));

        auto growth = mrSettings.createSettingDoubleOr("standartVulkanHWL.objectBufferGrowth", 1.5);
        if (growth < 1.0) growth = 1.5;
        mObjectBufferGrowth = growth;

        mObjectChunkSize = mrScGr.getComponentChunkMaxElementCount();

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = 0;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkResult = mSym.vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mCommandPool);
        if (vkResult != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkCreateCommandPool failed [" << vkResult << "]\n";
            throw TPR_UNKNOWN_ERROR;
        }

        VkCommandBufferAllocateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        bufferInfo.commandPool = mCommandPool;
        bufferInfo.commandBufferCount = 1;
        bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        vkResult = mSym.vkAllocateCommandBuffers(mDevice, &bufferInfo, &mImmidiateCopyCmdBuffer);
        if (vkResult != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkAllocateCommandBuffers failed [" << vkResult << "]\n";
            throw TPR_UNKNOWN_ERROR;
        }

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vkResult = mSym.vkCreateFence(mDevice, &fenceInfo, nullptr, &mImmidiateCopyFence);
        if (vkResult != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkCreateFence failed [" << vkResult << "]\n";
            throw TPR_UNKNOWN_ERROR;
        }
    }
}



TprResult HardwareLayerVulkan::update() {

    TprResult tprResult;
    VkResult vkResult;

    tprResult = mrScGr.getComponentChunkHandles(mComponentRenderable, mRenderableChunkFetchResource);
    if (tprResult != TPR_SUCCESS) return tprResult;
    auto chunkCountExp = mrResReg.sizeofResource(mRenderableChunkFetchResource);
    if (!chunkCountExp.has_value()) return chunkCountExp.error();
    auto chunkCount = chunkCountExp.value() / sizeof(TprComponentChunk);
    if (chunkCount > 0) {
        auto ptrExp = mrResReg.getResourceConstPointer(mRenderableChunkFetchResource);
        if (!ptrExp.has_value()) return ptrExp.error();
        auto ptr = reinterpret_cast<const TprComponentChunk*>(ptrExp.value());
        std::unordered_set<uint64_t> handles;
        handles.reserve(chunkCount);
        for (auto it = ptr; it < ptr + chunkCount; it++) {
            handles.insert(it->_d);
        }
        std::vector<TprComponentRenderable> copyBuffer(mObjectChunkSize);
        std::unordered_map<uint32_t, std::vector<uint32_t>> newObjectDataIndices;
        
        bool updateObjectIndices = false;
        if (mObjectBuffer->size() < handles.size() * sizeof(ObjectData)) {
            // need to migrate old data to new buffer
            // and update the out-of-date data simultaneously
            updateObjectIndices = true;

            size_t newSize = std::ceil(handles.size() * mObjectBufferGrowth);
            auto newBufferExp = createBuffer(
                newSize * sizeof(ObjectData) * mObjectChunkSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            );
            if (!newBufferExp.has_value()) return newBufferExp.error();
            Buffer& oldBuffer = mObjectBuffer.value();
            Buffer newBuffer = std::move(newBufferExp.value());
            tprResult = newBuffer.mapMemory();
            if (tprResult != TPR_SUCCESS) return tprResult;

            mObjectsFreeList.clear();
            for (uint32_t i = handles.size(); i < newSize; i++) {
                mObjectsFreeList.push_back(i);
            }

            for (auto it = mChunks.begin(); it != mChunks.end();) {
                if (!handles.contains(it->first)) {
                    it = mChunks.erase(it);
                } else {
                    it++;
                }
            }

            std::vector<VkBufferCopy> copyRegions;

            std::byte* newData = reinterpret_cast<std::byte*>(newBuffer.mapping());
            size_t chunkIndex = 0;
            for (
                auto handleIt = handles.begin();
                handleIt != handles.end();
                handleIt++, newData += sizeof(ObjectData) * mObjectChunkSize, chunkIndex++
            ) {
                auto handle = TprComponentChunk{*handleIt};
                auto [chunkIt, newChunk] = mChunks.try_emplace(*handleIt);
                auto& chunk = chunkIt->second;
                auto versionExp = mrScGr.getComponentChunkVersion(handle);
                if (!versionExp.has_value()) return versionExp.error();
                uint32_t version = versionExp.value();
                auto countExp = mrScGr.getComponentChunkElementCount(handle);
                if (!countExp.has_value()) return countExp.error();
                chunk.count = countExp.value();
                tprResult = mrScGr.copyComponentChunkData(handle, 0, 0, reinterpret_cast<char*>(copyBuffer.data()));
                if (tprResult != TPR_SUCCESS) return tprResult;
                for (size_t localEntityIndex = 0; localEntityIndex < chunk.count; localEntityIndex++) {
                    auto dest = newData + localEntityIndex * sizeof(ObjectData);
                    auto& src = copyBuffer[localEntityIndex];
                    // adding this entity to according object image
                    if (get_basic_handle_index(src.image) > mObjectImageCounter) return TPR_ERROR_INVALID_VALUE;
                    if (!mObjectImages.contains(get_basic_handle_index(src.image))) return TPR_ERROR_INVALID_VALUE;
                    newObjectDataIndices[get_basic_handle_index(src.image)].push_back(chunkIndex + localEntityIndex);
                    if (newChunk || version != chunk.cachedVersion) {
                        // need to update the data
                        chunk.cachedVersion = version;
                        glm::mat4 matrix(
                            src.transform.x0, src.transform.y0, src.transform.z0, src.transform.w0,
                            src.transform.x1, src.transform.y1, src.transform.z1, src.transform.w1,
                            src.transform.x2, src.transform.y2, src.transform.z2, src.transform.w2,
                            src.transform.x3, src.transform.y3, src.transform.z3, src.transform.w3
                        );
                        std::memcpy(dest + offsetof(ObjectData, matrix), &matrix, sizeof(matrix));
                    } else {
                        // can just copy from old buffer to new buffer
                        VkBufferCopy region{};
                        region.size = sizeof(ObjectData) * chunk.count;
                        region.srcOffset = chunk.offset * sizeof(ObjectData) * mObjectChunkSize;
                        region.dstOffset = chunkIndex * sizeof(ObjectData) * mObjectChunkSize;
                        copyRegions.push_back(region);
                    }
                }
                chunk.offset = chunkIndex * sizeof(ObjectData) * mObjectChunkSize;
            }

            newBuffer.unmapMemory();

            if (!copyRegions.empty()) {
                vkResult = mSym.vkResetCommandBuffer(mImmidiateCopyCmdBuffer, 0);
                if (vkResult != VK_SUCCESS) {
                    mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkBeginCommandBuffer failed [" << vkResult << "]\n";
                    return TPR_UNKNOWN_ERROR;
                }
                VkCommandBufferBeginInfo cmdBeginInfo{};
                cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vkResult = mSym.vkBeginCommandBuffer(mImmidiateCopyCmdBuffer, &cmdBeginInfo);
                if (vkResult != VK_SUCCESS) {
                    mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkBeginCommandBuffer failed [" << vkResult << "]\n";
                    return TPR_UNKNOWN_ERROR;
                }

                mSym.vkCmdCopyBuffer(mImmidiateCopyCmdBuffer, oldBuffer.handle(), newBuffer.handle(), copyRegions.size(), copyRegions.data());
                vkResult = mSym.vkEndCommandBuffer(mImmidiateCopyCmdBuffer);
                if (vkResult != VK_SUCCESS) {
                    mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkEndCommandBuffer failed [" << vkResult << "]\n";
                    return TPR_UNKNOWN_ERROR;
                }
                // submitting render queue
                VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
                VkSubmitInfo submitInfo{};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &mImmidiateCopyCmdBuffer;
                submitInfo.pWaitDstStageMask = &waitStageMask;
                vkResult = mSym.vkQueueSubmit(mRenderQueue, 1, &submitInfo, mImmidiateCopyFence);
                if (vkResult != VK_SUCCESS) {
                    mLogger.error(TPR_LOG_STYLE_ERROR1) << "render: vkQueueSubmit failed [" << vkResult << "]\n";
                    return TPR_UNKNOWN_ERROR;
                }
                vkResult = mSym.vkWaitForFences(mDevice, 1, &mImmidiateCopyFence, VK_TRUE, UINT64_MAX);
                if (vkResult != VK_SUCCESS) {
                    mLogger.error(TPR_LOG_STYLE_ERROR1) << "render: vkWaitForFences failed [" << vkResult << "]\n";
                    return TPR_UNKNOWN_ERROR;
                }
                vkResult = mSym.vkResetFences(mDevice, 1, &mImmidiateCopyFence);
                if (vkResult != VK_SUCCESS) {
                    mLogger.error(TPR_LOG_STYLE_ERROR1) << "render: vkResetFences failed [" << vkResult << "]\n";
                    return TPR_UNKNOWN_ERROR;
                }
            }

            mObjectBuffer.reset();
            mObjectBuffer.emplace(std::move(newBuffer));

        } else {
            // need to just update the out-of-date data
            for (auto it = mChunks.begin(); it != mChunks.end();) {
                if (!handles.contains(it->first)) {
                    mObjectsFreeList.push_back(it->second.offset);
                    it = mChunks.erase(it);
                } else {
                    it++;
                }
            }
            tprResult = mObjectBuffer->mapMemory();
            if (tprResult != TPR_SUCCESS) return tprResult;

            std::byte* data = reinterpret_cast<std::byte*>(mObjectBuffer->mapping());
            for (
                auto handleIt = handles.begin();
                handleIt != handles.end();
                handleIt++, data += sizeof(ObjectData) * mObjectChunkSize
            ) {
                auto handle = TprComponentChunk{*handleIt};
                auto chunkIt = mChunks.find(*handleIt);
                bool newChunk = false;
                if (chunkIt == mChunks.end()) {
                    mChunks.try_emplace(*handleIt, ChunkEntry{.offset = mObjectsFreeList.back()});
                    mObjectsFreeList.pop_back();
                    newChunk = true;
                }
                auto& chunk = mChunks.at(*handleIt);
                auto versionExp = mrScGr.getComponentChunkVersion(handle);
                if (!versionExp.has_value()) return versionExp.error();
                uint32_t version = versionExp.value();
                auto countExp = mrScGr.getComponentChunkElementCount(handle);
                if (!countExp.has_value()) return countExp.error();
                chunk.count = countExp.value();
                tprResult = mrScGr.copyComponentChunkData(handle, 0, 0, reinterpret_cast<char*>(copyBuffer.data()));
                if (tprResult != TPR_SUCCESS) return tprResult;
                for (size_t localEntityIndex = 0; localEntityIndex < chunk.count; localEntityIndex++) {
                    auto dest = data + localEntityIndex * sizeof(ObjectData);
                    auto& src = copyBuffer[localEntityIndex];
                    // adding this entity to according object image
                    if (get_basic_handle_index(src.image) > mObjectImageCounter) return TPR_ERROR_INVALID_VALUE;
                    if (!mObjectImages.contains(get_basic_handle_index(src.image))) return TPR_ERROR_INVALID_VALUE;
                    newObjectDataIndices[get_basic_handle_index(src.image)].push_back(chunk.offset + localEntityIndex);
                    if (newChunk || version != chunk.cachedVersion) {
                        // need to update the data
                        chunk.cachedVersion = version;
                        updateObjectIndices = true;
                        glm::mat4 matrix(
                            src.transform.x0, src.transform.y0, src.transform.z0, src.transform.w0,
                            src.transform.x1, src.transform.y1, src.transform.z1, src.transform.w1,
                            src.transform.x2, src.transform.y2, src.transform.z2, src.transform.w2,
                            src.transform.x3, src.transform.y3, src.transform.z3, src.transform.w3
                        );
                        std::memcpy(dest + offsetof(ObjectData, matrix), &matrix, sizeof(matrix));
                    }
                }
            }

            mObjectBuffer->unmapMemory();

        }

        if (updateObjectIndices) {
            uint32_t count = 0;
            for (auto& [handle, image] : mObjectImages) {
                auto it = newObjectDataIndices.find(handle);
                if (it != newObjectDataIndices.end()) {
                    image.objectDataIndices = it->second;
                } else {
                    image.objectDataIndices.clear();
                }
                count += image.objectDataIndices.size();
            }
            mObjectIndicesBuffer->reallocate(count * sizeof(uint32_t));
            tprResult = mObjectIndicesBuffer->mapMemory();
            if (tprResult != TPR_SUCCESS) return tprResult;
            auto data = reinterpret_cast<std::byte*>(mObjectIndicesBuffer->mapping());
            size_t i = 0;
            for (auto it = mObjectImages.begin(); it != mObjectImages.end(); it++) {
                auto& image = it->second;
                std::memcpy(data, image.objectDataIndices.data(), image.objectDataIndices.size() * sizeof(uint32_t));
                image.instanceIndicesOffset = i;
                i += image.objectDataIndices.size();
                data += sizeof(uint32_t) * image.objectDataIndices.size();
            }
            mObjectIndicesBuffer->unmapMemory();
        }
    }

    return TPR_SUCCESS;
}


HardwareLayerVulkan::~HardwareLayerVulkan() noexcept {

    mSym.vkDeviceWaitIdle(mDevice);

    mObjectIndicesBuffer.reset();
    mObjectBuffer.reset();

    if (mBasicPipelinelayout) mSym.vkDestroyPipelineLayout(mDevice, mBasicPipelinelayout, nullptr);
    if (mObjectDataSetLayout) mSym.vkDestroyDescriptorSetLayout(mDevice, mObjectDataSetLayout, nullptr);

    if (mImmidiateCopyFence) mSym.vkDestroyFence(mDevice, mImmidiateCopyFence, nullptr);

    if (mCommandPool) mSym.vkDestroyCommandPool(mDevice, mCommandPool, nullptr);

    mIndexBuffers.clear();
    mVertexBuffers.clear();

    mWindowContexts.clear();

    if (mDevice) mSym.vkDestroyDevice(mDevice, nullptr);

    if (mDebugMessenger) {
        if (mSym.vkDestroyDebugUtilsMessengerEXT) {
            mSym.vkDestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
        }
    }

    if (mInstance) mSym.vkDestroyInstance(mInstance, nullptr);
}


TprResult HardwareLayerVulkan::registerWindow(TprWindow handle) noexcept {

    TprResult result;

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

        auto exp = createWindowContext(0, handle);
        if (!exp.has_value()) return exp.error();

        mWindowContexts.try_emplace(handle._d, std::move(exp.value()));

    } catch (const std::exception& e) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << "Unxpected exception: " << e.what() << "\n";
        return TPR_UNKNOWN_ERROR;
    } catch (...) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << "Unknowm exception\n";
        return TPR_UNKNOWN_ERROR;
    }

    return TPR_SUCCESS;
}


void HardwareLayerVulkan::unregisterWindow(TprWindow handle) noexcept {
    try {
        auto it = mWindowContexts.find(handle._d);
        if (it == mWindowContexts.end()) return;
        VkResult result = vkDeviceWaitIdle(mDevice);
        if (result != VK_SUCCESS) return;
        mWindowContexts.erase(it);
        for (auto it = mRenderTargets.begin(); it != mRenderTargets.end();) {
            if (it->second.window._d == handle._d) {
                for (uint32_t objectImage : it->second.objectImages) {
                    destroyObjectImage(construct_basic_handle<TprObjectImage>(it->first, 0, handle_type::object_image));
                }
                it = mRenderTargets.erase(it);
            } else {
                it++;
            }
        }
    } catch (...) {}
}


TprResult HardwareLayerVulkan::render() {

    mFrameCounter = (mFrameCounter + 1) % mMaxFramesInFlight;
    VkResult result;

    for (auto& [id, target] : mRenderTargets) {

        auto ctxIt = mWindowContexts.find(target.window._d);
        if (ctxIt == mWindowContexts.end()) {
            mLogger.error(TPR_LOG_STYLE_PANIC1) << "Corrupted internal structures: target[" << id << "].window is not in mWindowContexts\n";
            return TPR_PANIC;
        }

        auto& ctx = ctxIt->second;
        const Frame& frame = ctx.frames()[mFrameCounter];
        uint32_t swapchainImageIndex;
        VkImage swapchainImage;
        VkDescriptorSet objectSet = ctx.objectSets()[mFrameCounter];

        // render pass begin
        {
            result = mSym.vkWaitForFences(mDevice, 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX);
            if (result != VK_SUCCESS) {
                mLogger.error(TPR_LOG_STYLE_ERROR1) << "render: vkWaitForFences failed [" << result << "]\n";
                return TPR_UNKNOWN_ERROR;
            }

            result = mSym.vkResetCommandPool(mDevice, frame.commandPool, 0);
            if (result != VK_SUCCESS) {
                mLogger.error(TPR_LOG_STYLE_ERROR1) << "render: vkResetCommandPool failed [" << result << "]\n";
                return TPR_UNKNOWN_ERROR;
            }

            // auto resizing the swapchain if size changed
            // Wayland sometimes doesn't invalidate the VkSurface even if it's size has changed so a manual recreation is nessesary
            TprBool8 resized;
            auto exp = mrWinMan.hasWindowResized(ctx.handle());
            if (!exp.has_value()) {
                return exp.error();
            }
            resized = exp.value();
            if (resized) {
                result = mSym.vkDeviceWaitIdle(mDevice);
                if (result != VK_SUCCESS) {
                    mLogger.error(TPR_LOG_STYLE_ERROR1) << "render: vkDeviceWaitIdle failed [" << result << "]\n";
                    return TPR_UNKNOWN_ERROR;
                }
                ctx.recreate();
                return TPR_SUCCESS;
            }

            // acquiring swapchain image
            result = mSym.vkAcquireNextImageKHR(mDevice, ctx.swapchain(), UINT64_MAX, frame.imageAvailableSemaphore, VK_NULL_HANDLE, &swapchainImageIndex);
            switch (result) {
                case VK_ERROR_OUT_OF_DATE_KHR:
                    result = mSym.vkDeviceWaitIdle(mDevice);
                    if (result != VK_SUCCESS) {
                        mLogger.error(TPR_LOG_STYLE_ERROR1) << "render: vkDeviceWaitIdle failed [" << result << "]\n";
                        return TPR_UNKNOWN_ERROR;
                    }
                    ctx.recreate();
                    return TPR_SUCCESS;

                case VK_SUBOPTIMAL_KHR: break;
                case VK_SUCCESS: break;

                default:
                    mLogger.error(TPR_LOG_STYLE_ERROR1) << "render: vkAcquireNextImageKHR failed [" << result << "]\n";
                    return TPR_UNKNOWN_ERROR;
            }
            swapchainImage = ctx.chainImages()[swapchainImageIndex];


            // render command buffer begin
            VkCommandBufferBeginInfo commandBeginInfo{};
            commandBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            commandBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            result = mSym.vkBeginCommandBuffer(frame.renderCommandBuffer(), &commandBeginInfo);
            if (result != VK_SUCCESS) {
                mLogger.error(TPR_LOG_STYLE_ERROR1) << "render: vkBeginCommandBuffer failed [" << result << "]\n";
                return TPR_UNKNOWN_ERROR;
            }

            // render pass begin
            VkClearValue chainClearValues[2];
            chainClearValues[0].color = {{0.1f, 0.11f, 0.13f, 1.0f}};
            // chainClearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            chainClearValues[1].depthStencil = {1.0f, 0};
            VkRenderPassBeginInfo renderPassBeginInfo{};
            renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            uint32_t renderAreaX = std::min(ctx.extent().width, target.scissor.x);
            uint32_t renderAreaY = std::min(ctx.extent().height, target.scissor.y);
            uint32_t renderAreaWidth = std::min(ctx.extent().width - renderAreaX, target.scissor.width);
            uint32_t renderAreaHeight = std::min(ctx.extent().height - renderAreaY, target.scissor.height);
            renderPassBeginInfo.renderArea.extent.width = renderAreaWidth;
            renderPassBeginInfo.renderArea.extent.height = renderAreaHeight;
            renderPassBeginInfo.renderArea.offset.x = renderAreaX;
            renderPassBeginInfo.renderArea.offset.y = renderAreaY;
            renderPassBeginInfo.renderPass = ctx.renderPass().renderPass;
            renderPassBeginInfo.clearValueCount = std::size(chainClearValues);
            renderPassBeginInfo.pClearValues = chainClearValues;
            renderPassBeginInfo.framebuffer = ctx.framebuffers()[swapchainImageIndex];
            mSym.vkCmdBeginRenderPass(frame.renderCommandBuffer(), &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            // scissor
            VkRect2D scissor;
            scissor.offset.x = target.scissor.x;
            scissor.offset.y = target.scissor.y;
            scissor.extent.width = target.scissor.width;
            scissor.extent.height = target.scissor.height;
            mSym.vkCmdSetScissor(frame.renderCommandBuffer(), 0, 1, &scissor);

            // viewport
            VkViewport viewport;
            viewport.x = target.viewport.x;
            viewport.y = target.viewport.y;
            viewport.width = target.viewport.width;
            viewport.height = target.viewport.height;
            viewport.minDepth = target.viewport.minDepth;
            viewport.maxDepth = target.viewport.maxDepth;
            mSym.vkCmdSetViewport(frame.renderCommandBuffer(), 0, 1, &viewport);

            // updating the object data set
            VkDescriptorBufferInfo dataBufferInfo{};
            dataBufferInfo.buffer = mObjectBuffer->handle();
            dataBufferInfo.offset = 0;
            dataBufferInfo.range = VK_WHOLE_SIZE;
            VkDescriptorBufferInfo indicesBufferInfo{};
            indicesBufferInfo.buffer = mObjectIndicesBuffer->handle();
            indicesBufferInfo.offset = 0;
            indicesBufferInfo.range = VK_WHOLE_SIZE;
            VkWriteDescriptorSet descSetWrites[] = {
                {
                    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    nullptr, objectSet, 0, 0, 1,
                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr,
                    &dataBufferInfo
                },
                {
                    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    nullptr, objectSet, 1, 0, 1,
                    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr,
                    &indicesBufferInfo
                }
            };
            mSym.vkUpdateDescriptorSets(mDevice, std::size(descSetWrites), descSetWrites, 0, nullptr);
        }

        // rendering
        {
            mSym.vkCmdBindPipeline(
                frame.renderCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, ctx.renderPass().basicPipeline
            );

            mSym.vkCmdBindDescriptorSets(
                frame.renderCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                mBasicPipelinelayout, 0, 1, &objectSet, 0, nullptr
            );

            for (uint32_t imageIndex : target.objectImages) {
                auto& objectImage = mObjectImages.at(imageIndex);
                auto& mesh = mMeshes.at(objectImage.mesh._d);
                VkDeviceSize offset = 0;
                VkBuffer handle = mVertexBuffers.at(mesh.vertexBuffer).positions.handle();
                mSym.vkCmdBindVertexBuffers(
                    frame.renderCommandBuffer(), 0, 1, &handle, &offset
                );
                mSym.vkCmdBindIndexBuffer(frame.renderCommandBuffer(), mIndexBuffers.at(mesh.indexBuffer).indices.handle(), 0, VK_INDEX_TYPE_UINT32);
                mSym.vkCmdDrawIndexed(
                    frame.renderCommandBuffer(), mesh.indicesInterval.size() / sizeof(uint32_t),
                    objectImage.objectDataIndices.size(), mesh.indicesInterval.begin() / sizeof(uint32_t),
                    mesh.verticesInterval.begin() / sizeof(VertexPosition), objectImage.instanceIndicesOffset
                );
            }
        }

        // render pass end
        {
            mSym.vkCmdEndRenderPass(frame.renderCommandBuffer());

            result = mSym.vkEndCommandBuffer(frame.renderCommandBuffer());
            if (result != VK_SUCCESS) {
                mLogger.error(TPR_LOG_STYLE_ERROR1) << "render: vkEndCommandBuffer failed [" << result << "]\n";
                return TPR_UNKNOWN_ERROR;
            }

            result = mSym.vkResetFences(mDevice, 1, &frame.inFlightFence);
            if (result != VK_SUCCESS) {
                mLogger.error(TPR_LOG_STYLE_ERROR1) << "render: vkResetFences failed [" << result << "]\n";
                return TPR_UNKNOWN_ERROR;
            }

            // submitting render queue
            VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkCommandBuffer renderCommandBuffer = frame.renderCommandBuffer();
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &renderCommandBuffer;
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &frame.imageAvailableSemaphore;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &ctx.semaphores()[swapchainImageIndex];
            submitInfo.pWaitDstStageMask = &waitStageMask;
            result = mSym.vkQueueSubmit(mRenderQueue, 1, &submitInfo, frame.inFlightFence);
            if (result != VK_SUCCESS) {
                mLogger.error(TPR_LOG_STYLE_ERROR1) << "render: vkQueueSubmit failed [" << result << "]\n";
                return TPR_UNKNOWN_ERROR;
            }
            // mLogger << "1\n";

            // submitting present queue
            VkSwapchainKHR swapchain = ctx.swapchain();
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.pImageIndices = &swapchainImageIndex;
            presentInfo.pSwapchains = &swapchain;
            presentInfo.swapchainCount = 1;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &ctx.semaphores()[swapchainImageIndex];
            result = mSym.vkQueuePresentKHR(mRenderQueue, &presentInfo);
            switch (result) {
                case VK_ERROR_OUT_OF_DATE_KHR:
                case VK_SUBOPTIMAL_KHR:
                    result = mSym.vkDeviceWaitIdle(mDevice);
                    if (result != VK_SUCCESS) {
                        mLogger.error(TPR_LOG_STYLE_ERROR1) << "render: vkDeviceWaitIdle failed [" << result << "]\n";
                        return TPR_UNKNOWN_ERROR;
                    }
                    ctx.recreate();
                    return TPR_SUCCESS;

                case VK_SUCCESS: break;

                default:
                    mLogger.error(TPR_LOG_STYLE_ERROR1) << "render: vkQueuePresentKHR failed [" << result << "]\n";
                    return TPR_UNKNOWN_ERROR;
            }
            // mLogger << "1\n";
        }
    }
    // mLogger << "AAA\n";

    return TPR_SUCCESS;
}

