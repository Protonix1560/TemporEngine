
#include "hardware_layer.hpp"
#include "plugin_core.h"
#include <vulkan/vulkan_core.h>


TprResult HardwareLayerVulkan::constructFrame(Frame& frame, uint32_t queueFamilyIndex) {

    VkResult result;

    VkSemaphoreCreateInfo imageAvailableSemaphoreCreateInfo{};
    imageAvailableSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    result = mSym.vkCreateSemaphore(mDevice, &imageAvailableSemaphoreCreateInfo, nullptr, &frame.imageAvailableSemaphore);
    if (result != VK_SUCCESS) return TPR_UNKNOWN_ERROR;

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    result = mSym.vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &frame.inFlightFence);
    if (result != VK_SUCCESS) return TPR_UNKNOWN_ERROR;

    VkCommandPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.queueFamilyIndex = queueFamilyIndex;
    result = mSym.vkCreateCommandPool(mDevice, &poolCreateInfo, nullptr, &frame.commandPool);
    if (result != VK_SUCCESS) return TPR_UNKNOWN_ERROR;

    VkCommandBufferAllocateInfo commandAllocInfo{};
    commandAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandAllocInfo.commandBufferCount = std::size(frame.commandBuffers);
    commandAllocInfo.commandPool = frame.commandPool;
    commandAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    result = mSym.vkAllocateCommandBuffers(mDevice, &commandAllocInfo, frame.commandBuffers);
    if (result != VK_SUCCESS) return TPR_UNKNOWN_ERROR;

    return TPR_SUCCESS;
}


void HardwareLayerVulkan::destroyFrame(Frame& frame) noexcept {
    if (frame.commandPool) {
        mSym.vkDestroyCommandPool(mDevice, frame.commandPool, nullptr);
        frame.commandPool = VK_NULL_HANDLE;
    }

    if (frame.inFlightFence) {
        mSym.vkDestroyFence(mDevice, frame.inFlightFence, nullptr);
        frame.inFlightFence = VK_NULL_HANDLE;
    }

    if (frame.imageAvailableSemaphore) {
        mSym.vkDestroySemaphore(mDevice, frame.imageAvailableSemaphore, nullptr);
        frame.imageAvailableSemaphore = VK_NULL_HANDLE;
    }
}
