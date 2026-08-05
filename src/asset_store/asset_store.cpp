

#include "asset_store.hpp"
#include "core.hpp"
#include "plugin_core.h"
#include "logger.hpp"
#include "file_registry.hpp"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/math.hpp>

#include <stdexcept>



AssetStore::AssetStore(Logger logger, FileRegistry& rFileReg, IGraphicsDevice& rHWLI) : mLogger(logger), mrFileReg(rFileReg), mrHWLI(rHWLI) {}


AssetStore::~AssetStore() noexcept {}


expected<TprMesh, TprResult> AssetStore::createMesh(const TprMeshCreateInfo* pInfo) noexcept {
    if (!pInfo) return unexpected(TPR_ERROR_INVALID_VALUE);

    try {
        TprResult result;
        result = mrFileReg.seek(pInfo->file, 0, TPR_SEEK_WHENCE_END);
        if (result != TPR_SUCCESS) return unexpected(result);
        auto tellExp = mrFileReg.tell(pInfo->file);
        if (!tellExp.has_value()) return unexpected(tellExp.error());
        auto size = tellExp.value();
        if (size == 0) {
            return unexpected(TPR_ERROR_INVALID_VALUE);
        }
        std::vector<std::byte> data(size);
        result = mrFileReg.readAt(pInfo->file, 0, data.size(), data.data());
        if (result != TPR_SUCCESS) return unexpected(result);

        auto gltfDataExp = fastgltf::GltfDataBuffer::FromBytes(data.data(), data.size());
        if (gltfDataExp.error() != fastgltf::Error::None) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "FastGLTF error: " << fastgltf::getErrorMessage(gltfDataExp.error()) << "\n";
            return unexpected(TPR_UNKNOWN_ERROR);
        }
        auto& gltfData = gltfDataExp.get();

        fastgltf::Parser gltfParser{};
        // TODO: add support for in-memory binary chunks
        auto gltfLibraryExp = gltfParser.loadGltfBinary(gltfData, "");
        if (gltfLibraryExp.error() != fastgltf::Error::None) {
            mLogger.error(TPR_LOG_STYLE_ERROR1) << "FastGLTF error: " << fastgltf::getErrorMessage(gltfLibraryExp.error()) << "\n";
            return unexpected(TPR_UNKNOWN_ERROR);
        }
        auto& gltfLibrary = gltfLibraryExp.get();

        if (pInfo->index >= gltfLibrary.meshes.size()) return unexpected(TPR_ERROR_INVALID_VALUE);
        const auto& meshData = gltfLibrary.meshes[pInfo->index];

        uint32_t totalPrimitiveCount = meshData.primitives.size();
        uint32_t totalIndexCount = 0;
        uint32_t totalVertexCount = 0;
        for (const auto& primitive : meshData.primitives) {
            uint32_t vertexCount = 0;
            for (const auto& attribute : primitive.attributes) {
                if (attribute.name == "POSITION") {
                    auto& accessor = gltfLibrary.accessors[attribute.accessorIndex];
                    totalVertexCount += accessor.count;
                    vertexCount = accessor.count;
                }
            }
            if (primitive.indicesAccessor.has_value()) {
                totalIndexCount += gltfLibrary.accessors[*primitive.indicesAccessor].count;
            } else {
                totalIndexCount += vertexCount;
            }
        }
        uint32_t primitiveChunkSize = totalPrimitiveCount * sizeof(AssetMesh::Primitive);
        uint32_t indexChunkSize = totalIndexCount * sizeof(AssetMesh::Index);
        uint32_t vertexChunkSize = totalVertexCount * sizeof(AssetMesh::Vertex);

        auto& mesh = mMeshes.try_emplace(mMeshCounter).first->second;
        mesh.data.resize(primitiveChunkSize + vertexChunkSize + indexChunkSize);
        mesh.header.primitiveCount = meshData.primitives.size();
        mesh.header.primitiveOffset = 0;
        mesh.header.indexCount = totalIndexCount;
        mesh.header.indexOffset = mesh.header.primitiveOffset + primitiveChunkSize;
        mesh.header.vertexCount = totalVertexCount;
        mesh.header.vertexOffset = mesh.header.indexOffset + indexChunkSize;

        uint32_t currentVertexOffset = mesh.header.vertexOffset;
        uint32_t currentIndexOffset = mesh.header.indexOffset;
        uint32_t currentPrimitiveOffset = mesh.header.primitiveOffset;
        for (const auto& primitive : meshData.primitives) {
            uint32_t vertexCount = 0;
            for (const auto& attribute : primitive.attributes) {
                if (attribute.name == "POSITION") {
                    const auto& accessor = gltfLibrary.accessors[attribute.accessorIndex];
                    vertexCount = accessor.count;
                    if (vertexCount > 0) {
                        auto* itEl = new (mesh.data.data() + currentVertexOffset) AssetMesh::Vertex[accessor.count];
                        auto* it = reinterpret_cast<std::byte*>(itEl);
                        for (uint32_t i = 0; i < accessor.count; i++, it += sizeof(AssetMesh::Vertex)) {
                            fastgltf::math::fvec3 pos = fastgltf::getAccessorElement<fastgltf::math::fvec3>(gltfLibrary, accessor, i);
                            std::memcpy(it + offsetof(AssetMesh::Vertex, x), &pos.x(), sizeof(float));
                            std::memcpy(it + offsetof(AssetMesh::Vertex, y), &pos.y(), sizeof(float));
                            std::memcpy(it + offsetof(AssetMesh::Vertex, z), &pos.z(), sizeof(float));
                        }
                    }
                }
            }
            if (vertexCount > 0) {
                auto* itEl = new (mesh.data.data() + currentPrimitiveOffset) AssetMesh::Primitive;
                auto* it = reinterpret_cast<std::byte*>(itEl);
                std::memcpy(it + offsetof(AssetMesh::Primitive, count), &vertexCount, sizeof(vertexCount));
                std::memcpy(it + offsetof(AssetMesh::Primitive, offset), &currentVertexOffset, sizeof(currentVertexOffset));
                currentVertexOffset += vertexCount * sizeof(AssetMesh::Vertex);
                currentPrimitiveOffset += sizeof(AssetMesh::Primitive);
                if (primitive.indicesAccessor.has_value()) {
                    auto& accessor = gltfLibrary.accessors[*primitive.indicesAccessor];
                    auto* itEl = new (mesh.data.data() + currentIndexOffset) AssetMesh::Index[accessor.count];
                    auto* it = reinterpret_cast<std::byte*>(itEl);
                    for (uint32_t i = 0; i < accessor.count; i++, it += sizeof(AssetMesh::Index)) {
                        AssetMesh::Index index = fastgltf::getAccessorElement<AssetMesh::Index>(gltfLibrary, accessor, i);
                        std::memcpy(it, &index, sizeof(index));
                    }
                    currentIndexOffset += accessor.count * sizeof(AssetMesh::Index);
                } else {
                    auto* itEl = new (mesh.data.data() + currentIndexOffset) AssetMesh::Index[vertexCount];
                    auto* it = reinterpret_cast<std::byte*>(itEl);
                    for (uint32_t i = 0; i < vertexCount; i++, it += sizeof(AssetMesh::Index)) {
                        std::memcpy(it, &i, sizeof(i));
                    }
                    currentIndexOffset += vertexCount * sizeof(AssetMesh::Index);
                }
            }
        }

        TprMesh handle = construct_basic_handle<TprMesh>(mMeshCounter, 0, handle_type::mesh);
        mesh.handle = handle;

        mMeshCounter++;

        return handle;
    
    } catch (const std::runtime_error& e) {
        mLogger.error(TPR_LOG_STYLE_ERROR1) << e.what() << "\n";
        return unexpected(TPR_UNKNOWN_ERROR);
    } catch (...) {
        return unexpected(TPR_UNKNOWN_ERROR);
    }

}


