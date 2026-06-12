
#include "asset_store/common.hpp"
#include "core.hpp"
#include "hardware_layer.hpp"
#include "interval_union.hpp"
#include "plugin_core.h"

#include <vulkan/vulkan.h>

#include <algorithm>


TprResult HardwareLayerVulkan::loadMesh(const AssetMesh& mesh) noexcept {

    TprResult tprResult;

    try {
        uint32_t meshIndexSize = mesh.header.indexCount * sizeof(uint32_t);
        uint32_t meshVertexSize = mesh.header.vertexCount * sizeof(Vertex);

        bool indexMemoryFound = false;
        interval<uint32_t> indexMemory;
        GeometryIndexBuffer* indexBuffer;
        uint32_t indexBufferId;
        for (auto& [id, buffer] : mIndexBuffers) {
            for (const auto& mem : buffer.free.data()) {
                if (mem.size() >= meshIndexSize) {
                    indexMemory = {mem.begin(), meshIndexSize};
                    buffer.free -= indexMemory;
                    indexBuffer = &buffer;
                    indexBufferId = id;
                    indexMemoryFound = true;
                }
            }
        }

        if (!indexMemoryFound) {
            uint32_t size = std::max(meshIndexSize, mGeometryBufferSize * static_cast<uint32_t>(sizeof(uint32_t)));
            auto exp = createBuffer(size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            if (!exp.has_value()) return exp.error();
            auto& buffer = mIndexBuffers.try_emplace(
                mGeoIndexBufferCount,
                // reserving meshIndicesSize bytes at the beginning
                std::move(exp.value()), interval_union<uint32_t>{interval<uint32_t>{meshIndexSize, size}}
            ).first->second;
            indexMemory = {0, meshIndexSize};
            indexBuffer = &buffer;
            indexBufferId = mGeoIndexBufferCount;
            mGeoIndexBufferCount++;
        }

        bool vertexMemoryFound = false;
        interval<uint32_t> vertexMemory;
        GeometryVertexBuffer* vertexBuffer;
        uint32_t vertexBufferId;
        for (auto& [id, buffer] : mVertexBuffers) {
            for (const auto& mem : buffer.free.data()) {
                if (mem.size() >= meshVertexSize) {
                    vertexMemory = {mem.begin(), meshVertexSize};
                    buffer.free -= vertexMemory;
                    vertexBuffer = &buffer;
                    vertexBufferId = id;
                    vertexMemoryFound = true;
                }
            }
        }

        if (!vertexMemoryFound) {
            uint32_t size = std::max(meshVertexSize, mGeometryBufferSize * static_cast<uint32_t>(sizeof(Vertex)));
            auto exp = createBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            if (!exp.has_value()) return exp.error();
            auto& buffer = mVertexBuffers.try_emplace(
                mGeoVertexBufferCount,
                // reserving meshVerticesSize bytes at the beginning
                std::move(exp.value()), interval_union<uint32_t>{interval<uint32_t>{meshVertexSize, size}}
            ).first->second;
            vertexMemory = {0, meshVertexSize};
            vertexBuffer = &buffer;
            vertexBufferId = mGeoVertexBufferCount;
            mGeoVertexBufferCount++;
        }

        tprResult = indexBuffer->indices.mapMemory();
        if (tprResult != TPR_SUCCESS) return tprResult;
        auto* indexData = reinterpret_cast<std::byte*>(indexBuffer->indices.mapping());
        for (uint32_t i = 0; i < mesh.header.indexCount; i++) {
            auto* data = mesh.data.data() + mesh.header.indexOffset + i * sizeof(uint32_t);
            std::memcpy(indexData + i * sizeof(uint32_t), data, sizeof(uint32_t));
        }
        indexBuffer->indices.unmapMemory();

        tprResult = vertexBuffer->positions.mapMemory();
        if (tprResult != TPR_SUCCESS) return tprResult;
        auto* positionData = reinterpret_cast<std::byte*>(vertexBuffer->positions.mapping());
        for (uint32_t i = 0; i < mesh.header.vertexCount; i++) {
            auto* data = mesh.data.data() + mesh.header.vertexOffset + i * sizeof(AssetMesh::Vertex);
            float x, y, z;
            std::memcpy(&x, data + offsetof(AssetMesh::Vertex, x), sizeof(float));
            std::memcpy(&y, data + offsetof(AssetMesh::Vertex, y), sizeof(float));
            std::memcpy(&z, data + offsetof(AssetMesh::Vertex, z), sizeof(float));
            VertexPosition pos{{x, y, z}};
            std::memcpy(positionData + i * sizeof(VertexPosition), &pos, sizeof(VertexPosition));
        }
        vertexBuffer->positions.unmapMemory();

        mMeshes.try_emplace(mesh.handle._d, indexBufferId, vertexBufferId, indexMemory, vertexMemory);
    
    } catch (...) {
        return TPR_UNKNOWN_ERROR;
    }

    return TPR_SUCCESS;
}


void HardwareLayerVulkan::unloadMesh(TprMesh handle) noexcept {
    try {
        auto it = mMeshes.find(handle._d);
        if (it == mMeshes.end()) return;
        auto& mesh = it->second;
        for (auto image : mesh.images) {
            destroyObjectImage(construct_basic_handle<TprObjectImage>(image, 0, handle_type::object_image));
        }
        auto& indexBuffer = mIndexBuffers.at(mesh.indexBuffer);
        indexBuffer.free += mesh.indicesInterval;
        if (
            indexBuffer.free.data().size() == 1 &&
            indexBuffer.free.data().back().end() == indexBuffer.indices.size() &&
            indexBuffer.free.data().back().begin() == 0
        ) {
            mIndexBuffers.erase(mIndexBuffers.find(mesh.indexBuffer));
        }
        auto& vertexBuffer = mVertexBuffers.at(mesh.vertexBuffer);
        vertexBuffer.free += mesh.verticesInterval;
        if (
            vertexBuffer.free.data().size() == 1 &&
            vertexBuffer.free.data().back().end() == vertexBuffer.positions.size() &&
            vertexBuffer.free.data().back().begin() == 0
        ) {
            mVertexBuffers.erase(mVertexBuffers.find(mesh.vertexBuffer));
        }
        mMeshes.erase(it);
    } catch (...) {
        return;
    }
}
