
#include "backend.hpp"
#include "core.hpp"
#include "graphics_common.hpp"
#include "i_graphics_device.hpp"
#include "logger.hpp"
#include "plugin_core.h"
#include "file_registry.hpp"
#include "settings.hpp"
#include "windowing.hpp"
#include "plugin_core.h"
#include "scene_graph.hpp"
#include "scheduler.hpp"
#include "asset_store.hpp"
#include "log_entry.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <exception>
#include <mutex>
#include <vector>
#include <memory>

#include <vulkan/vulkan.h>

#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_core.h>


// registring graphics device backend
GraphicsDeviceBackendInfo backendInfo {
    [](
        Logger logger, FileRegistry& rFileReg, Windowing& rWinMan, Settings& rSettings, SceneGraph& rScGr,
        Scheduler& rSched, AssetStore& rAstr, std::atomic<TprResult>& rRunResult, uint32_t packedEngineVersion
    ) {
        return PGraphicsDevice(std::make_unique<VulkanBackend>(
            logger, rFileReg, rWinMan, rSettings, rScGr, rSched, rAstr, rRunResult, packedEngineVersion
        ));
    },
    "Standard Vulkan HWL", GraphicsAPI::Vulkan
};
static_registry<GraphicsDeviceBackendInfo, 0>::registrar registrar(backendInfo);


VulkanBackend::VulkanBackend(
    Logger logger, FileRegistry& rResReg, Windowing& rWinMan, Settings& rSettings, SceneGraph& rScGr,
    Scheduler& rSched, AssetStore& rAstr, std::atomic<TprResult>& rRunResult, uint32_t packedEngineVersion
) : mLogger(logger), mrFileReg(rResReg), mrWin(rWinMan), mrSett(rSettings), mrScGr(rScGr),
    mrSched(rSched), mrAstr(rAstr), mrRunResult(rRunResult), mPackedEngineVersion(packedEngineVersion) {}

