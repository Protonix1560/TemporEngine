

#ifndef HARDWARE_LAYER_INTERFACE_HARDWARE_COMMON_STRUCTS_HPP_
#define HARDWARE_LAYER_INTERFACE_HARDWARE_COMMON_STRUCTS_HPP_


#include <glm/glm.hpp>


enum class GraphicsBackend {
    None = 0,
    Unknown = 1,
    Vulkan = 2
};

constexpr const char* graphicsBackendName[] = {
    "None",
    "Unknown",
    "Vulkan"
};


#endif  // HARDWARE_LAYER_INTERFACE_HARDWARE_COMMON_STRUCTS_HPP_
