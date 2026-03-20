
#include "core.hpp"
#include "plugin_core.h"
#include "hardware_layer.hpp"
#include "logger.hpp"
#include "window_manager.hpp"

#include <algorithm>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>



TprResult HardwareLayerVulkan::constructWindowContext(WindowContext& ctx, uint32_t queueFamilyIndex, TprWindow window, bool constructSurface) {

    VkResult result;

    if (constructSurface) {
        ctx.windowHandle = window;

        auto surfaceExp = mrWinMan.createSurfaceVk(ctx.windowHandle, mInstance);
        if (!surfaceExp.has_value()) {
            return surfaceExp.error();
        }
        ctx.surface = surfaceExp.value();

        ctx.frames.resize(mMaxFramesInFlight);
        for (auto& frame : ctx.frames) {
            VkSemaphoreCreateInfo imageAvailableSemaphoreCreateInfo{};
            imageAvailableSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            result = mSym.vkCreateSemaphore(mDevice, &imageAvailableSemaphoreCreateInfo, nullptr, &frame.imageAvailableSemaphore);
            if (result != VK_SUCCESS) {
                mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "constructWindowContext: vkCreateSemaphore failed [" << result << "]\n";
                destroyWindowContext(ctx);
                return TPR_UNKNOWN_ERROR;
            }

            VkFenceCreateInfo fenceCreateInfo{};
            fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            result = mSym.vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &frame.inFlightFence);
            if (result != VK_SUCCESS) {
                mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "constructWindowContext: vkCreateFence failed [" << result << "]\n";
                destroyWindowContext(ctx);
                return TPR_UNKNOWN_ERROR;
            }

            VkCommandPoolCreateInfo poolCreateInfo{};
            poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolCreateInfo.queueFamilyIndex = queueFamilyIndex;
            result = mSym.vkCreateCommandPool(mDevice, &poolCreateInfo, nullptr, &frame.commandPool);
            if (result != VK_SUCCESS) {
                mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "constructWindowContext: vkCreateCommandPool failed [" << result << "]\n";
                destroyWindowContext(ctx);
                return TPR_UNKNOWN_ERROR;
            }

            VkCommandBufferAllocateInfo commandAllocInfo{};
            commandAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            commandAllocInfo.commandBufferCount = std::size(frame.commandBuffers);
            commandAllocInfo.commandPool = frame.commandPool;
            commandAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            result = mSym.vkAllocateCommandBuffers(mDevice, &commandAllocInfo, frame.commandBuffers);
            if (result != VK_SUCCESS) {
                mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "constructWindowContext: vkAllocateCommandBuffers failed [" << result << "]\n";
                destroyWindowContext(ctx);
                return TPR_UNKNOWN_ERROR;
            }
        }
    }

    VkSurfaceCapabilitiesKHR surfaceCaps;
    result = mSym.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, ctx.surface, &surfaceCaps);
    if (result != VK_SUCCESS) {
        mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "constructWindowContext: vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed [" << result << "]\n";
        destroyWindowContext(ctx);
        return TPR_UNKNOWN_ERROR;
    }

    if (surfaceCaps.currentExtent.width == UINT32_MAX && surfaceCaps.currentExtent.height == UINT32_MAX) {
        int32_t width = mrWinMan.getWindowWidth(ctx.windowHandle).value();
        int32_t height = mrWinMan.getWindowHeight(ctx.windowHandle).value();
        ctx.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
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
        result = mSym.vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, ctx.surface, &formatCount, nullptr);
        if (result != VK_SUCCESS) {
            mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "constructWindowContext: vkGetPhysicalDeviceSurfaceFormatsKHR failed [" << result << "]\n";
            destroyWindowContext(ctx);
            return TPR_UNKNOWN_ERROR;
        }
        std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
        result = mSym.vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, ctx.surface, &formatCount, surfaceFormats.data());
        if (result != VK_SUCCESS) {
            mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "constructWindowContext: vkGetPhysicalDeviceSurfaceFormatsKHR failed [" << result << "]\n";
            destroyWindowContext(ctx);
            return TPR_UNKNOWN_ERROR;
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

        ctx.chainImageFormat = surfaceFormat.format;

        uint32_t imageCount = surfaceCaps.minImageCount + 1;
        if (surfaceCaps.maxImageCount != 0 && surfaceCaps.maxImageCount < imageCount) {
            imageCount = surfaceCaps.maxImageCount;
        }

        uint32_t presentCount;
        result = mSym.vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, ctx.surface, &presentCount, nullptr);
        if (result != VK_SUCCESS) {
            mrLogger.error(TPR_LOG_STYLE_ERROR1) << "constructWindowContext: vkGetPhysicalDeviceSurfacePresentModesKHR at count retreival failed [" << result << "]\n";
            destroyWindowContext(ctx);
            return TPR_UNKNOWN_ERROR;
        }
        std::vector<VkPresentModeKHR> presentModes(presentCount);
        result = mSym.vkGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, ctx.surface, &presentCount, presentModes.data());
        if (result != VK_SUCCESS) {
            mrLogger.error(TPR_LOG_STYLE_ERROR1) << "constructWindowContext: vkGetPhysicalDeviceSurfacePresentModesKHR at modes retreival failed [" << result << "]\n";
            destroyWindowContext(ctx);
            return TPR_UNKNOWN_ERROR;
        }
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (VkPresentModeKHR mode : presentModes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                presentMode = mode;
                break;
            }
        }

        VkSwapchainKHR oldSwapchain = ctx.swapchain;

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
        createInfo.minImageCount = imageCount;
        createInfo.oldSwapchain = oldSwapchain;
        createInfo.preTransform = surfaceCaps.currentTransform;
        createInfo.surface = ctx.surface;
        createInfo.presentMode = presentMode;

        result = mSym.vkCreateSwapchainKHR(mDevice, &createInfo, nullptr, &ctx.swapchain);
        if (result != VK_SUCCESS) {
            mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "constructWindowContext: vkCreateSwapchainKHR failed [" << result << "]\n";
            destroyWindowContext(ctx);
            return TPR_UNKNOWN_ERROR;
        }

        auto l = mrLogger.debug();
        l << logPrxPHWL() << "Created swapchain " << ctx.extent.width << "x" << ctx.extent.height << " with format: ";
        switch (ctx.chainImageFormat) {
            case VK_FORMAT_B8G8R8A8_UNORM: l << "VK_FORMAT_B8G8R8A8_UNORM"; break;
                case VK_FORMAT_R8G8B8A8_UNORM: l << "VK_FORMAT_R8G8B8A8_UNORM"; break;
                case VK_FORMAT_B8G8R8_UNORM: l << "VK_FORMAT_B8G8R8_UNORM"; break;
                case VK_FORMAT_R8G8B8_UNORM: l << "VK_FORMAT_R8G8B8_UNORM"; break;
                case VK_FORMAT_B8G8R8A8_SRGB: l << "VK_FORMAT_B8G8R8A8_SRGB"; break;
                case VK_FORMAT_R8G8B8A8_SRGB: l << "VK_FORMAT_R8G8B8A8_SRGB"; break;
                case VK_FORMAT_B8G8R8_SRGB: l << "VK_FORMAT_B8G8R8_SRGB"; break;
                case VK_FORMAT_R8G8B8_SRGB: l << "VK_FORMAT_R8G8B8_SRGB"; break;
            default: l << ctx.chainImageFormat;
        }
        l << "\n";

        if (oldSwapchain != VK_NULL_HANDLE) {
            mSym.vkDestroySwapchainKHR(mDevice, oldSwapchain, nullptr);
        }

        ctx.depthImageFormat = VK_FORMAT_D32_SFLOAT;

        result = mSym.vkGetSwapchainImagesKHR(mDevice, ctx.swapchain, &ctx.imageCount, nullptr);
        if (result != VK_SUCCESS) {
            mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "constructWindowContext: vkGetSwapchainImagesKHR at count retrieval failed [" << result << "]\n";
            destroyWindowContext(ctx);
            return TPR_UNKNOWN_ERROR;
        }

        ctx.chainImages.assign(ctx.imageCount, VK_NULL_HANDLE);
        ctx.chainImageViews.assign(ctx.imageCount, VK_NULL_HANDLE);

        ctx.depthImages.assign(ctx.imageCount, VK_NULL_HANDLE);
        ctx.depthImageViews.assign(ctx.imageCount, VK_NULL_HANDLE);
        ctx.depthImageMemories.assign(ctx.imageCount, VK_NULL_HANDLE);

        ctx.semaphores.assign(ctx.imageCount, VK_NULL_HANDLE);
        ctx.framebuffers.assign(ctx.imageCount, VK_NULL_HANDLE);

        result = mSym.vkGetSwapchainImagesKHR(mDevice, ctx.swapchain, &ctx.imageCount, ctx.chainImages.data());
        if (result != VK_SUCCESS) {
            mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "constructWindowContext: vkGetSwapchainImagesKHR at image retrieval failed [" << result << "]\n";
            destroyWindowContext(ctx);
            return TPR_UNKNOWN_ERROR;
        }

        for (uint32_t i = 0; i < ctx.imageCount; i++) {

            // chain image view
            {
                VkImageViewCreateInfo viewCreateInfo{};
                viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewCreateInfo.format = ctx.chainImageFormat;
                viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
                viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
                viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
                viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
                viewCreateInfo.image = ctx.chainImages[i];
                viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                viewCreateInfo.subresourceRange.baseArrayLayer = 0;
                viewCreateInfo.subresourceRange.baseMipLevel = 0;
                viewCreateInfo.subresourceRange.layerCount = 1;
                viewCreateInfo.subresourceRange.levelCount = 1;
                
                result = mSym.vkCreateImageView(mDevice, &viewCreateInfo, nullptr, &ctx.chainImageViews[i]);
                if (result != VK_SUCCESS) {
                    mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "constructWindowContext: vkCreateImageView failed [" << result << "]\n";
                    destroyWindowContext(ctx);
                    return TPR_UNKNOWN_ERROR;
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
                imageCreateInfo.format = ctx.depthImageFormat;
                result = mSym.vkCreateImage(mDevice, &imageCreateInfo, nullptr, &ctx.depthImages[i]);
                if (result != VK_SUCCESS) {
                    mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "constructWindowContext: vkCreateImage failed [" << result << "]\n";
                    destroyWindowContext(ctx);
                    return TPR_UNKNOWN_ERROR;
                }

                VkMemoryRequirements req;
                mSym.vkGetImageMemoryRequirements(mDevice, ctx.depthImages[i], &req);

                VkMemoryAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                allocInfo.allocationSize = req.size;
                auto memExp = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                if (!memExp.has_value()) {
                    destroyWindowContext(ctx);
                    return memExp.error();
                }
                allocInfo.memoryTypeIndex = memExp.value();
                result = mSym.vkAllocateMemory(mDevice, &allocInfo, nullptr, &ctx.depthImageMemories[i]);
                if (result != VK_SUCCESS) {
                    mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "constructWindowContext: vkAllocateMemory failed [" << result << "]\n";
                    destroyWindowContext(ctx);
                    return TPR_UNKNOWN_ERROR;
                }

                result = mSym.vkBindImageMemory(mDevice, ctx.depthImages[i], ctx.depthImageMemories[i], 0);
                if (result != VK_SUCCESS) {
                    mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "constructWindowContext: vkBindMemory failed [" << result << "]\n";
                    destroyWindowContext(ctx);
                    return TPR_UNKNOWN_ERROR;
                }

                VkImageViewCreateInfo viewCreateInfo{};
                viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewCreateInfo.image = ctx.depthImages[i];
                viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
                viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
                viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
                viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
                viewCreateInfo.format = ctx.depthImageFormat;
                viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                viewCreateInfo.subresourceRange.baseArrayLayer = 0;
                viewCreateInfo.subresourceRange.baseMipLevel = 0;
                viewCreateInfo.subresourceRange.layerCount = 1;
                viewCreateInfo.subresourceRange.levelCount = 1;
                viewCreateInfo.image = ctx.depthImages[i];
                result = mSym.vkCreateImageView(mDevice, &viewCreateInfo, nullptr, &ctx.depthImageViews[i]);
                if (result != VK_SUCCESS) {
                    mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "constructWindowContext: vkCreateImageView failed [" << result << "]\n";
                    destroyWindowContext(ctx);
                    return TPR_UNKNOWN_ERROR;
                }

            }

            // semaphore
            {
                VkSemaphoreCreateInfo semaphoreCreateInfo{};
                semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                result = mSym.vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &ctx.semaphores[i]);
                if (result != VK_SUCCESS) {
                    mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "constructWindowContext: vkCreateSemaphore failed [" << result << "]\n";
                    destroyWindowContext(ctx);
                    return TPR_UNKNOWN_ERROR;
                }
            }

            // framebuffer
            {
                VkImageView attachments[] = {
                    ctx.chainImageViews[i],
                    ctx.depthImageViews[i]
                };

                VkFramebufferCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                createInfo.width = ctx.extent.width;
                createInfo.height = ctx.extent.height;
                createInfo.attachmentCount = std::size(attachments);
                createInfo.pAttachments = attachments;
                createInfo.layers = 1;
                createInfo.renderPass = ctx.renderPass->mRenderPass;

                result = mSym.vkCreateFramebuffer(mDevice, &createInfo, nullptr, &ctx.framebuffers[i]);
                if (result != VK_SUCCESS) {
                    mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "constructWindowContext: vkCreateFramebuffer failed [" << result << "]\n";
                    destroyWindowContext(ctx);
                    return TPR_UNKNOWN_ERROR;
                }
            }
        
        }   
    }

    return TPR_SUCCESS;
}


