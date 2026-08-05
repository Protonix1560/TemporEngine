
#include "core.hpp"
#include "backend.hpp"
#include "plugin_core.h"


Allocator VulkanBackend::createAllocator() {
    return Allocator(mLogger.derive("Allocator: "), mSym, mPhysicalDevice, mDevice);
}

expected<Buffer, TprResult> VulkanBackend::createBuffer(
    uint32_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags property,
    VkSharingMode sharingMode, std::span<uint32_t> queueFamilyIndices
) {
    if (!mAlloc.has_value()) return unexpected(TPR_UNKNOWN_ERROR);
    try {
        return Buffer(
            mLogger.derive("Buffer: "), mSym, mAlloc.value(), mPhysicalDevice,
            mDevice, size, usage, property, sharingMode, queueFamilyIndices
        );
    } catch (TprResult r) {
        return unexpected(r);
    }
}

expected<WindowContext, TprResult> VulkanBackend::createWindowContext(uint32_t queueFamilyIndex, TprWindow window) {
    if (!mAlloc.has_value()) return unexpected(TPR_UNKNOWN_ERROR);
    try {
        return WindowContext(
            mLogger.derive("WindowContext: "), mAlloc.value(), mrWinMan,
            mrFileReg, mSym, mInstance, mPhysicalDevice, mDevice, queueFamilyIndex,
            mMaxFramesInFlight, window, mBasicPipelinelayout, mObjectDataSetLayout
        );
    } catch (TprResult r) {
        return unexpected(r);
    }
}
