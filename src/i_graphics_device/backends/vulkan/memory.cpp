
#include "core.hpp"
#include "backend.hpp"
#include "plugin_core.h"
#include "log_entry.hpp"

#include <vulkan/vulkan_core.h>


expected<uint32_t, TprResult> VulkanBackend::findMemoryType(const VkMemoryRequirements& requirements, VkMemoryPropertyFlags property) {
    VkPhysicalDeviceMemoryProperties props;
    mLoader.vkGetPhysicalDeviceMemoryProperties()(mPhysicalDevice, &props);
    for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
        if ((requirements.memoryTypeBits & (1 << i)) && (props.memoryTypes[i].propertyFlags & property) == property) {
            return i;
        }
    }
    return unexpected(TPR_ERROR_DOESNT_EXIST);
}
expected<VulkanBackend::Allocation, TprResult> VulkanBackend::allocateMemory(const VkMemoryRequirements& requirements, VkMemoryPropertyFlags property, std::source_location loc) {
    auto memIndexExp = findMemoryType(requirements, property);
    if (!memIndexExp.has_value()) return unexpected(memIndexExp.error());
    VkDeviceMemory mem;
    // TODO: a better allocator than this
    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.memoryTypeIndex = memIndexExp.value();
    alloc.allocationSize = requirements.size;
    if (auto r = mLoader.vkAllocateMemory()(mDevice, &alloc, nullptr, &mem); r != VK_SUCCESS) {
        mLogger.error() << __FILE__ ": " << __LINE__ << ": vkAllocateMemory failed [" << r << "]";
        return unexpected(TPR_PANIC);
    }
    mLogger.trace() << current_file(loc, BACKEND_ROOT_DIR) << ": " << loc.line() << ": Allocated ordinary memory " << mem;
    return Allocation{mem, 0};
}
// NOTE: This function is supposed to be allocating a memory, exclusive to a single resource.
// this is helpful for mapping host-visible resources
expected<VulkanBackend::Allocation, TprResult> VulkanBackend::allocateExclusiveMemory(const VkMemoryRequirements& requirements, VkMemoryPropertyFlags property, std::source_location loc) {
    auto memIndexExp = findMemoryType(requirements, property);
    if (!memIndexExp.has_value()) return unexpected(memIndexExp.error());
    VkDeviceMemory mem;
    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.memoryTypeIndex = memIndexExp.value();
    alloc.allocationSize = requirements.size;
    if (auto r = mLoader.vkAllocateMemory()(mDevice, &alloc, nullptr, &mem); r != VK_SUCCESS) {
        mLogger.error() << __FILE__ ": " << __LINE__ << ": vkAllocateMemory failed [" << r << "]";
        return unexpected(TPR_PANIC);
    }
    mLogger.trace() << current_file(loc, BACKEND_ROOT_DIR) << ": " << loc.line() << ": Allocated exclusive memory " << mem;
    return Allocation{mem, 0};
}
void VulkanBackend::freeMemory(Allocation allocation, std::source_location loc) {
    mLoader.vkFreeMemory()(mDevice, allocation.memory, nullptr);
    mLogger.trace() << current_file(loc, BACKEND_ROOT_DIR) << ": " << loc.line() << ": Freed memory " << allocation.memory;
}

expected<VulkanBackend::PartialBuffer, TprResult> VulkanBackend::createPartialBuffer(uint64_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags property, std::source_location loc) {
    PartialBuffer buffer{};
    if (size == 0) return std::move(buffer);
    buffer.byteSize = size;
    buffer.free = interval<uint64_t>{0, size};
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    if (auto r = mLoader.vkCreateBuffer()(mDevice, &bufferInfo, nullptr, &buffer.buffer); r != VK_SUCCESS) {
        mLogger.error() << __FILE__ ": " << __LINE__ << ": vkCreateBuffer failed [" << r << "]";
        return unexpected(TPR_PANIC);
    }
    VkMemoryRequirements memReq;
    mLoader.vkGetBufferMemoryRequirements()(mDevice, buffer.buffer, &memReq);
    auto allocExp = allocateMemory(memReq, property);
    if (!allocExp.has_value()) return unexpected(allocExp.error());
    buffer.alloc = allocExp.value();
    if (auto r = mLoader.vkBindBufferMemory()(mDevice, buffer.buffer, buffer.alloc.memory, buffer.alloc.offset); r != VK_SUCCESS) {
        mLogger.error() << __FILE__ ": " << __LINE__ << ": vkBindBufferMemory failed [" << r << "]";
        return unexpected(TPR_PANIC);
    }
    mLogger.trace() << current_file(loc, BACKEND_ROOT_DIR) << ": " << loc.line() << ": Created ordinary partial buffer " << buffer.buffer;
    return std::move(buffer);
}
expected<VulkanBackend::PartialBuffer, TprResult> VulkanBackend::createExclusivePartialBuffer(uint64_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags property, std::source_location loc) {
    PartialBuffer buffer{};
    if (size == 0) return std::move(buffer);
    buffer.byteSize = size;
    buffer.free = interval<uint64_t>{0, size};
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    if (auto r = mLoader.vkCreateBuffer()(mDevice, &bufferInfo, nullptr, &buffer.buffer); r != VK_SUCCESS) {
        mLogger.error() << __FILE__ ": " << __LINE__ << ": vkCreateBuffer failed [" << r << "]";
        return unexpected(TPR_PANIC);
    }
    VkMemoryRequirements memReq;
    mLoader.vkGetBufferMemoryRequirements()(mDevice, buffer.buffer, &memReq);
    auto allocExp = allocateExclusiveMemory(memReq, property);
    if (!allocExp.has_value()) return unexpected(allocExp.error());
    buffer.alloc = allocExp.value();
    if (auto r = mLoader.vkBindBufferMemory()(mDevice, buffer.buffer, buffer.alloc.memory, buffer.alloc.offset); r != VK_SUCCESS) {
        mLogger.error() << __FILE__ ": " << __LINE__ << ": vkBindBufferMemory failed [" << r << "]";
        return unexpected(TPR_PANIC);
    }
    mLogger.trace() << current_file(loc, BACKEND_ROOT_DIR) << ": " << loc.line() << ": Created exclusive partial buffer " << buffer.buffer;
    return std::move(buffer);
}
void VulkanBackend::freePartialBuffer(PartialBuffer& buffer, std::source_location loc) {
    if (buffer.byteSize == 0) return;
    if (buffer.buffer == VK_NULL_HANDLE) return;
    mLoader.vkDestroyBuffer()(mDevice, buffer.buffer, nullptr);
    buffer.buffer = VK_NULL_HANDLE;
    freeMemory(buffer.alloc);
    mLogger.trace() << current_file(loc, BACKEND_ROOT_DIR) << ": " << loc.line() << ": Freed partial buffer " << buffer.buffer;
}