TprResult VulkanBackend::init() {
    std::lock_guard<std::mutex> lock(mMutex);
    assert(!mInitialised);

    auto loadPtrExp = mrWin.getVkGetInstanceProcAddr();
    if (!loadPtrExp.has_value()) return loadPtrExp.error();
    mLoader.setLoadPtr(loadPtrExp.value());

    if (mLoader.vkEnumerateInstanceVersion()) {
        if (auto r = mLoader.vkEnumerateInstanceVersion()(&mApiVer); r != VK_SUCCESS) {
            mLogger.error() << __FILE__ ": " << __LINE__ << ": vkEnumerateInstanceVersion failed [" << r << "]";
            return TPR_ERROR_NOT_LOADED;
        }
    } else {
        mApiVer = VK_API_VERSION_1_0;
    }

    bool createDebugMessenger = false;

    // instance
    {
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.applicationVersion = 0;
        appInfo.engineVersion = mPackedEngineVersion;
        appInfo.apiVersion = mApiVer;
        appInfo.pEngineName = "Tempor Engine";
        appInfo.pApplicationName = "Standart Vulkan Graphics Device Backend";

        // layers
        std::vector<const char*> layers;
        
        if (mrSett.createSettingBoolOr(mrSett.getRoot(), "standartVulkanHWL.enableKhronosValidationLayer", false)) {
            layers.push_back("VK_LAYER_KHRONOS_validation");
        }

        uint32_t layerCount;
        if (auto r = mLoader.vkEnumerateInstanceLayerProperties()(&layerCount, nullptr); r != VK_SUCCESS) {
            mLogger.error() << __FILE__ ": " << __LINE__ << ": vkEnumerateInstanceVersion failed [" << r << "]";
            return TPR_ERROR_NOT_LOADED;
        }
        std::vector<VkLayerProperties> layerProps(layerCount);
        if (auto r = mLoader.vkEnumerateInstanceLayerProperties()(&layerCount, layerProps.data()); r != VK_SUCCESS) {
            mLogger.error() << __FILE__ ": " << __LINE__ << ": vkEnumerateInstanceVersion failed [" << r << "]";
            return TPR_ERROR_NOT_LOADED;
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
                mLogger.error() << __FILE__ ": " << __LINE__ << ": No support for required vulkan instance layer: " << layer;
                return TPR_NOT_SUPPORTED;
            }
        }

        mLogger.debug() << "Getting Vulkan Instance extension list";
        // extensions
        auto extExp = mrWin.getVkInstanceExtensions();
        if (!extExp.has_value()) return extExp.error();
        auto windowingExtensions = extExp.value();
        std::vector<const char*> extensions(windowingExtensions.begin(), windowingExtensions.end());
        if (mrSett.createSettingBoolOr(mrSett.getRoot(), "standartVulkanHWL.enableDebugUtils", false)) {
            createDebugMessenger = true;
            extensions.push_back("VK_EXT_debug_utils");
        }

        uint32_t extCount;
        if (auto r = mLoader.vkEnumerateInstanceExtensionProperties()(nullptr, &extCount, nullptr); r != VK_SUCCESS) {
            mLogger.error() << __FILE__ ": " << __LINE__ << ": vkEnumerateInstanceExtensionProperties failed [" << r << "]";
            return TPR_ERROR_NOT_LOADED;
        }
        std::vector<VkExtensionProperties> extProps(extCount);
        if (auto r = mLoader.vkEnumerateInstanceExtensionProperties()(nullptr, &extCount, extProps.data()); r != VK_SUCCESS) {
            mLogger.error() << __FILE__ ": " << __LINE__ << ": vkEnumerateInstanceExtensionProperties failed [" << r << "]";
            return TPR_ERROR_NOT_LOADED;
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
                mLogger.error() << __FILE__ ": " << __LINE__ << ": No support for required vulkan instance extension: " << ext;
                return TPR_NOT_SUPPORTED;
            }
        }

        VkInstanceCreateInfo instanceCreateInfo{};
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pApplicationInfo = &appInfo;
        instanceCreateInfo.enabledExtensionCount = extensions.size();
        instanceCreateInfo.ppEnabledExtensionNames = extensions.data();
        instanceCreateInfo.enabledLayerCount = layers.size();
        instanceCreateInfo.ppEnabledLayerNames = layers.data();

        if (auto r = mLoader.vkCreateInstance()(&instanceCreateInfo, nullptr, &mInstance); r != VK_SUCCESS) {
            mLogger.error() << __FILE__ ": " << __LINE__ << ": vkCreateInstance failed [" << r << "]";
            return TPR_ERROR_NOT_LOADED;
        }

        mLoader.setInstance(mInstance);

        mLogger.debug() << "Created instance";
    }

    // debug utils messenger
    {
        if (createDebugMessenger) {
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
                const VkDebugUtilsMessengerCallbackDataEXT* callback, void* ctx
            ) -> VkBool32 {

                VulkanBackend* backend = reinterpret_cast<VulkanBackend*>(ctx);

                if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                    backend->mLogger.warn() << callback->pMessage;
                } else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                    backend->mLogger.error() << callback->pMessage;
                } else {
                    backend->mLogger.info() << callback->pMessage;
                }

                if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                    return VK_TRUE;
                }
                return VK_FALSE;
            };
            createInfo.pUserData = this;

            if (auto r = mLoader.vkCreateDebugUtilsMessengerEXT()(mInstance, &createInfo, nullptr, &mDebugMessenger); r != VK_SUCCESS) {
                mLogger.error() << __FILE__ ": " << __LINE__ << ": vkCreateDebugUtilsMessengerEXT failed [" << r << "]";
                return TPR_ERROR_NOT_LOADED;
            }

            mLogger.debug() << "Created debug utils messenger";
        }
    }

    // physical device
    {
        uint32_t count;
        if (auto r = mLoader.vkEnumeratePhysicalDevices()(mInstance, &count, nullptr); r != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkEnumeratePhysicalDevices failed [" << r << "]";
            return TPR_ERROR_NOT_LOADED;
        }
        std::vector<VkPhysicalDevice> physicalDevices(count);
        if (auto r = mLoader.vkEnumeratePhysicalDevices()(mInstance, &count, physicalDevices.data()); r != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkEnumeratePhysicalDevices failed [" << r << "]";
            return TPR_ERROR_NOT_LOADED;
        }

        // TODO: add proper physical device tests

        mPhysicalDevice = physicalDevices[0];

        VkPhysicalDeviceProperties props;
        mLoader.vkGetPhysicalDeviceProperties()(mPhysicalDevice, &props);

        mLogger.debug() << "Picked physical device " << props.deviceName;
    }

    // device
    {
        // TODO: add queue tests
        uint32_t familyCount;
        mLoader.vkGetPhysicalDeviceQueueFamilyProperties()(mPhysicalDevice, &familyCount, nullptr);

        float renderPriority = 1.0f;
        float transferPriority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queues;
        VkDeviceQueueCreateInfo renderQueue{};
        renderQueue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        renderQueue.queueFamilyIndex = 0;
        renderQueue.queueCount = 1;
        renderQueue.pQueuePriorities = &renderPriority;
        queues.push_back(renderQueue);
        VkDeviceQueueCreateInfo transferQueue{};
        transferQueue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        transferQueue.queueFamilyIndex = 1;
        transferQueue.queueCount = 1;
        transferQueue.pQueuePriorities = &transferPriority;
        queues.push_back(transferQueue);

        std::vector<const char*> extensions = {
            "VK_KHR_swapchain"
        };

        uint32_t extCount;
        if (auto r = mLoader.vkEnumerateDeviceExtensionProperties()(mPhysicalDevice, nullptr, &extCount, nullptr); r != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkEnumerateDeviceExtensionProperties failed [" << r << "]";
            return TPR_ERROR_NOT_LOADED;
        }
        std::vector<VkExtensionProperties> props(extCount);
        if (auto r = mLoader.vkEnumerateDeviceExtensionProperties()(mPhysicalDevice, nullptr, &extCount, props.data()); r != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkEnumerateDeviceExtensionProperties failed [" << r << "]";
            return TPR_ERROR_NOT_LOADED;
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
                mLogger.error(TPR_LOG_STYLE_ERROR1) << "No support for crucial vulkan device extension: " << ext;
                return TPR_NOT_SUPPORTED;
            }
        }

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = queues.size();
        createInfo.pQueueCreateInfos = queues.data();
        createInfo.ppEnabledExtensionNames = extensions.data();
        createInfo.enabledExtensionCount = extensions.size();

        if (auto r = mLoader.vkCreateDevice()(mPhysicalDevice, &createInfo, nullptr, &mDevice); r != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkCreateDevice failed [" << r << "]";
            return TPR_ERROR_NOT_LOADED;
        }

        mLoader.setDevice(mDevice);

        mLogger.debug() << "Created device";

        mRenderQueueFamily = 0;
        mLoader.vkGetDeviceQueue()(mDevice, mRenderQueueFamily, 0, &mRenderQueue);
        mTransferQueueFamily = 1;
        mLoader.vkGetDeviceQueue()(mDevice, mTransferQueueFamily, 0, &mTransferQueue);
    }

    // allocator
    {
        VkPhysicalDeviceMemoryProperties memProps;
        mLoader.vkGetPhysicalDeviceMemoryProperties()(mPhysicalDevice, &memProps);
        mAllocMemoryTypes.reserve(memProps.memoryTypeCount);
        mAllocMemoryTypes.resize(memProps.memoryTypeCount);
        std::memcpy(mAllocMemoryTypes.data(), memProps.memoryTypes, memProps.memoryTypeCount * sizeof(VkMemoryType));

        VkPhysicalDeviceProperties props;
        mLoader.vkGetPhysicalDeviceProperties()(mPhysicalDevice, &props);
        mAllocMaxAllocCount = props.limits.maxMemoryAllocationCount;
    }

    VkDescriptorSetLayoutBinding entityDataBindings[] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT}
    };
    VkDescriptorSetLayoutCreateInfo entityDataLayoutInfo{};
    entityDataLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    entityDataLayoutInfo.bindingCount = std::size(entityDataBindings);
    entityDataLayoutInfo.pBindings = entityDataBindings;
    if (auto r = mLoader.vkCreateDescriptorSetLayout()(mDevice, &entityDataLayoutInfo, nullptr, &mEntityDataSetLayout); r != VK_SUCCESS) {
        mLogger.error() << __FILE__ ": " << __LINE__ << ": vkCreateDescriptorSetLayout failed [" << r << "]";
        return TPR_ERROR_NOT_LOADED;
    }

    // frames
    {
        auto maxFramesInFlightExp = mrSett.createSetting(mrSett.getRoot(), "maxFramesInFlight")
            .and_then([&](auto s) { return mrSett.getSettingInteger(s); });
        if (!maxFramesInFlightExp && maxFramesInFlightExp.error() == TPR_PANIC) return TPR_PANIC;
        mMaxFramesInFlight = bounded_or(maxFramesInFlightExp.value_or(3), 0, UINT32_MAX, 3);

        for (uint32_t i = 0; i < mMaxFramesInFlight; i++) {
            Frame frame;
            VkFenceCreateInfo fenceCreateInfo{};
            fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            if (auto r = mLoader.vkCreateFence()(mDevice, &fenceCreateInfo, nullptr, &frame.inFlightFence); r != VK_SUCCESS) {
                mLogger.error() << __FILE__ ": " << __LINE__ << ": vkCreateFence failed [" << r << "]";
                return TPR_ERROR_NOT_LOADED;
            }

            VkCommandPoolCreateInfo commandPoolInfo{};
            commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            commandPoolInfo.queueFamilyIndex = mRenderQueueFamily;
            if (auto r = mLoader.vkCreateCommandPool()(mDevice, &commandPoolInfo, nullptr, &frame.commandPool); r != VK_SUCCESS) {
                mLogger.error() << __FILE__ ": " << __LINE__ << ": vkCreateCommandPool failed [" << r << "]";
                return TPR_ERROR_NOT_LOADED;
            }

            VkCommandBuffer commandBuffers[2];
            VkCommandBufferAllocateInfo commandAllocInfo{};
            commandAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            commandAllocInfo.commandBufferCount = std::size(commandBuffers);
            commandAllocInfo.commandPool = frame.commandPool;
            commandAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            if (auto r = mLoader.vkAllocateCommandBuffers()(mDevice, &commandAllocInfo, commandBuffers); r != VK_SUCCESS) {
                mLogger.error() << __FILE__ ": " << __LINE__ << ": vkAllocateCommandBuffers failed [" << r << "]";
                return TPR_ERROR_NOT_LOADED;
            }
            frame.presentCommandBuffer = commandBuffers[0];
            frame.renderCommandBuffer = commandBuffers[1];

            auto entityChunksBufferExp = createFullBuffer(
                0, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            );
            if (!entityChunksBufferExp.has_value()) return entityChunksBufferExp.error();
            frame.entityChunksBuffer = std::move(entityChunksBufferExp.value());

            VkDescriptorPoolSize poolSizes[] = {
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}
            };
            VkDescriptorPoolCreateInfo descPoolInfo{};
            descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            descPoolInfo.poolSizeCount = std::size(poolSizes);
            descPoolInfo.pPoolSizes = poolSizes;
            descPoolInfo.maxSets = 1;
            if (auto r = mLoader.vkCreateDescriptorPool()(mDevice, &descPoolInfo, nullptr, &frame.descriptorPool); r != VK_SUCCESS) {
                mLogger.error() << __FILE__ ": " << __LINE__ << ": vkCreateDescriptorPool failed [" << r << "]";
                return TPR_ERROR_NOT_LOADED;
            }

            VkDescriptorSetAllocateInfo entityDataSetInfo{};
            entityDataSetInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            entityDataSetInfo.descriptorPool = frame.descriptorPool;
            entityDataSetInfo.descriptorSetCount = 1;
            entityDataSetInfo.pSetLayouts = &mEntityDataSetLayout;
            if (auto r = mLoader.vkAllocateDescriptorSets()(mDevice, &entityDataSetInfo, &frame.entityDataSet); r != VK_SUCCESS) {
                mLogger.error() << __FILE__ ": " << __LINE__ << ": vkAllocateDescriptorSets failed [" << r << "]";
                return TPR_ERROR_NOT_LOADED;
            }

            mFrames.emplace_back(std::move(frame));
        }
    }

    VkPipelineLayoutCreateInfo basicPipelineInfo{};
    basicPipelineInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    basicPipelineInfo.setLayoutCount = 1;
    basicPipelineInfo.pSetLayouts = &mEntityDataSetLayout;
    if (auto r = mLoader.vkCreatePipelineLayout()(mDevice, &basicPipelineInfo, nullptr, &mBasicPipelineLayout); r != VK_SUCCESS) {
        mLogger.error() << __FILE__ ": " << __LINE__ << ": vkCreatePipelineLayout failed [" << r << "]";
        return TPR_ERROR_NOT_LOADED;
    }

    VkCommandPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.queueFamilyIndex = mTransferQueueFamily;
    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (auto r = mLoader.vkCreateCommandPool()(mDevice, &poolCreateInfo, nullptr, &mCommandPool); r != VK_SUCCESS) {
        mLogger.error() << __FILE__ ": " << __LINE__ << ": vkCreateCommandPool failed [" << r << "]";
        return TPR_ERROR_NOT_LOADED;
    }

    VkCommandBufferAllocateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    bufferInfo.commandPool = mCommandPool;
    bufferInfo.commandBufferCount = 1;
    bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    if (auto r = mLoader.vkAllocateCommandBuffers()(mDevice, &bufferInfo, &mImmidiateCopyBuffer); r != VK_SUCCESS) {
        mLogger.error() << __FILE__ ": " << __LINE__ << ": vkAllocateCommandBuffers failed [" << r << "]";
        return TPR_ERROR_NOT_LOADED;
    }

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (auto r = mLoader.vkCreateFence()(mDevice, &fenceInfo, nullptr, &mImmidiateCopyFence); r != VK_SUCCESS) {
        mLogger.error() << __FILE__ ": " << __LINE__ << ": vkCreateFence failed [" << r << "]";
        return TPR_ERROR_NOT_LOADED;
    }

    auto indexBufferSizeExp = mrSett.createSetting(mrSett.getRoot(), "StandartVulkanBackend.indexBufferSize")
        .and_then([&](auto s) { return mrSett.getSettingInteger(s); });
    if (!indexBufferSizeExp && indexBufferSizeExp.error() == TPR_PANIC) return TPR_PANIC;
    mIndexBufferSize = bounded_or(indexBufferSizeExp.value_or(16777216), 0, INT64_MAX, 16777216);
    auto vertexBufferSizeExp = mrSett.createSetting(mrSett.getRoot(), "StandartVulkanBackend.vertexBufferSize")
        .and_then([&](auto s) { return mrSett.getSettingInteger(s); });
    if (!vertexBufferSizeExp && vertexBufferSizeExp.error() == TPR_PANIC) return TPR_PANIC;
    mVertexBufferSize = bounded_or(vertexBufferSizeExp.value_or(16777216), 0, INT64_MAX, 16777216);

    auto fetchRenderableFileExp = mrFileReg.createMemoryFile();
    if (!fetchRenderableFileExp.has_value()) return fetchRenderableFileExp.error();
    mFetchRenderableFile = fetchRenderableFileExp.value();

    auto renderableExp = mrScGr.createComponent(sizeof(TprComponentRenderable));
    if (!renderableExp.has_value()) return renderableExp.error();
    mComponentRenderable = renderableExp.value();

    TprJobCreateInfo renderJobInfo{};
    renderJobInfo.duration = TPR_JOB_DURATION_LONG;
    renderJobInfo.triggerType = TPR_JOB_TRIGGER_TYPE_SCHEDULE;
    renderJobInfo.context = this;
    renderJobInfo.function = [](void* ctx, TprJob job) noexcept {
        VulkanBackend* backend = reinterpret_cast<VulkanBackend*>(ctx);
        if (auto r = backend->render(); r != TPR_SUCCESS) {
            backend->mLogger.error() << "render failed [" << r << "]";
            backend->mrRunResult.store(r);
        }
    };
    auto renderJobExp = mrSched.createJob(renderJobInfo);
    if (!renderJobExp.has_value()) return renderJobExp.error();
    mRenderJob = renderJobExp.value();

    TprJobCreateInfo renderSignalJobInfo{};
    renderSignalJobInfo.duration = TPR_JOB_DURATION_SHORT;
    renderSignalJobInfo.triggerType = TPR_JOB_TRIGGER_TYPE_SCHEDULE;
    auto renderSignalJobExp = mrSched.createJob(renderSignalJobInfo);
    if (!renderSignalJobExp.has_value()) return renderSignalJobExp.error();
    mRenderSignalJob = renderSignalJobExp.value();

    mRenderLaunchTime = mrSched.now();
    if (auto r = mrSched.scheduleJob(mRenderJob, mRenderLaunchTime); r != TPR_SUCCESS) return r;

    mInitialised = true;

    return TPR_SUCCESS;
}

