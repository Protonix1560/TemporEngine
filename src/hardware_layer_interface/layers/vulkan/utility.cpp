
#include "core.hpp"
#include "hardware_layer.hpp"
#include "plugin_core.h"


Allocator HardwareLayerVulkan::createAllocator() {
    return Allocator(mrLogger, mSym, mPhysicalDevice, mDevice);
}

expected<Buffer, TprResult> HardwareLayerVulkan::createBuffer(
    uint32_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags property,
    VkSharingMode sharingMode, std::span<uint32_t> queueFamilyIndices
) {
    if (!mAlloc.has_value()) return unexpected(TPR_UNKNOWN_ERROR);
    try {
        return Buffer(
            mrLogger, mSym, mAlloc.value(), mPhysicalDevice,
            mDevice, size, usage, property, sharingMode, queueFamilyIndices
        );
    } catch (TprResult r) {
        return unexpected(r);
    }
}

expected<WindowContext, TprResult> HardwareLayerVulkan::createWindowContext(uint32_t queueFamilyIndex, TprWindow window) {
    if (!mAlloc.has_value()) return unexpected(TPR_UNKNOWN_ERROR);
    try {
        return WindowContext(
            mrLogger, mAlloc.value(), mrWinMan, mrResReg, mSym,
            mInstance, mPhysicalDevice, mDevice, queueFamilyIndex,
            mMaxFramesInFlight, window, mBasicPipelinelayout,
            mObjectDataSetLayout
        );
    } catch (TprResult r) {
        return unexpected(r);
    }
}