expected<VulkanBackend::FullBuffer, TprResult> VulkanBackend::createFullBuffer(uint64_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags property, std::source_location loc) {
    FullBuffer buffer{};
    if (size == 0) return std::move(buffer);
    buffer.byteSize = size;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    if (auto r = mLoader.vkCreateBuffer()(mDevice, &bufferInfo, nullptr, &buffer.buffer); r != VK_SUCCESS) {
        mLogger.error() << __FILE__ ": " << __LINE__ << ": vkCreateBuffer failed [" << r << "]";
        return unexpected(TPR_PANIC);
    }
    VkMemoryRequirements memReq;
    mLoader.vkGetBufferMemoryRequirements()(mDevice, buffer.buffer, &memReq);
    auto allocExp = allocateMemory(memReq, property);
    if (!allocExp.has_value()) return unexpected(allocExp.error());
    buffer.alloc = allocExp.value();
    if (auto r = mLoader.vkBindBufferMemory()(mDevice, buffer.buffer, buffer.alloc.memory, buffer.alloc.offset); r != VK_SUCCESS) {
        mLogger.error() << __FILE__ ": " << __LINE__ << ": vkBindBufferMemory failed [" << r << "]";
        return unexpected(TPR_PANIC);
    }
    mLogger.trace() << current_file(loc, BACKEND_ROOT_DIR) << ": " << loc.line() << ": Created ordinary full buffer " << buffer.buffer;
    return std::move(buffer);
}
expected<VulkanBackend::FullBuffer, TprResult> VulkanBackend::createExclusiveFullBuffer(uint64_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags property, std::source_location loc) {
    FullBuffer buffer{};
    if (size == 0) return std::move(buffer);
    buffer.byteSize = size;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    if (auto r = mLoader.vkCreateBuffer()(mDevice, &bufferInfo, nullptr, &buffer.buffer); r != VK_SUCCESS) {
        mLogger.error() << __FILE__ ": " << __LINE__ << ": vkCreateBuffer failed [" << r << "]";
        return unexpected(TPR_PANIC);
    }
    VkMemoryRequirements memReq;
    mLoader.vkGetBufferMemoryRequirements()(mDevice, buffer.buffer, &memReq);
    auto allocExp = allocateExclusiveMemory(memReq, property);
    if (!allocExp.has_value()) return unexpected(allocExp.error());
    buffer.alloc = allocExp.value();
    if (auto r = mLoader.vkBindBufferMemory()(mDevice, buffer.buffer, buffer.alloc.memory, buffer.alloc.offset); r != VK_SUCCESS) {
        mLogger.error() << __FILE__ ": " << __LINE__ << ": vkBindBufferMemory failed [" << r << "]";
        return unexpected(TPR_PANIC);
    }
    mLogger.trace() << current_file(loc, BACKEND_ROOT_DIR) << ": " << loc.line() << ": Created exclusive full buffer " << buffer.buffer;
    return std::move(buffer);
}
void VulkanBackend::freeFullBuffer(FullBuffer& buffer, std::source_location loc) {
    if (buffer.byteSize == 0) return;
    if (buffer.buffer == VK_NULL_HANDLE) return;
    mLoader.vkDestroyBuffer()(mDevice, buffer.buffer, nullptr);
    buffer.buffer = VK_NULL_HANDLE;
    freeMemory(buffer.alloc);
    mLogger.trace() << current_file(loc, BACKEND_ROOT_DIR) << ": " << loc.line() << ": Freed full buffer " << buffer.buffer;
}
