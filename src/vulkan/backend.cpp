
#include "backend.hpp"

#include <cstring>
#include <vector>
#include <vulkan/vulkan_core.h>



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
        std::vector<const char*> layers = {
            "VK_LAYER_KHRONOS_validation"
        };

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
    }

    mpServiceLocator->get<Logger>() << "Created VkInstance\n";

    #ifdef TEMPOR_VK_DEBUG_UTILS_MESSENGER
    {
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
                    This->mpServiceLocator->get<Logger>() << "\033[93m";
                } else if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                    This->mpServiceLocator->get<Logger>() << "\033[91m";
                }

                This->mpServiceLocator->get<Logger>() << callback->pMessage << "\n\033[0m";

                if (severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                    return VK_TRUE;
                }
                return VK_FALSE;
            };
            createInfo.pUserData = this;

            vkCreateDebugUtilsMessengerEXT(mInstance, &createInfo, nullptr, &mDebugMessenger);
        }
        mpServiceLocator->get<Logger>() << "Created VkDebugUtilsMessengerEXT\n";
    }
    #endif

    // physical device
    {
        uint32_t count;
        TOF(vkEnumeratePhysicalDevices(mInstance, &count, nullptr));
        std::vector<VkPhysicalDevice> physicalDevices(count);
        TOF(vkEnumeratePhysicalDevices(mInstance, &count, physicalDevices.data()));

        // TODO: add proper physical device test

        mPhysicalDevice = physicalDevices[0];
    }
    mpServiceLocator->get<Logger>() << "Picked VkPhysicalDevice\n";

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
    }
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(mPhysicalDevice, &props);
    mpServiceLocator->get<Logger>() << "Created VkDevice: " << props.deviceName << "\n";

    mMaxFramesInFlight = 3;
    mpWindowManager = windowManager;

    // surface related
    {
        mSurface = windowManager->createSurfaceVk(mInstance);

        mSwapchain.construct(mPhysicalDevice, mDevice, mSurface, windowManager);

        mFrames.resize(mMaxFramesInFlight);
        for (auto& frame : mFrames) {
            frame.construct(mDevice, 0);
        }
    }
    mpServiceLocator->get<Logger>() << "Created VkSurfaceKHR and VkSwapchainKHR\n";

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

        VkRenderPassCreateInfo renderPassCreateInfo{};
        renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCreateInfo.attachmentCount = std::size(attachments);
        renderPassCreateInfo.pAttachments = attachments;
        renderPassCreateInfo.subpassCount = std::size(subpasses);
        renderPassCreateInfo.pSubpasses = subpasses;

        TOF(vkCreateRenderPass(mDevice, &renderPassCreateInfo, nullptr, &mRenderPass));

        mFramebuffers.construct(mSwapchain, mDevice, mRenderPass);
    }

    mpServiceLocator->get<Logger>() << "Created VkRenderPass\n";

    mpServiceLocator->get<Logger>() << "Init finished\n";

}



