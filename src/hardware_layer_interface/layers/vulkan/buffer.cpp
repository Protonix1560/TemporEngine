
#include "core.hpp"
#include "hardware_layer.hpp"
#include "plugin_core.h"
#include <vulkan/vulkan_core.h>


TprResult HardwareLayerVulkan::allocateBuffer(
    Buffer& buffer, uint32_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags property,
    VkSharingMode sharingMode, const uint32_t* pQueueFamilyIndices, uint32_t queueFamilyIndexCount
) {

    VkResult result;

    buffer.size = size;
    buffer.usage = usage;
    buffer.property = property;
    buffer.sharingMode = sharingMode;
    buffer.queueFamilyIndices.clear();
    buffer.queueFamilyIndices.reserve(queueFamilyIndexCount);
    buffer.queueFamilyIndices.insert(buffer.queueFamilyIndices.end(), pQueueFamilyIndices, pQueueFamilyIndices + queueFamilyIndexCount);

    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = buffer.size;
    createInfo.usage = buffer.usage;
    createInfo.sharingMode = buffer.sharingMode;
    createInfo.pQueueFamilyIndices = buffer.queueFamilyIndices.data();
    createInfo.queueFamilyIndexCount = buffer.queueFamilyIndices.size();
    
    result = mSym.vkCreateBuffer(mDevice, &createInfo, nullptr, &buffer.buffer);
    if (result != VK_SUCCESS) {
        mrLogger.error(TPR_LOG_STYLE_ERROR1) << "vkCreateBuffer failed [" << result << "]\n";
        return TPR_UNKNOWN_ERROR;
    }

    VkMemoryRequirements memReq{};
    mSym.vkGetBufferMemoryRequirements(mDevice, buffer.buffer, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, buffer.property).value();

    result = mSym.vkAllocateMemory(mDevice, &allocInfo, nullptr, &buffer.memory);
    if (result != VK_SUCCESS) {
        mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "allocateBuffer: vkAllocateMemory failed [" << result << "]\n";
        return TPR_UNKNOWN_ERROR;
    }
    result = mSym.vkBindBufferMemory(mDevice, buffer.buffer, buffer.memory, 0);
    if (result != VK_SUCCESS) {
        mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "allocateBuffer: vkBindBufferMemory failed [" << result << "]\n";
        return TPR_UNKNOWN_ERROR;
    }

    return TPR_SUCCESS;
}


TprResult HardwareLayerVulkan::mapBufferMemory(Buffer& buffer, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags) {
    buffer.mapOffset = offset;
    buffer.mapSize = size;
    buffer.mapFlags = flags;
    VkResult result = mSym.vkMapMemory(mDevice, buffer.memory, offset, size, flags, &buffer.map);
    if (result != VK_SUCCESS) {
        mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "mapBufferMemory: vkMapMemory failed [" << result << "]\n";
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}


void HardwareLayerVulkan::unmapBufferMemory(Buffer& buffer) noexcept {
    if (buffer.memory) {
        mSym.vkUnmapMemory(mDevice, buffer.memory);
        buffer.map = nullptr;
    }
}


void HardwareLayerVulkan::freeBuffer(Buffer& buffer) noexcept {
    unmapBufferMemory(buffer);
    if (buffer.memory) mSym.vkFreeMemory(mDevice, buffer.memory, nullptr);
    if (mDevice) mSym.vkDestroyBuffer(mDevice, buffer.buffer, nullptr);
}


TprResult HardwareLayerVulkan::resizeBuffer(Buffer& buffer, uint32_t newSize) {
    if (newSize == buffer.size) return TPR_SUCCESS;
    TprResult result;
    buffer.size = newSize;
    bool wasMapped = (buffer.map != nullptr);
    freeBuffer(buffer);
    result = allocateBuffer(
        buffer, buffer.size, buffer.usage, buffer.property, buffer.sharingMode,
        buffer.queueFamilyIndices.data(), buffer.queueFamilyIndices.size()
    );
    if (result < 0) return result;
    if (wasMapped) {
        result = mapBufferMemory(buffer, buffer.mapOffset, buffer.mapSize, buffer.mapFlags);
        if (result < 0) return result;
    }
    return TPR_SUCCESS;
}
