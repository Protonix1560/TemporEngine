
#include "vulkan_backend.hpp"

#include <algorithm>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>



void Swapchain::construct(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, const WindowManager* windowManager) {

    mConstructed = true;

    VkDevice oldDevice = mDevice;

    mDevice = device;
    mSurface = surface;

    {
        VkSurfaceCapabilitiesKHR surfaceCaps;
        TOF(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCaps));

        if (surfaceCaps.currentExtent.width == UINT32_MAX && surfaceCaps.currentExtent.height == UINT32_MAX) {
            mExtent = {windowManager->getWidth(), windowManager->getHeight()};
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

            mFormat = surfaceFormat.format;

            uint32_t imageCount = surfaceCaps.minImageCount + 1;
            if (surfaceCaps.maxImageCount != 0 && surfaceCaps.maxImageCount < imageCount) {
                imageCount = surfaceCaps.maxImageCount;
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
            createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;

            TOF(vkCreateSwapchainKHR(device, &createInfo, nullptr, &mSwapchain));

            if (oldSwapchain) {
                vkDestroySwapchainKHR(oldDevice, oldSwapchain, nullptr);
            }
        }

    }

    // cleanup if needed
    {
        for (uint32_t i = 0; i < mImages.size(); i++) {
            if (mImageViews[i]) vkDestroyImageView(oldDevice, mImageViews[i], nullptr);
            if (mSemaphores[i]) vkDestroySemaphore(oldDevice, mSemaphores[i], nullptr);
        }

    }

    if (mExtent.width != 0 && mExtent.height != 0) {

        TOF(vkGetSwapchainImagesKHR(device, mSwapchain, &mImageCount, nullptr));

        mImages.resize(mImageCount);
        mImageViews.assign(mImageCount, VK_NULL_HANDLE);
        mSemaphores.assign(mImageCount, VK_NULL_HANDLE);

        TOF(vkGetSwapchainImagesKHR(device, mSwapchain, &mImageCount, mImages.data()));

        for (uint32_t i = 0; i < mImageCount; i++) {

            VkImageViewCreateInfo viewCreateInfo{};
            viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewCreateInfo.format = mFormat;
            viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
            viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
            viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
            viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
            viewCreateInfo.image = mImages[i];
            viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewCreateInfo.subresourceRange.baseArrayLayer = 0;
            viewCreateInfo.subresourceRange.baseMipLevel = 0;
            viewCreateInfo.subresourceRange.layerCount = 1;
            viewCreateInfo.subresourceRange.levelCount = 1;
            
            TOF(vkCreateImageView(device, &viewCreateInfo, nullptr, &mImageViews[i]));

            VkSemaphoreCreateInfo semaphoreCreateInfo{};
            semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            TOF(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &mSemaphores[i]));

        }
    }

}


void Swapchain::destroy() noexcept {

    if (!mConstructed) return;

    mConstructed = false;

    for (uint32_t i = 0; i < mImages.size(); i++) {
        if (mImageViews[i]) vkDestroyImageView(mDevice, mImageViews[i], nullptr);
        if (mSemaphores[i]) vkDestroySemaphore(mDevice, mSemaphores[i], nullptr);
    }

    if (mSwapchain) vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);

}




void FramebuffersHolder::construct(Swapchain& swapchain, VkDevice device, VkRenderPass renderPass) {

    mDevice = device;

    mFramebuffers.assign(swapchain.imageCount(), VK_NULL_HANDLE);

    for (uint32_t i = 0; i < swapchain.imageCount(); i++) {

        VkFramebufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.width = swapchain.extent().width;
        createInfo.height = swapchain.extent().height;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &swapchain.getImageView(i);
        createInfo.layers = 1;
        createInfo.renderPass = renderPass;

        TOF(vkCreateFramebuffer(device, &createInfo, nullptr, &mFramebuffers[i]));
    
    }

}



void FramebuffersHolder::destroy() noexcept {

    for (const auto& framebuffer : mFramebuffers) {
        if (framebuffer) vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
    }

}