void BackendVk::destroy() noexcept {

    vkDeviceWaitIdle(mDevice);

    mFramebuffers.destroy();

    if (mRenderPass) vkDestroyRenderPass(mDevice, mRenderPass, nullptr);

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



void BackendVk::render() {

    mFrameCounter = (mFrameCounter + 1) % mMaxFramesInFlight;

    Frame& frame = mFrames[mFrameCounter];

    TOF(vkWaitForFences(mDevice, 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX));

    TOF(vkResetCommandPool(mDevice, frame.commandPool, 0));

    VkResult result;

    uint32_t swapchainIndex;
    result = vkAcquireNextImageKHR(mDevice, mSwapchain.swapchain(), UINT64_MAX, frame.imageAvailableSemaphore, VK_NULL_HANDLE, &swapchainIndex);
    switch (result) {
        case VK_ERROR_OUT_OF_DATE_KHR:
        case VK_SUBOPTIMAL_KHR:
            TOF(vkDeviceWaitIdle(mDevice));
            mSwapchain.construct(mPhysicalDevice, mDevice, mSurface, mpWindowManager);
            mFramebuffers.destroy();
            mFramebuffers.construct(mSwapchain, mDevice, mRenderPass);
            return;
        case VK_SUCCESS: break;
        default: throw Exception(ErrCode::InternalError, "Failed to acquire swapchain image");
    }

    VkImage swapchainImage = mSwapchain.getImage(swapchainIndex);

    VkCommandBufferBeginInfo commandBeginInfo{};
    commandBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    TOF(vkBeginCommandBuffer(frame.renderCommandBuffer(), &commandBeginInfo));

    // VkImageMemoryBarrier transferDstBarrier{};
    // transferDstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // transferDstBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // transferDstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    // transferDstBarrier.image = swapchainImage;
    // transferDstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    // transferDstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    // transferDstBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    // transferDstBarrier.subresourceRange.baseArrayLayer = 0;
    // transferDstBarrier.subresourceRange.baseMipLevel = 0;
    // transferDstBarrier.subresourceRange.layerCount = 1;
    // transferDstBarrier.subresourceRange.levelCount = 1;
    // transferDstBarrier.srcAccessMask = 0;
    // transferDstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    // vkCmdPipelineBarrier(
    //     frame.renderCommandBuffer(),
    //     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    //     VK_PIPELINE_STAGE_TRANSFER_BIT,
    //     0,
    //     0, nullptr,
    //     0, nullptr,
    //     1, &transferDstBarrier
    // );

    // VkClearColorValue clearColour = {0.21f, 0.205f, 0.22f, 1.0f};
    // VkImageSubresourceRange subresourceRange{};
    // subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    // subresourceRange.baseArrayLayer = 0;
    // subresourceRange.baseMipLevel = 0;
    // subresourceRange.layerCount = 1;
    // subresourceRange.levelCount = 1;
    // vkCmdClearColorImage(
    //     frame.renderCommandBuffer(), swapchainImage,
    //     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColour,
    //     1, &subresourceRange
    // );

    // VkImageMemoryBarrier presentBarrier{};
    // presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    // presentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    // presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    // presentBarrier.image = swapchainImage;
    // presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    // presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    // presentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    // presentBarrier.subresourceRange.baseArrayLayer = 0;
    // presentBarrier.subresourceRange.baseMipLevel = 0;
    // presentBarrier.subresourceRange.layerCount = 1;
    // presentBarrier.subresourceRange.levelCount = 1;
    // presentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    // presentBarrier.dstAccessMask = 0;

    // vkCmdPipelineBarrier(
    //     frame.renderCommandBuffer(),
    //     VK_PIPELINE_STAGE_TRANSFER_BIT,
    //     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
    //     0,
    //     0, nullptr,
    //     0, nullptr,
    //     1, &presentBarrier
    // );

    VkClearValue clearValue = {0.21f, 0.205f, 0.22f, 1.0f};

    VkRenderPassBeginInfo renderBeginInfo{};
    renderBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderBeginInfo.renderArea.extent.width = mSwapchain.extent().width;
    renderBeginInfo.renderArea.extent.height = mSwapchain.extent().height;
    renderBeginInfo.renderArea.offset.x = 0;
    renderBeginInfo.renderArea.offset.y = 0;
    renderBeginInfo.renderPass = mRenderPass;
    renderBeginInfo.clearValueCount = 1;
    renderBeginInfo.pClearValues = &clearValue;
    renderBeginInfo.framebuffer = mFramebuffers.getFramebuffer(swapchainIndex);

    vkCmdBeginRenderPass(frame.renderCommandBuffer(), &renderBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdEndRenderPass(frame.renderCommandBuffer());

    TOF(vkEndCommandBuffer(frame.renderCommandBuffer()));

    TOF(vkResetFences(mDevice, 1, &frame.inFlightFence));

    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.renderCommandBuffer();
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frame.imageAvailableSemaphore;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &mSwapchain.getSemaphore(swapchainIndex);
    submitInfo.pWaitDstStageMask = &waitStageMask;

    TOF(vkQueueSubmit(mRenderQueue, 1, &submitInfo, frame.inFlightFence));

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pImageIndices = &swapchainIndex;
    presentInfo.pSwapchains = &mSwapchain.swapchain();
    presentInfo.swapchainCount = 1;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &mSwapchain.getSemaphore(swapchainIndex);
    
    result = vkQueuePresentKHR(mRenderQueue, &presentInfo);
    switch (result) {
        case VK_ERROR_OUT_OF_DATE_KHR:
        case VK_SUBOPTIMAL_KHR:
            TOF(vkDeviceWaitIdle(mDevice));
            mSwapchain.construct(mPhysicalDevice, mDevice, mSurface, mpWindowManager);
            mFramebuffers.destroy();
            mFramebuffers.construct(mSwapchain, mDevice, mRenderPass);
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