TprResult HardwareLayerVulkan::reconstructWindowContext(WindowContext& ctx) {

    {
        for (uint32_t i = 0; i < ctx.chainImages.size(); i++) {
            if (ctx.chainImageViews[i]) mSym.vkDestroyImageView(mDevice, ctx.chainImageViews[i], nullptr);

            if (ctx.depthImageViews[i]) mSym.vkDestroyImageView(mDevice, ctx.depthImageViews[i], nullptr);
            if (ctx.depthImageMemories[i]) mSym.vkFreeMemory(mDevice, ctx.depthImageMemories[i], nullptr);
            if (ctx.depthImages[i]) mSym.vkDestroyImage(mDevice, ctx.depthImages[i], nullptr);

            if (ctx.semaphores[i]) mSym.vkDestroySemaphore(mDevice, ctx.semaphores[i], nullptr);
        }
    }

    return constructWindowContext(ctx, mRenderQueue, ctx.windowHandle, false);
}


void HardwareLayerVulkan::destroyWindowContext(WindowContext& ctx) noexcept {

    for (uint32_t i = 0; i < ctx.chainImages.size(); i++) {

        if (ctx.framebuffers[i]) mSym.vkDestroyFramebuffer(mDevice, ctx.framebuffers[i], nullptr);

        if (ctx.semaphores[i]) mSym.vkDestroySemaphore(mDevice, ctx.semaphores[i], nullptr);

        if (ctx.depthImageViews[i]) mSym.vkDestroyImageView(mDevice, ctx.depthImageViews[i], nullptr);
        if (ctx.depthImageMemories[i]) mSym.vkFreeMemory(mDevice, ctx.depthImageMemories[i], nullptr);
        if (ctx.depthImages[i]) mSym.vkDestroyImage(mDevice, ctx.depthImages[i], nullptr);

        if (ctx.chainImageViews[i]) mSym.vkDestroyImageView(mDevice, ctx.chainImageViews[i], nullptr);
    }

    if (ctx.swapchain) mSym.vkDestroySwapchainKHR(mDevice, ctx.swapchain, nullptr);

    if (ctx.surface) mSym.vkDestroySurfaceKHR(mInstance, ctx.surface, nullptr);

}