TprResult AssetStore::loadMesh(TprMesh handle, const TprMeshLoadInfo* pInfo) noexcept {
    if (!pInfo) return TPR_ERROR_INVALID_VALUE;
    try {
        if (get_basic_handle_type(handle) != handle_type::mesh) return TPR_ERROR_INVALID_VALUE;
        if (get_basic_handle_index(handle) > mMeshCounter) return TPR_ERROR_INVALID_VALUE;
        auto it = mMeshes.find(get_basic_handle_index(handle));
        if (it == mMeshes.end()) return TPR_ERROR_INVALID_VALUE;
        auto& mesh = it->second;
        if (mesh.loaded) return TPR_ERROR_INVALID_OPERATION;
        mesh.loaded = true;
        return mrHWLI.loadMesh(mesh);
    } catch (...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}


void AssetStore::unloadMesh(TprMesh handle) noexcept {
    try {
        if (get_basic_handle_type(handle) != handle_type::mesh) return;
        if (get_basic_handle_index(handle) > mMeshCounter) return;
        auto it = mMeshes.find(get_basic_handle_index(handle));
        if (it == mMeshes.end()) return;
        auto& mesh = it->second;
        if (!mesh.loaded) return;
        mesh.loaded = false;
        mrHWLI.unloadMesh(handle);
    } catch (...) {
        return;
    }
}


void AssetStore::destroyMesh(TprMesh handle) noexcept {
    try {
        if (get_basic_handle_type(handle) != handle_type::mesh) return;
        if (get_basic_handle_index(handle) > mMeshCounter) return;
        auto it = mMeshes.find(get_basic_handle_index(handle));
        if (it == mMeshes.end()) return;
        if (it->second.loaded) unloadMesh(handle);
        mMeshes.erase(it);
    } catch (...) {
        return;
    }
}

