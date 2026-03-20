
#include "core.hpp"
#include "hardware_layer.hpp"
#include "plugin_core.h"

expected<uint32_t, TprResult> HardwareLayerVulkan::findMemoryType(uint32_t memType, VkMemoryPropertyFlags property) {
    VkPhysicalDeviceMemoryProperties memProperties;
    mSym.vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memType & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & property) == property) {
            return i;
        }
    }

    mrLogger.error(TPR_LOG_STYLE_ERROR1) << logPrxPHWL() << "Failed to find sufficient physical device memory type\n";
    return unexpected(TPR_UNKNOWN_ERROR);
}
