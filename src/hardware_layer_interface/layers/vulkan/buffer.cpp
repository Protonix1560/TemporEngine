
#include "core.hpp"
#include "hardware_layer.hpp"
#include "plugin_core.h"
#include <vulkan/vulkan_core.h>


Buffer::Buffer(
    Logger& rLogger, VulkanSymbols& rSym, Allocator& rAlloc, VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize size,
    VkBufferUsageFlags usage, VkMemoryPropertyFlags property, VkSharingMode sharingMode, std::span<uint32_t> queueFamilyIndices
) : BufferResources{
    .mrLogger = rLogger, .mrSym = rSym, .mrAlloc = rAlloc, .mPhysicalDevice = physicalDevice, .mDevice = device, .mSize = size,
    .mUsage = usage, .mProperty = property, .mSharing = sharingMode, .mQueueFamilyIndices = {queueFamilyIndices.begin(), queueFamilyIndices.end()}
} {

    VkResult result;

    if (size == 0) return;

    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = mSize;
    createInfo.usage = mUsage;
    createInfo.sharingMode = mSharing;
    createInfo.pQueueFamilyIndices = mQueueFamilyIndices.data();
    createInfo.queueFamilyIndexCount = mQueueFamilyIndices.size();
    
    result = mrSym.vkCreateBuffer(mDevice, &createInfo, nullptr, &mBuffer);
    if (result != VK_SUCCESS) {
        mrLogger.error(TPR_LOG_STYLE_ERROR1) << "Buffer: vkCreateBuffer failed [" << result << "]\n";
        throw TPR_UNKNOWN_ERROR;
    }

    VkMemoryRequirements memReq{};
    mrSym.vkGetBufferMemoryRequirements(mDevice, mBuffer, &memReq);

    auto allocExp = mrAlloc.allocate(memReq, mProperty);
    if (!allocExp.has_value()) throw allocExp.error();
    mAllocation = allocExp.value();

    result = mrSym.vkBindBufferMemory(mDevice, mBuffer, mAllocation.memory, mAllocation.offset);
    if (result != VK_SUCCESS) {
        mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "Buffer: vkBindBufferMemory failed [" << result << "]\n";
        throw TPR_UNKNOWN_ERROR;
    }
}


Buffer::~Buffer() noexcept {
    if (!freeResources) return;
    unmapMemory();
    if (mSize == 0) return;
    if (mDevice) mrSym.vkDestroyBuffer(mDevice, mBuffer, nullptr);
    mrAlloc.free(mAllocation);
}


TprResult Buffer::mapMemory() {
    if (mSize == 0) return TPR_BAD_ALLOC;
    auto mapExp = mrAlloc.map(mAllocation);
    if (!mapExp.has_value()) return mapExp.error();
    mMap = mapExp.value();
    return TPR_SUCCESS;
}


void Buffer::unmapMemory() noexcept {
    mrAlloc.unmap(mAllocation);
}

VkBuffer Buffer::handle() const {
    return mBuffer;
}


TprResult Buffer::reallocate(VkDeviceSize newSize) {
    if (newSize == mSize) return TPR_SUCCESS;
    VkResult result;
    mSize = newSize;
    bool wasMapped = (mMap != nullptr);
    
    unmapMemory();
    if (mDevice) mrSym.vkDestroyBuffer(mDevice, mBuffer, nullptr);
    mrAlloc.free(mAllocation);

    if (newSize == 0) return TPR_SUCCESS;

    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = mSize;
    createInfo.usage = mUsage;
    createInfo.sharingMode = mSharing;
    createInfo.pQueueFamilyIndices = mQueueFamilyIndices.data();
    createInfo.queueFamilyIndexCount = mQueueFamilyIndices.size();
    
    result = mrSym.vkCreateBuffer(mDevice, &createInfo, nullptr, &mBuffer);
    if (result != VK_SUCCESS) {
        mrLogger.error(TPR_LOG_STYLE_ERROR1) << "Buffer: vkCreateBuffer failed [" << result << "]\n";
        throw TPR_UNKNOWN_ERROR;
    }

    VkMemoryRequirements memReq{};
    mrSym.vkGetBufferMemoryRequirements(mDevice, mBuffer, &memReq);

    auto allocExp = mrAlloc.allocate(memReq, mProperty);
    if (!allocExp.has_value()) throw allocExp.error();
    mAllocation = allocExp.value();

    result = mrSym.vkBindBufferMemory(mDevice, mBuffer, mAllocation.memory, mAllocation.offset);
    if (result != VK_SUCCESS) {
        mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "Buffer: vkBindBufferMemory failed [" << result << "]\n";
        return TPR_UNKNOWN_ERROR;
    }
    
    if (wasMapped) mapMemory();
    return TPR_SUCCESS;
}
