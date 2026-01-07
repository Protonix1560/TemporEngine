
#include "core.hpp"
#include "plugin_core.h"
#include "renderer_vulkan.hpp"
#include "logger.hpp"
#include "window_manager.hpp"

#include <algorithm>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>



void Swapchain::construct(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, TprWindow handle) {

    mDevice = device;
    mSurface = surface;

    {
        VkSurfaceCapabilitiesKHR surfaceCaps;
        TOF(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps));

        if (surfaceCaps.currentExtent.width == UINT32_MAX && surfaceCaps.currentExtent.height == UINT32_MAX) {
            int32_t width, height;
            gGetServiceLocator()->get<WindowManager>().getWindowWidth(handle, &width);
            gGetServiceLocator()->get<WindowManager>().getWindowHeight(handle, &height);
            mExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
            mExtent.width = std::clamp(
                mExtent.width,
                surfaceCaps.minImageExtent.width,
                surfaceCaps.maxImageExtent.width
            );
            mExtent.height = std::clamp(
                mExtent.height,
                surfaceCaps.minImageExtent.height,
                surfaceCaps.maxImageExtent.height
            );
        } else {
            mExtent.width = surfaceCaps.currentExtent.width;
            mExtent.height = surfaceCaps.currentExtent.height;
        }

        if (mExtent.width != 0 && mExtent.height != 0) {

            uint32_t formatCount;
            TOF(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr));
            std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
            TOF(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, surfaceFormats.data()));

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

            mChainImageFormat = surfaceFormat.format;

            uint32_t imageCount = surfaceCaps.minImageCount + 1;
            if (surfaceCaps.maxImageCount != 0 && surfaceCaps.maxImageCount < imageCount) {
                imageCount = surfaceCaps.maxImageCount;
            }

            uint32_t presentCount;
            TOF(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, nullptr));
            std::vector<VkPresentModeKHR> presentModes(presentCount);
            TOF(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, presentModes.data()));
            VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
            for (VkPresentModeKHR mode : presentModes) {
                if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                    presentMode = mode;
                    break;
                }
            }

            VkSwapchainKHR oldSwapchain = mSwapchain;

            VkSwapchainCreateInfoKHR createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            createInfo.clipped = VK_TRUE;
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            createInfo.imageArrayLayers = 1;
            createInfo.imageColorSpace = surfaceFormat.colorSpace;
            createInfo.imageFormat = surfaceFormat.format;
            createInfo.imageExtent = mExtent;
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            createInfo.minImageCount = imageCount;
            createInfo.oldSwapchain = oldSwapchain;
            createInfo.preTransform = surfaceCaps.currentTransform;
            createInfo.surface = surface;
            createInfo.presentMode = presentMode;

            TOF(vkCreateSwapchainKHR(device, &createInfo, nullptr, &mSwapchain));

            if (!mConstructed) {
                auto l = gGetServiceLocator()->get<Logger>().debug();
                l << LOG_RENDERER_NAME ": Created swapchain " << mExtent.width << "x" << mExtent.height << " with format: ";
                switch (mChainImageFormat) {
                    case VK_FORMAT_B8G8R8A8_UNORM: l << "VK_FORMAT_B8G8R8A8_UNORM"; break;
                        case VK_FORMAT_R8G8B8A8_UNORM: l << "VK_FORMAT_R8G8B8A8_UNORM"; break;
                        case VK_FORMAT_B8G8R8_UNORM: l << "VK_FORMAT_B8G8R8_UNORM"; break;
                        case VK_FORMAT_R8G8B8_UNORM: l << "VK_FORMAT_R8G8B8_UNORM"; break;
                        case VK_FORMAT_B8G8R8A8_SRGB: l << "VK_FORMAT_B8G8R8A8_SRGB"; break;
                        case VK_FORMAT_R8G8B8A8_SRGB: l << "VK_FORMAT_R8G8B8A8_SRGB"; break;
                        case VK_FORMAT_B8G8R8_SRGB: l << "VK_FORMAT_B8G8R8_SRGB"; break;
                        case VK_FORMAT_R8G8B8_SRGB: l << "VK_FORMAT_R8G8B8_SRGB"; break;
                    default: l << mChainImageFormat;
                }
                l << "\n";
            }

            if (oldSwapchain) {
                vkDestroySwapchainKHR(mDevice, oldSwapchain, nullptr);
            }
        }

    }

    {
        mDepthImageFormat = VK_FORMAT_D32_SFLOAT;
    }

    // cleanup if needed
    {
        for (uint32_t i = 0; i < mChainImages.size(); i++) {
            if (mChainImageViews[i]) vkDestroyImageView(mDevice, mChainImageViews[i], nullptr);

            if (mDepthImageViews[i]) vkDestroyImageView(mDevice, mDepthImageViews[i], nullptr);
            if (mDepthImageMemories[i]) vkFreeMemory(mDevice, mDepthImageMemories[i], nullptr);
            if (mDepthImages[i]) vkDestroyImage(mDevice, mDepthImages[i], nullptr);

            if (mSemaphores[i]) vkDestroySemaphore(mDevice, mSemaphores[i], nullptr);
        }

    }

    if (mExtent.width != 0 && mExtent.height != 0) {

        TOF(vkGetSwapchainImagesKHR(device, mSwapchain, &mImageCount, nullptr));

        mChainImages.assign(mImageCount, VK_NULL_HANDLE);
        mChainImageViews.assign(mImageCount, VK_NULL_HANDLE);

        mDepthImages.assign(mImageCount, VK_NULL_HANDLE);
        mDepthImageViews.assign(mImageCount, VK_NULL_HANDLE);
        mDepthImageMemories.assign(mImageCount, VK_NULL_HANDLE);

        mSemaphores.assign(mImageCount, VK_NULL_HANDLE);

        TOF(vkGetSwapchainImagesKHR(device, mSwapchain, &mImageCount, mChainImages.data()));

        for (uint32_t i = 0; i < mImageCount; i++) {

            // chain image view
            {
                VkImageViewCreateInfo viewCreateInfo{};
                viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewCreateInfo.format = mChainImageFormat;
                viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
                viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
                viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
                viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
                viewCreateInfo.image = mChainImages[i];
                viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                viewCreateInfo.subresourceRange.baseArrayLayer = 0;
                viewCreateInfo.subresourceRange.baseMipLevel = 0;
                viewCreateInfo.subresourceRange.layerCount = 1;
                viewCreateInfo.subresourceRange.levelCount = 1;
                
                TOF(vkCreateImageView(device, &viewCreateInfo, nullptr, &mChainImageViews[i]));
            }

            // depth image
            {

                VkImageCreateInfo imageCreateInfo{};
                imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
                imageCreateInfo.arrayLayers = 1;
                imageCreateInfo.extent.width = mExtent.width;
                imageCreateInfo.extent.height = mExtent.height;
                imageCreateInfo.extent.depth = 1;
                imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                imageCreateInfo.mipLevels = 1;
                imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
                imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
                imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
                imageCreateInfo.format = mDepthImageFormat;
                TOF(vkCreateImage(device, &imageCreateInfo, nullptr, &mDepthImages[i]));

                VkMemoryRequirements req;
                vkGetImageMemoryRequirements(device, mDepthImages[i], &req);

                VkMemoryAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                allocInfo.allocationSize = req.size;
                allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                TOF(vkAllocateMemory(device, &allocInfo, nullptr, &mDepthImageMemories[i]));

                TOF(vkBindImageMemory(device, mDepthImages[i], mDepthImageMemories[i], 0));

                VkImageViewCreateInfo viewCreateInfo{};
                viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewCreateInfo.image = mDepthImages[i];
                viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
                viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
                viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
                viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
                viewCreateInfo.format = mDepthImageFormat;
                viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                viewCreateInfo.subresourceRange.baseArrayLayer = 0;
                viewCreateInfo.subresourceRange.baseMipLevel = 0;
                viewCreateInfo.subresourceRange.layerCount = 1;
                viewCreateInfo.subresourceRange.levelCount = 1;
                viewCreateInfo.image = mDepthImages[i];
                TOF(vkCreateImageView(device, &viewCreateInfo, nullptr, &mDepthImageViews[i]));

            }

            VkSemaphoreCreateInfo semaphoreCreateInfo{};
            semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            TOF(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &mSemaphores[i]));

        }
    }

    mConstructed = true;

}


void Swapchain::destroy() noexcept {

    if (!mConstructed) return;

    mConstructed = false;

    for (uint32_t i = 0; i < mChainImages.size(); i++) {
        if (mChainImageViews[i]) vkDestroyImageView(mDevice, mChainImageViews[i], nullptr);

        if (mDepthImageViews[i]) vkDestroyImageView(mDevice, mDepthImageViews[i], nullptr);
        if (mDepthImageMemories[i]) vkFreeMemory(mDevice, mDepthImageMemories[i], nullptr);
        if (mDepthImages[i]) vkDestroyImage(mDevice, mDepthImages[i], nullptr);

        if (mSemaphores[i]) vkDestroySemaphore(mDevice, mSemaphores[i], nullptr);
    }

    if (mSwapchain) vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);

}


