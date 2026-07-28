
#ifndef ASSET_STORE_COMMON_HPP_
#define ASSET_STORE_COMMON_HPP_


#include "plugin_core.h"

#include <cstdint>
#include <vector>


struct AssetMesh {

    struct Primitive {
        uint32_t offset = 0;
        uint32_t count = 0;
    };

    struct Vertex {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    using Index = uint32_t;

    struct {
        uint32_t primitiveCount = 0;
        uint32_t primitiveOffset = 0;
        uint32_t indexCount = 0;
        uint32_t indexOffset = 0;
        uint32_t vertexCount = 0;
        uint32_t vertexOffset = 0;
    } header;
    std::vector<std::byte> data;

    bool loaded = false;
    TprMesh handle;
};


#endif  // ASSET_STORE_COMMON_HPP_
