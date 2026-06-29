
#include "core.hpp"
#include "hardware_layer.hpp"
#include "logger.hpp"
#include "plugin_core.h"

#include <vulkan/vulkan_core.h>

#include <cstring>


Allocator::Allocator(Logger logger, VulkanSymbols& rSym, VkPhysicalDevice physicalDevice, VkDevice device)
    : mLogger(logger), mrSym(rSym), mPhysicalDevice(physicalDevice), mDevice(device) {

    VkPhysicalDeviceMemoryProperties memProps;
    mrSym.vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProps);

    mMemoryTypes.reserve(memProps.memoryTypeCount);
    mMemoryTypes.resize(memProps.memoryTypeCount);
    std::memcpy(mMemoryTypes.data(), memProps.memoryTypes, memProps.memoryTypeCount * sizeof(VkMemoryType));

    VkPhysicalDeviceProperties props;
    mrSym.vkGetPhysicalDeviceProperties(mPhysicalDevice, &props);

    mMaxAllocCount = props.limits.maxMemoryAllocationCount;
}


expected<uint32_t, TprResult> Allocator::findMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags property) {
    for (uint32_t i = 0; i < mMemoryTypes.size(); i++) {
        if ((memoryTypeBits & (1 << i)) && (mMemoryTypes[i].propertyFlags & property) == property) return i;
    }
    mLogger() << "Allocator: Failed to find suitable memory type for memoryTypeBits=" << memoryTypeBits << ", memoryPropertyFlags=" << property << "\n";
    return unexpected(TPR_BAD_ALLOC);
}


expected<Allocation, TprResult> Allocator::allocate(VkMemoryRequirements requirements, VkMemoryPropertyFlags property) {
    assert(requirements.size != 0);
    if (mAllocCount >= mMaxAllocCount) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << "Allocator: Exceeded max allocation count of " << mMaxAllocCount << "\n";
        return unexpected(TPR_BAD_ALLOC);
    }
    auto exp = findMemoryType(requirements.memoryTypeBits, property);
    if (!exp.has_value()) return unexpected(exp.error());
    VkMemoryAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.memoryTypeIndex = exp.value();
    info.allocationSize = requirements.size;
    VkDeviceMemory memory;
    VkResult r = mrSym.vkAllocateMemory(mDevice, &info, nullptr, &memory);
    if (r == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << "Allocator: Device out of memory\n";
        return unexpected(TPR_BAD_ALLOC);
    }
    if (r != VK_SUCCESS) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << "Allocator: vkAllocateMemory failed [" << r << "]\n";
        return unexpected(TPR_BAD_ALLOC);
    }
    mAllocCount++;
    mMemories.try_emplace(memory);
    return Allocation{memory, 0};
}


void Allocator::free(Allocation allocation) noexcept {
    try {
        auto it = mMemories.find(allocation.memory);
        if (it != mMemories.end()) {
            mrSym.vkFreeMemory(mDevice, allocation.memory, nullptr);
            mMemories.erase(it);
            mAllocCount--;
        }
    } catch (...) {}
}


expected<void*, TprResult> Allocator::map(Allocation alloc) {
    auto it = mMemories.find(alloc.memory);
    if (it != mMemories.end()) {
        auto& memory = it->second;
        VkResult r = mrSym.vkMapMemory(mDevice, alloc.memory, 0, VK_WHOLE_SIZE, 0, &memory.map);
        if (r != VK_SUCCESS) return unexpected(TPR_UNKNOWN_ERROR);
        return memory.map;
    }
    return unexpected(TPR_ERROR_INVALID_VALUE);
}


bool Allocator::mapped(Allocation alloc) const noexcept {
    try {
        auto it = mMemories.find(alloc.memory);
        if (it != mMemories.end()) {
            auto& memory = it->second;
            return memory.map != nullptr;
        }
    } catch (...) {}
    return false;
}


void Allocator::unmap(Allocation alloc) noexcept {
    try {
        auto it = mMemories.find(alloc.memory);
        if (it != mMemories.end()) {
            auto& memory = it->second;
            if (memory.map) mrSym.vkUnmapMemory(mDevice, alloc.memory);
            memory.map = nullptr;
        }
    } catch (...) {}
}