VulkanBackend::~VulkanBackend() noexcept {

    if (auto r = mLoader.vkDeviceWaitIdle()(mDevice); r != VK_SUCCESS) {
        mLogger.panic() << "vkDeviceWaitIdle failed [" << r << "]";
        mrRunResult.store(TPR_PANIC);
        return;
    }

    mMeshes.clear();

    for (auto& [handle, target] : mRenderTargets) {
        freeFullBuffer(target.entry->indirectDrawBuffer);
    }

    for (auto& [id, ctx] : mWindowContexts) {
        freeWindowEntry(ctx);
    }

    for (auto& frame : mFrames) {
        freeFullBuffer(frame.entityChunksBuffer);
        mLoader.vkDestroyCommandPool()(mDevice, frame.commandPool, nullptr);
        mLoader.vkDestroyFence()(mDevice, frame.inFlightFence, nullptr);
        mLoader.vkDestroyDescriptorPool()(mDevice, frame.descriptorPool, nullptr);
    }

    mLoader.vkDestroyPipelineLayout()(mDevice, mBasicPipelineLayout, nullptr);

    mLoader.vkDestroyCommandPool()(mDevice, mCommandPool, nullptr);
    mLoader.vkDestroyDescriptorSetLayout()(mDevice, mEntityDataSetLayout, nullptr);
    mLoader.vkDestroyFence()(mDevice, mImmidiateCopyFence, nullptr);

    if (mDevice) mLoader.vkDestroyDevice()(mDevice, nullptr);

    if (mLoader.vkDestroyDebugUtilsMessengerEXT()) {
        mLoader.vkDestroyDebugUtilsMessengerEXT()(mInstance, mDebugMessenger, nullptr);
    }

    mLoader.vkDestroyInstance()(mInstance, nullptr);
}


expected<TprDepthDomain, TprResult> VulkanBackend::createDepthDomain(const TprDepthDomainCreateInfo& info) noexcept {
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        auto orderIt = mDepthDomainOrder.end();
        if (info.anchor._d != 0) {
            if (get_basic_handle_type(info.anchor) != handle_type::depth_domain) return unexpected(TPR_ERROR_INVALID_VALUE);
            auto handleIt = mDepthDomains.find(get_basic_handle_index(info.anchor));
            if (handleIt == mDepthDomains.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
            auto entry = handleIt->second.entry;
            orderIt = std::ranges::find_if(mDepthDomainOrder, [&](const auto& domain) { return domain.lock() == entry; });
            if (orderIt == mDepthDomainOrder.end()) {
                mLogger.panic() << "Corrupted internal structures: mDepthDomainOrder doesn't contain Depth Domain " << get_basic_handle_index(info.anchor);
                mrRunResult.store(TPR_PANIC);
                return unexpected(TPR_PANIC);
            }
            if (!(info.flags & TPR_CREATE_DEPTH_DOMAIN_BEFORE_ANCHOR_FLAG_BIT)) {
                orderIt = std::next(orderIt);
            }
        } else {
            if (info.flags & TPR_CREATE_DEPTH_DOMAIN_BEFORE_ANCHOR_FLAG_BIT) {
                orderIt = mDepthDomainOrder.begin();
            }
        }
        auto& handle = mDepthDomains.insert_or_assign(
            mDepthDomainCounter, DepthDomainHandle{.entry = std::make_shared<DepthDomainEntry>()}
        ).first->second;
        mDepthDomainOrder.insert(orderIt, handle.entry);
        mLogger.debug() << "Created Depth Domain " << mDepthDomainCounter;
        TprDepthDomain h = construct_basic_handle<TprDepthDomain>(mDepthDomainCounter, 0, handle_type::depth_domain);
        mDepthDomainCounter++;
        return h;
        
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

expected<TprDepthDomain, TprResult> VulkanBackend::createDepthDomainCapability(TprDepthDomain domain, TprDepthDomainCapabilityFlags mask) noexcept {
    if (get_basic_handle_type(domain) != handle_type::depth_domain) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        auto it = mDepthDomains.find(get_basic_handle_index(domain));
        if (it == mDepthDomains.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        mDepthDomains.insert_or_assign(mDepthDomainCounter, DepthDomainHandle{it->second.capability & mask, it->second.entry});
        mLogger.debug() << "Created Depth Domain capability " << mDepthDomainCounter << " for Depth Domain " << get_basic_handle_index(domain);
        TprDepthDomain h = construct_basic_handle<TprDepthDomain>(mDepthDomainCounter, 0, handle_type::depth_domain);
        mDepthDomainCounter++;
        return h;
        
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

void VulkanBackend::destroyDepthDomain(TprDepthDomain domain) noexcept {
    if (get_basic_handle_type(domain) != handle_type::depth_domain) return;
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        auto it = mDepthDomains.find(get_basic_handle_index(domain));
        if (it == mDepthDomains.end()) return;
        auto entry = it->second.entry;
        mDepthDomains.erase(it);
        if (entry.use_count() == 1) {
            auto it = std::ranges::find_if(mDepthDomainOrder, [&](const auto& domain) { return domain.lock() == entry; });
            if (it != mDepthDomainOrder.end()) mDepthDomainOrder.erase(it);
        }
        mLogger.debug() << "Destroyed Depth Domain " << mDepthDomainCounter;
        
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
    }
}


expected<TprRenderTarget, TprResult> VulkanBackend::createRenderTarget(const TprRenderTargetCreateInfo& info) noexcept {
    if (get_basic_handle_type(info.depthDomain) != handle_type::depth_domain) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        auto domainIt = mDepthDomains.find(get_basic_handle_index(info.depthDomain));
        if (domainIt == mDepthDomains.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        auto depthDomain = domainIt->second.entry;

        auto windowIdExp = mrWin.getWindowIdentity(info.window);
        if (!windowIdExp.has_value()) return unexpected(windowIdExp.error());
        auto windowContextIt = mWindowContexts.find(windowIdExp.value());
        if (windowContextIt == mWindowContexts.end()) {
            mLogger.panic() << "Corrupted internal structures: mWindowContexts doesn't contain given WindowIdentity";
            mrRunResult.store(TPR_PANIC);
            return unexpected(TPR_PANIC);
        }
        auto& windowContext = windowContextIt->second;

        auto indirectDrawBufferExp = createExclusiveFullBuffer(
            0, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        );
        if (!indirectDrawBufferExp.has_value()) return unexpected(indirectDrawBufferExp.error());

        auto handle = mRenderTargets.insert_or_assign(
            mRenderTargetCounter, RenderTargetHandle{.entry = std::make_shared<RenderTargetEntry>(
                depthDomain, &windowContext, std::move(indirectDrawBufferExp.value()), info.viewport, info.scissor
            )}
        ).first->second;
        windowContext.renderTargets.push_back(handle.entry);
        depthDomain->renderTargets.push_back(handle.entry);

        mLogger.debug() << "Created Render Target " << mRenderTargetCounter;
        TprRenderTarget h = construct_basic_handle<TprRenderTarget>(mRenderTargetCounter, 0, handle_type::render_target);
        mRenderTargetCounter++;
        return h;
        
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

expected<TprRenderTarget, TprResult> VulkanBackend::createRenderTargetCapability(TprRenderTarget target, TprRenderTargetCapabilityFlags mask) noexcept {
    if (get_basic_handle_type(target) != handle_type::render_target) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        auto it = mRenderTargets.find(get_basic_handle_index(target));
        if (it != mRenderTargets.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        mRenderTargets.insert_or_assign(mRenderTargetCounter, RenderTargetHandle{it->second.capability & mask, it->second.entry});
        mLogger.debug() << "Created Render Target capability " << mRenderTargetCounter << " for Render Target " << get_basic_handle_index(target);
        TprRenderTarget h = construct_basic_handle<TprRenderTarget>(mRenderTargetCounter, 0, handle_type::render_target);
        mRenderTargetCounter++;
        return h;
        
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

void VulkanBackend::destroyRenderTarget(TprRenderTarget target) noexcept {
    if (get_basic_handle_type(target) != handle_type::render_target) return;
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        auto it = mRenderTargets.find(get_basic_handle_index(target));
        if (it != mRenderTargets.end()) return;
        auto entry = it->second.entry;
        mRenderTargets.erase(it);
        if (entry.use_count() == 1) {
            if (auto domain = entry->depthDomain.lock(); domain) {
                auto it = std::ranges::find_if(domain->renderTargets, [&](auto other) { return other.lock() == entry; });
                if (it != domain->renderTargets.end()) domain->renderTargets.erase(it);
            }
            if (entry->windowContext) {
                auto it = std::ranges::find_if(entry->windowContext->renderTargets, [&](auto other) { return other.lock() == entry; });
                if (it != entry->windowContext->renderTargets.end()) entry->windowContext->renderTargets.erase(it);
            }
            for (auto setWeak : entry->sets) {
                auto set = setWeak.lock();
                if (set) {
                    auto it = std::ranges::find_if(set->renderTargets, [&](auto other) { return other.lock() == entry; });
                    if (it != set->renderTargets.end()) set->renderTargets.erase(it);
                }
            }
            freeFullBuffer(entry->indirectDrawBuffer);
        }
        mLogger.debug() << "Destroyed Render Target " << get_basic_handle_index(target);
        
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
    }
}


expected<TprRenderTargetSet, TprResult> VulkanBackend::createRenderTargetSet(const TprRenderTargetSetCreateInfo& info) noexcept {
    if (info.targetCount != 0 && !info.pTargets) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        std::vector<std::shared_ptr<RenderTargetEntry>> renderTargets;
        renderTargets.reserve(info.targetCount);
        for (uint32_t i = 0; i < info.targetCount; i++) {
            auto target = info.pTargets[i];
            if (get_basic_handle_type(target) != handle_type::render_target) return unexpected(TPR_ERROR_INVALID_VALUE);
            auto it = mRenderTargets.find(get_basic_handle_index(target));
            if (it == mRenderTargets.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
            renderTargets.push_back(it->second.entry);
        }
        auto handle = mRenderTargetSets.insert_or_assign(
            mRenderTargetSetCounter, RenderTargetSetHandle{.entry = std::make_shared<RenderTargetSetEntry>()}
        ).first->second;
        handle.entry->renderTargets.reserve(renderTargets.size());
        for (auto target : renderTargets) {
            handle.entry->renderTargets.push_back(target);
            target->sets.push_back(handle.entry);
        }
        mLogger.debug() << "Created Render Target Set " << mRenderTargetSetCounter;
        TprRenderTargetSet h = construct_basic_handle<TprRenderTargetSet>(mRenderTargetSetCounter, 0, handle_type::render_target_set);
        mRenderTargetSetCounter++;
        return h;
        
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

expected<TprRenderTargetSet, TprResult> VulkanBackend::createRenderTargetSetCapability(TprRenderTargetSet set, TprRenderTargetSetCapabilityFlags mask) noexcept {
    if (get_basic_handle_type(set) != handle_type::render_target_set) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        auto it = mRenderTargetSets.find(get_basic_handle_index(set));
        if (it == mRenderTargetSets.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        mRenderTargetSets.insert_or_assign(mRenderTargetSetCounter, RenderTargetSetHandle{it->second.capability & mask, it->second.entry});
        mLogger.debug() << "Created Render Target Set capability " << mRenderTargetSetCounter << " for Render Target Set " << get_basic_handle_index(set);
        TprRenderTargetSet h = construct_basic_handle<TprRenderTargetSet>(mRenderTargetSetCounter, 0, handle_type::render_target_set);
        mRenderTargetSetCounter++;
        return h;
        
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

void VulkanBackend::destroyRenderTargetSet(TprRenderTargetSet set) noexcept {
    if (get_basic_handle_type(set) != handle_type::render_target_set) return;
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        auto it = mRenderTargetSets.find(get_basic_handle_index(set));
        if (it == mRenderTargetSets.end()) return;
        auto entry = it->second.entry;
        mRenderTargetSets.erase(it);
        if (entry.use_count() == 1) {
            for (auto renderTargetWeak : entry->renderTargets) {
                auto target = renderTargetWeak.lock();
                if (target) {
                    auto it = std::ranges::find_if(target->sets, [&](const auto& set) { return set.lock() == entry; });
                    if (it != target->sets.end()) target->sets.erase(it);
                }
            }
        }
        mLogger.debug() << "Destroyed Render Target Set " << get_basic_handle_index(set);
        
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
    }
}


expected<TprEntityImage, TprResult> VulkanBackend::createEntityImage(const TprEntityImageCreateInfo& info) noexcept {
    if (get_basic_handle_type(info.mesh) != handle_type::mesh) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        auto idExp = mrAstr.getMeshIdentity(info.mesh);
        if (!idExp.has_value()) return unexpected(idExp.error());
        auto it = mMeshes.find(idExp.value());
        if (it == mMeshes.end()) {
            mLogger.panic() << "Corrupted internal structures: mMeshes doesn't contain mesh indenity " << idExp.value();
            mrRunResult.store(TPR_PANIC);
            return unexpected(TPR_PANIC);
        }
        auto& mesh = it->second;

        auto handle = mEntityImages.insert_or_assign(
            mEntityImageCounter, EntityImageHandle{.entry = std::make_shared<EntityImageEntry>(&mesh)}
        ).first->second;
        mesh.entityImages.push_back(handle.entry);
        mLogger.debug() << "Created Entity Image " << mEntityImageCounter;
        TprEntityImage h = construct_basic_handle<TprEntityImage>(mEntityImageCounter, 0, handle_type::entity_image);
        mEntityImageCounter++;
        return h;
        
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

expected<TprEntityImage, TprResult> VulkanBackend::createEntityImageCapability(TprEntityImage image, TprEntityImageCapabilityFlags mask) noexcept {
    if (get_basic_handle_type(image) != handle_type::entity_image) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        auto it = mEntityImages.find(get_basic_handle_index(image));
        if (it == mEntityImages.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        mEntityImages.insert_or_assign(mEntityImageCounter, EntityImageHandle{it->second.capability & mask, it->second.entry});
        mLogger.debug() << "Created Entity Image capability " << mEntityImageCounter << " for Entity Image " << get_basic_handle_index(image);
        TprEntityImage h = construct_basic_handle<TprEntityImage>(mEntityImageCounter, 0, handle_type::entity_image);
        mEntityImageCounter++;
        return h;
        
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

void VulkanBackend::destroyEntityImage(TprEntityImage image) noexcept {
    if (get_basic_handle_type(image) != handle_type::entity_image) return;
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        auto it = mEntityImages.find(get_basic_handle_index(image));
        if (it == mEntityImages.end()) return;
        auto entry = it->second.entry;
        mEntityImages.erase(it);
        if (entry.use_count() == 1) {
            if (entry->mesh) {
                auto it = std::ranges::find_if(entry->mesh->entityImages, [&](const auto& image) { return image.lock() == entry; });
                if (it != entry->mesh->entityImages.end()) entry->mesh->entityImages.erase(it);
            }
        }
        mLogger.debug() << "Destroyed Entity Image " << get_basic_handle_index(image);
        
    } catch (const std::exception& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
    }
}


TprJob VulkanBackend::getRenderJob() noexcept {
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    auto exp = mrSched.createJobCapability(mRenderJob, 0);
    if (!exp.has_value()) {
        mrRunResult.store(exp.error());
        return {};
    }
    return exp.value();
}

TprJob VulkanBackend::getRenderSignalJob() noexcept {
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    auto exp = mrSched.createJobCapability(mRenderSignalJob, 0);
    if (!exp.has_value()) {
        mrRunResult.store(exp.error());
        return {};
    }
    return exp.value();
}

TprComponent VulkanBackend::getComponentRenderable() noexcept {
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    return mComponentRenderable;
}


TprResult VulkanBackend::registerWindow(WindowIdentity id) {
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    WindowEntry ctx{};
    ctx.id = id;
    auto surfaceExp = mrWin.createVkSurfaceKHR(id, mInstance, nullptr);
    if (!surfaceExp.has_value()) return surfaceExp.error();
    ctx.surface = surfaceExp.value();
    for (size_t i = 0; i < mMaxFramesInFlight; i++) {
        VkSemaphore semaphore;
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (auto r = mLoader.vkCreateSemaphore()(mDevice, &semaphoreInfo, nullptr, &semaphore); r != VK_SUCCESS) {
            mLogger.error() << __FILE__ ": " << __LINE__ << ": vkCreateSemaphore failed [" << r << "]";
            freeWindowEntry(ctx);
            return TPR_ERROR_NOT_LOADED;
        }
        ctx.imageAvailableSemaphores.push_back(semaphore);
    }
    if (auto r = ensureSwapchain(ctx); r != TPR_SUCCESS) {
        freeWindowEntry(ctx);
        return r;
    }
    mWindowContexts.insert_or_assign(id, std::move(ctx));
    mLogger.trace() << "Registered window identity " << id;
    return TPR_SUCCESS;
}

TprResult VulkanBackend::ensureSwapchain(WindowEntry& ctx) {
    VkSurfaceCapabilitiesKHR surfaceCaps;
    if (auto r = mLoader.vkGetPhysicalDeviceSurfaceCapabilitiesKHR()(mPhysicalDevice, ctx.surface, &surfaceCaps); r != VK_SUCCESS) {
        mLogger.panic() << "vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed [" << r << "]";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }

    if (surfaceCaps.currentExtent.width == UINT32_MAX && surfaceCaps.currentExtent.height == UINT32_MAX) {
        auto widthExp = mrWin.windowPixelWidth(ctx.id);
        if (!widthExp.has_value()) return widthExp.error();
        uint32_t width = widthExp.value();

        auto heightExp = mrWin.windowPixelHeight(ctx.id);
        if (!heightExp.has_value()) return heightExp.error();
        uint32_t height = heightExp.value();

        ctx.extent = {width, height};
        ctx.extent.width = std::clamp(
            ctx.extent.width,
            surfaceCaps.minImageExtent.width,
            surfaceCaps.maxImageExtent.width
        );
        ctx.extent.height = std::clamp(
            ctx.extent.height,
            surfaceCaps.minImageExtent.height,
            surfaceCaps.maxImageExtent.height
        );
    } else {
        ctx.extent.width = surfaceCaps.currentExtent.width;
        ctx.extent.height = surfaceCaps.currentExtent.height;
    }

    if (ctx.extent.width != 0 && ctx.extent.height != 0) {

        uint32_t formatCount;
        if (auto r = mLoader.vkGetPhysicalDeviceSurfaceFormatsKHR()(mPhysicalDevice, ctx.surface, &formatCount, nullptr); r != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkGetPhysicalDeviceSurfaceFormatsKHR failed [" << r << "]";
            mrRunResult.store(TPR_PANIC);
            return TPR_PANIC;
        }
        std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
        if (auto r = mLoader.vkGetPhysicalDeviceSurfaceFormatsKHR()(mPhysicalDevice, ctx.surface, &formatCount, surfaceFormats.data()); r != VK_SUCCESS) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "vkGetPhysicalDeviceSurfaceFormatsKHR failed [" << r << "]";
            mrRunResult.store(TPR_PANIC);
            return TPR_PANIC;
        }

        auto scoreFormat = []( const VkSurfaceFormatKHR& format) -> uint32_t {
            uint32_t score = 0;

            switch (format.format) {
                case VK_FORMAT_B8G8R8A8_UNORM:
                    score += 100;
                case VK_FORMAT_R8G8B8A8_UNORM:
                    score += 1500;
                    break;

                case VK_FORMAT_B8G8R8_UNORM:
                    score += 100;
                case VK_FORMAT_R8G8B8_UNORM:
                    score += 1200;
                    break;
                case VK_FORMAT_B8G8R8A8_SRGB:
                    score += 100;
                case VK_FORMAT_R8G8B8A8_SRGB:
                    score += 2000;
                    break;

                case VK_FORMAT_B8G8R8_SRGB:
                    score += 100;
                case VK_FORMAT_R8G8B8_SRGB:
                    score += 1700;
                    break;

                default: break;
            }

            switch (format.colorSpace) {
                case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
                    score += 1000;
                    break;

                default: break;
            }

            return score;
        };

        VkSurfaceFormatKHR surfaceFormat = *std::max_element(
            surfaceFormats.begin(), surfaceFormats.end(), [&scoreFormat](const VkSurfaceFormatKHR& a, const VkSurfaceFormatKHR& b) -> bool {
                return scoreFormat(a) < scoreFormat(b);
            }
        );

        ctx.swapchain.swapFormat = surfaceFormat.format;

        uint32_t minImageCount = surfaceCaps.minImageCount + 1;
        if (surfaceCaps.maxImageCount != 0 && surfaceCaps.maxImageCount < minImageCount) {
            minImageCount = surfaceCaps.maxImageCount;
        }

        uint32_t presentCount;
        if (auto r = mLoader.vkGetPhysicalDeviceSurfacePresentModesKHR()(mPhysicalDevice, ctx.surface, &presentCount, nullptr); r != VK_SUCCESS) {
            mLogger.error() << __FILE__ ": " << __LINE__ << ": vkGetPhysicalDeviceSurfacePresentModesKHR at count retreival failed [" << r << "]";
            mrRunResult.store(TPR_PANIC);
            return TPR_PANIC;
        }
        std::vector<VkPresentModeKHR> presentModes(presentCount);
        if (auto r = mLoader.vkGetPhysicalDeviceSurfacePresentModesKHR()(mPhysicalDevice, ctx.surface, &presentCount, presentModes.data()); r != VK_SUCCESS) {
            mLogger.error() << __FILE__ ": " << __LINE__ << ": vkGetPhysicalDeviceSurfacePresentModesKHR at modes retreival failed [" << r << "]";
            mrRunResult.store(TPR_PANIC);
            return TPR_PANIC;
        }
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (VkPresentModeKHR mode : presentModes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                presentMode = mode;
                break;
            }
        }

        VkSwapchainKHR oldSwapchain = ctx.swapchain.swapchain;

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.clipped = VK_TRUE;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.imageArrayLayers = 1;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageExtent = ctx.extent;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        createInfo.minImageCount = minImageCount;
        createInfo.oldSwapchain = oldSwapchain;
        createInfo.preTransform = surfaceCaps.currentTransform;
        createInfo.surface = ctx.surface;
        createInfo.presentMode = presentMode;

        if (auto r = mLoader.vkCreateSwapchainKHR()(mDevice, &createInfo, nullptr, &ctx.swapchain.swapchain); r != VK_SUCCESS) {
            mLogger.error() << __FILE__ ": " << __LINE__ << ": vkCreateSwapchainKHR failed [" << r << "]";
            mrRunResult.store(TPR_PANIC);
            return TPR_PANIC;
        }

        auto l = mLogger.debug();
        l << "Created swapchain " << ctx.extent.width << "x" << ctx.extent.height << " with format: ";
        switch (ctx.swapchain.swapFormat) {
            case VK_FORMAT_B8G8R8A8_UNORM: l << "VK_FORMAT_B8G8R8A8_UNORM"; break;
                case VK_FORMAT_R8G8B8A8_UNORM: l << "VK_FORMAT_R8G8B8A8_UNORM"; break;
                case VK_FORMAT_B8G8R8_UNORM: l << "VK_FORMAT_B8G8R8_UNORM"; break;
                case VK_FORMAT_R8G8B8_UNORM: l << "VK_FORMAT_R8G8B8_UNORM"; break;
                case VK_FORMAT_B8G8R8A8_SRGB: l << "VK_FORMAT_B8G8R8A8_SRGB"; break;
                case VK_FORMAT_R8G8B8A8_SRGB: l << "VK_FORMAT_R8G8B8A8_SRGB"; break;
                case VK_FORMAT_B8G8R8_SRGB: l << "VK_FORMAT_B8G8R8_SRGB"; break;
                case VK_FORMAT_R8G8B8_SRGB: l << "VK_FORMAT_R8G8B8_SRGB"; break;
            default: l << ctx.swapchain.swapFormat;
        }
        l.flush();

        for (const auto& link : ctx.swapchain.links) {
            mLoader.vkDestroySemaphore()(mDevice, link.renderFinishedSemaphore, nullptr);
            mLoader.vkDestroyFramebuffer()(mDevice, link.framebuffer, nullptr);
            mLoader.vkDestroyImageView()(mDevice, link.depth.view, nullptr);
            mLoader.vkDestroyImage()(mDevice, link.depth.image, nullptr);
            freeMemory(link.depth.alloc);
            mLoader.vkDestroyImageView()(mDevice, link.swap.view, nullptr);
        }
        mLoader.vkDestroyPipeline()(mDevice, ctx.renderPass.basicPipeline, nullptr);
        mLoader.vkDestroyRenderPass()(mDevice, ctx.renderPass.renderPass, nullptr);

        mLoader.vkDestroySwapchainKHR()(mDevice, oldSwapchain, nullptr);

        ctx.swapchain.depthFormat = VK_FORMAT_D32_SFLOAT;

        uint32_t imageCount;
        if (auto r = mLoader.vkGetSwapchainImagesKHR()(mDevice, ctx.swapchain.swapchain, &imageCount, nullptr); r != VK_SUCCESS) {
            mLogger.panic() << "vkGetSwapchainImagesKHR at count retrieval failed [" << r << "]";
            mrRunResult.store(TPR_PANIC);
            return TPR_PANIC;
        }
        ctx.swapchain.links.assign(imageCount, {});
        std::vector<VkImage> swapchainImages(imageCount);
        if (auto r = mLoader.vkGetSwapchainImagesKHR()(mDevice, ctx.swapchain.swapchain, &imageCount, swapchainImages.data()); r != VK_SUCCESS) {
            mLogger.panic() << "vkGetSwapchainImagesKHR at image retrieval failed [" << r << "]";
            mrRunResult.store(TPR_PANIC);
            return TPR_PANIC;
        }
        
        // render pass
        {
            VkAttachmentDescription attachments[2] = {};
            VkAttachmentDescription& swapAttachment = attachments[0];
            swapAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            swapAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            swapAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            swapAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            swapAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            swapAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            swapAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            swapAttachment.format = ctx.swapchain.swapFormat;

            VkAttachmentDescription& depthAttachment = attachments[1];
            depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            depthAttachment.format = ctx.swapchain.depthFormat;

            VkSubpassDescription subpasses[1] = {};
            VkSubpassDescription& subpass = subpasses[0];
            VkAttachmentReference swapchainReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
            VkAttachmentReference depthReference = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &swapchainReference;
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.pDepthStencilAttachment = &depthReference;

            VkRenderPassCreateInfo renderPassCreateInfo{};
            renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassCreateInfo.attachmentCount = std::size(attachments);
            renderPassCreateInfo.pAttachments = attachments;
            renderPassCreateInfo.subpassCount = std::size(subpasses);
            renderPassCreateInfo.pSubpasses = subpasses;

            if (auto r = mLoader.vkCreateRenderPass()(mDevice, &renderPassCreateInfo, nullptr, &ctx.renderPass.renderPass); r != VK_SUCCESS) {
                mLogger.panic() << "vkCreateRenderPass failed [" << r << "]";
                mrRunResult.store(TPR_PANIC);
                return TPR_PANIC;
            }

            mLogger.debug() << "Created render pass";

        }

        // basic pipeline
        {
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
            
            VkPipelineDepthStencilStateCreateInfo depthState{};
            depthState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthState.depthBoundsTestEnable = VK_FALSE;
            depthState.depthTestEnable = VK_TRUE;
            depthState.depthWriteEnable = VK_TRUE;
            depthState.depthCompareOp = VK_COMPARE_OP_LESS;
            depthState.stencilTestEnable = VK_FALSE;
            depthState.minDepthBounds = 0.0f;
            depthState.maxDepthBounds = 1.0f;

            VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
            inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssembly.primitiveRestartEnable = VK_FALSE;

            VkPipelineShaderStageCreateInfo stages[2] = {};

            VkShaderModule fragShader = VK_NULL_HANDLE;
            VkShaderModule vertShader = VK_NULL_HANDLE;
            
            try {
                auto fragExp = mrFileReg.openFile("shaders/vulkan/basic.frag.spv");
                if (!fragExp.has_value()) return fragExp.error();
                TprFile fragFile = fragExp.value();
                if (auto r = mrFileReg.seek(fragFile, 0, TPR_SEEK_WHENCE_END); r != TPR_SUCCESS) return r;
                auto fragTellExp = mrFileReg.tell(fragFile);
                if (!fragTellExp.has_value()) return fragTellExp.error();
                std::vector<uint32_t> fragData(fragTellExp.value() / sizeof(uint32_t));
                if (auto r = mrFileReg.readAt(fragFile, 0, fragTellExp.value(), reinterpret_cast<std::byte*>(fragData.data())); r != TPR_SUCCESS) return r;
                VkShaderModuleCreateInfo fragModuleCreateInfo{};
                fragModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                fragModuleCreateInfo.codeSize = fragData.size() * sizeof(uint32_t);
                fragModuleCreateInfo.pCode = fragData.data();
                if (auto r = mLoader.vkCreateShaderModule()(mDevice, &fragModuleCreateInfo, nullptr, &fragShader); r != VK_SUCCESS) {
                    mLogger.panic() << "vkCreateShaderModule at debug basic pipeline's fragment shader creation failed [" << r << "]";
                    mrRunResult.store(TPR_PANIC);
                    return TPR_PANIC;
                }
                
                VkPipelineShaderStageCreateInfo& fragStage = stages[1];
                fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                fragStage.module = fragShader;
                fragStage.pName = "main";

                auto vertExp = mrFileReg.openFile("shaders/vulkan/basic.vert.spv");
                if (!vertExp.has_value()) {
                    mLoader.vkDestroyShaderModule()(mDevice, fragShader, nullptr);
                    return vertExp.error();
                }
                TprFile vertFile = vertExp.value();
                if (auto r = mrFileReg.seek(vertFile, 0, TPR_SEEK_WHENCE_END); r != TPR_SUCCESS) {
                    mLoader.vkDestroyShaderModule()(mDevice, fragShader, nullptr);
                    return r;
                }
                auto vertTellExp = mrFileReg.tell(vertFile);
                if (!vertTellExp.has_value()) {
                    mLoader.vkDestroyShaderModule()(mDevice, fragShader, nullptr);
                    return vertTellExp.error();
                }
                std::vector<uint32_t> vertData(vertTellExp.value() / sizeof(uint32_t));
                if (auto r = mrFileReg.readAt(vertFile, 0, vertTellExp.value(), reinterpret_cast<std::byte*>(vertData.data())); r != TPR_SUCCESS) return r;
                VkShaderModuleCreateInfo vertModuleCreateInfo{};
                vertModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                vertModuleCreateInfo.codeSize = vertData.size() * sizeof(uint32_t);
                vertModuleCreateInfo.pCode = vertData.data();
                if (auto r = mLoader.vkCreateShaderModule()(mDevice, &vertModuleCreateInfo, nullptr, &vertShader); r != VK_SUCCESS) {
                    mLogger.panic() << "vkCreateShaderModule at basic pipeline's vertex shader creation failed [" << r << "]";
                    mLoader.vkDestroyShaderModule()(mDevice, fragShader, nullptr);
                    mrRunResult.store(TPR_PANIC);
                    return TPR_PANIC;
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

                auto bindDesc = Vertex::bindDesc();
                auto attribDesc = Vertex::attributeDesc();

                VkPipelineVertexInputStateCreateInfo vertexInput{};
                vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
                vertexInput.pVertexBindingDescriptions = &bindDesc;
                vertexInput.vertexBindingDescriptionCount = 1;
                vertexInput.pVertexAttributeDescriptions = attribDesc.data();
                vertexInput.vertexAttributeDescriptionCount = attribDesc.size();

                VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
                pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
                pipelineCreateInfo.layout = mBasicPipelineLayout;
                pipelineCreateInfo.pColorBlendState = &colourBlend;
                pipelineCreateInfo.pRasterizationState = &raster;
                pipelineCreateInfo.pMultisampleState = &multisampling;
                pipelineCreateInfo.pDynamicState = &dynamicState;
                pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
                pipelineCreateInfo.renderPass = ctx.renderPass.renderPass;
                pipelineCreateInfo.subpass = 0;
                pipelineCreateInfo.pStages = stages;
                pipelineCreateInfo.stageCount = std::size(stages);
                pipelineCreateInfo.pViewportState = &viewportState;
                pipelineCreateInfo.pVertexInputState = &vertexInput;
                pipelineCreateInfo.pDepthStencilState = &depthState;

                if (auto r = mLoader.vkCreateGraphicsPipelines()(mDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &ctx.renderPass.basicPipeline); r != VK_SUCCESS) {
                    mLogger.panic() << "vkCreateGraphicsPipelines at basic pipeline creation failed [" << r << "]";
                    mLoader.vkDestroyShaderModule()(mDevice, fragShader, nullptr);
                    mLoader.vkDestroyShaderModule()(mDevice, vertShader, nullptr);
                    mrRunResult.store(TPR_PANIC);
                    return TPR_PANIC;
                }

                mLoader.vkDestroyShaderModule()(mDevice, fragShader, nullptr);
                mLoader.vkDestroyShaderModule()(mDevice, vertShader, nullptr);

            } catch (...) {
                mLoader.vkDestroyShaderModule()(mDevice, fragShader, nullptr);
                mLoader.vkDestroyShaderModule()(mDevice, vertShader, nullptr);
                throw;
            }
            mLogger.debug() << "Created basic pipeline";
        }

        for (uint32_t i = 0; i < imageCount; i++) {
            auto& link = ctx.swapchain.links[i];
            link.swap.image = swapchainImages[i];
            // swap image view
            {
                VkImageViewCreateInfo viewCreateInfo{};
                viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewCreateInfo.format = ctx.swapchain.swapFormat;
                viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
                viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
                viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
                viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
                viewCreateInfo.image = ctx.swapchain.links[i].swap.image;
                viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                viewCreateInfo.subresourceRange.baseArrayLayer = 0;
                viewCreateInfo.subresourceRange.baseMipLevel = 0;
                viewCreateInfo.subresourceRange.layerCount = 1;
                viewCreateInfo.subresourceRange.levelCount = 1;
                if (auto r = mLoader.vkCreateImageView()(mDevice, &viewCreateInfo, nullptr, &link.swap.view); r != VK_SUCCESS) {
                    mLogger.panic() << "vkCreateImageView failed [" << r << "]";
                    mrRunResult.store(TPR_PANIC);
                    return TPR_PANIC;
                }
            }

            // depth image
            {
                VkImageCreateInfo imageCreateInfo{};
                imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
                imageCreateInfo.arrayLayers = 1;
                imageCreateInfo.extent.width = ctx.extent.width;
                imageCreateInfo.extent.height = ctx.extent.height;
                imageCreateInfo.extent.depth = 1;
                imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                imageCreateInfo.mipLevels = 1;
                imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
                imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
                imageCreateInfo.format = ctx.swapchain.depthFormat;
                if (auto r = mLoader.vkCreateImage()(mDevice, &imageCreateInfo, nullptr, &link.depth.image); r != VK_SUCCESS) {
                    mLogger.panic() << "vkCreateImage failed [" << r << "]";
                    mrRunResult.store(TPR_PANIC);
                    return TPR_PANIC;
                }

                VkMemoryRequirements req;
                mLoader.vkGetImageMemoryRequirements()(mDevice, link.depth.image, &req);

                auto allocExp = allocateMemory(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                if (!allocExp.has_value()) return allocExp.error();
                link.depth.alloc = allocExp.value();

                if (auto r = mLoader.vkBindImageMemory()(mDevice, link.depth.image, link.depth.alloc.memory, link.depth.alloc.offset); r != VK_SUCCESS) {
                    mLogger.panic() << "vkBindMemory failed [" << r << "]";
                    mrRunResult.store(TPR_PANIC);
                    return TPR_PANIC;
                }

                VkImageViewCreateInfo viewCreateInfo{};
                viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewCreateInfo.image = link.depth.image;
                viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
                viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
                viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
                viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
                viewCreateInfo.format = ctx.swapchain.depthFormat;
                viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                viewCreateInfo.subresourceRange.baseArrayLayer = 0;
                viewCreateInfo.subresourceRange.baseMipLevel = 0;
                viewCreateInfo.subresourceRange.layerCount = 1;
                viewCreateInfo.subresourceRange.levelCount = 1;
                if (auto r = mLoader.vkCreateImageView()(mDevice, &viewCreateInfo, nullptr, &link.depth.view); r != VK_SUCCESS) {
                    mLogger.panic() << "vkCreateImageView failed [" << r << "]";
                    mrRunResult.store(TPR_PANIC);
                    return TPR_PANIC;
                }
            }

            // semaphore
            {
                VkSemaphoreCreateInfo semaphoreCreateInfo{};
                semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                if (auto r = mLoader.vkCreateSemaphore()(mDevice, &semaphoreCreateInfo, nullptr, &link.renderFinishedSemaphore); r != VK_SUCCESS) {
                    mLogger.panic() << "vkCreateSemaphore failed [" << r << "]";
                    mrRunResult.store(TPR_PANIC);
                    return TPR_PANIC;
                }
            }
            
            // framebuffer
            {
                VkImageView attachments[] = {link.swap.view, link.depth.view};
                VkFramebufferCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                createInfo.width = ctx.extent.width;
                createInfo.height = ctx.extent.height;
                createInfo.attachmentCount = std::size(attachments);
                createInfo.pAttachments = attachments;
                createInfo.layers = 1;
                createInfo.renderPass = ctx.renderPass.renderPass;
                if (auto r = mLoader.vkCreateFramebuffer()(mDevice, &createInfo, nullptr, &link.framebuffer); r != VK_SUCCESS) {
                    mLogger.panic() << "vkCreateFramebuffer failed [" << r << "]";
                    mrRunResult.store(TPR_PANIC);
                    return TPR_PANIC;
                }
            }
        }
    }
    return TPR_SUCCESS;
}

void VulkanBackend::unregisterWindow(WindowIdentity id) {
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);

    auto it = mWindowContexts.find(id);
    if (it == mWindowContexts.end()) {
        mLogger.panic() << "Corrupted internal structures: mWindowContexts doesn't contain window identity " << id;
        mrRunResult.store(TPR_PANIC);
        return;
    }
    if (auto r = mLoader.vkDeviceWaitIdle()(mDevice); r != VK_SUCCESS) {
        mLogger.panic() << "vkDeviceWaitIdle failed [" << r << "]";
        mrRunResult.store(TPR_PANIC);
        return;
    }
    freeWindowEntry(it->second);
    mWindowContexts.erase(it);
}

void VulkanBackend::freeWindowEntry(WindowEntry& ctx) {
    for (auto targetWeak : ctx.renderTargets) {
        auto target = targetWeak.lock();
        if (target) {
            target->windowContext = nullptr;
        }
    }
    freeSwapchain(ctx);
    for (const auto& semaphore : ctx.imageAvailableSemaphores) {
        mLoader.vkDestroySemaphore()(mDevice, semaphore, nullptr);
    }
    if (ctx.surface) mrWin.destroyVkSurfaceKHR(ctx.id, mInstance, ctx.surface, nullptr);
}

void VulkanBackend::freeSwapchain(WindowEntry& ctx) {
    for (const auto& link : ctx.swapchain.links) {
        mLoader.vkDestroySemaphore()(mDevice, link.renderFinishedSemaphore, nullptr);
        mLoader.vkDestroyFramebuffer()(mDevice, link.framebuffer, nullptr);
        mLoader.vkDestroyImageView()(mDevice, link.depth.view, nullptr);
        mLoader.vkDestroyImage()(mDevice, link.depth.image, nullptr);
        freeMemory(link.depth.alloc);
        mLoader.vkDestroyImageView()(mDevice, link.swap.view, nullptr);
    }
    mLoader.vkDestroySwapchainKHR()(mDevice, ctx.swapchain.swapchain, nullptr);
    mLoader.vkDestroyPipeline()(mDevice, ctx.renderPass.basicPipeline, nullptr);
    mLoader.vkDestroyRenderPass()(mDevice, ctx.renderPass.renderPass, nullptr);
}
