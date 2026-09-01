

#include "asset_store.hpp"
#include "asset_store_common.hpp"
#include "core.hpp"
#include "i_graphics_device.hpp"
#include "plugin_core.h"
#include "logger.hpp"
#include "file_registry.hpp"
#include "log_entry.hpp"
#include "scope_guard.hpp"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/math.hpp>

#include <mutex>
#include <stdexcept>



AssetStore::AssetStore(Logger logger, FileRegistry& rFileReg, std::atomic<TprResult>& rRunResult)
    : mLogger(logger), mrFileReg(rFileReg), mrRunResult(rRunResult) {}

TprResult AssetStore::init(IGraphicsDevice* pGDev) {
    std::lock_guard<std::mutex> lock(mMutex);
    assert(!mInitialised);
    mpGDev = pGDev;
    mInitialised = true;
    return TPR_SUCCESS;
}

AssetStore::~AssetStore() noexcept {}


expected<TprMesh, TprResult> AssetStore::createMesh(const TprMeshCreateInfo& info) noexcept {
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        if (auto r = mrFileReg.seek(info.data, 0, TPR_SEEK_WHENCE_END); r != TPR_SUCCESS) return unexpected(r);
        auto tellExp = mrFileReg.tell(info.data);
        if (!tellExp.has_value()) return unexpected(tellExp.error());
        auto size = tellExp.value();
        if (size == 0) { return unexpected(TPR_ERROR_INVALID_VALUE); }
        std::vector<std::byte> data(size);
        if (auto r = mrFileReg.readAt(info.data, 0, data.size(), data.data()); r != TPR_SUCCESS) return unexpected(r);

        auto gltfDataExp = fastgltf::GltfDataBuffer::FromBytes(data.data(), data.size());
        if (gltfDataExp.error() != fastgltf::Error::None) {
            mLogger.panic() << "fastgltf::GltfDataBuffer::FromBytes failed: " << fastgltf::getErrorMessage(gltfDataExp.error());
            mrRunResult.store(TPR_PANIC);
            return unexpected(TPR_PANIC);
        }
        auto& gltfData = gltfDataExp.get();

        fastgltf::Parser gltfParser{};
        // TODO: add support for in-memory binary chunks
        auto gltfLibraryExp = gltfParser.loadGltfBinary(gltfData, "");
        if (gltfLibraryExp.error() != fastgltf::Error::None) {
            mLogger.panic() << "fastgltf::Parser::loadGltfBinary failed: " << fastgltf::getErrorMessage(gltfLibraryExp.error());
            mrRunResult.store(TPR_PANIC);
            return unexpected(TPR_PANIC);
        }
        auto& gltfLibrary = gltfLibraryExp.get();

        if (info.index >= gltfLibrary.meshes.size()) return unexpected(TPR_ERROR_INVALID_VALUE);
        const auto& meshData = gltfLibrary.meshes[info.index];

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
        uint32_t primitiveChunkSize = totalPrimitiveCount * sizeof(MeshData::Primitive);
        uint32_t indexChunkSize = totalIndexCount * sizeof(MeshData::Index);
        uint32_t vertexChunkSize = totalVertexCount * sizeof(MeshData::Vertex);

        auto& handle = mMeshes.insert_or_assign(mMeshHandleCounter, MeshHandle{.entry = std::make_shared<MeshEntry>()}).first->second;
        auto& entry = *handle.entry.get();
        entry.data.data.resize(primitiveChunkSize + vertexChunkSize + indexChunkSize);
        entry.data.header.primitiveCount = meshData.primitives.size();
        entry.data.header.primitiveOffset = 0;
        entry.data.header.indexCount = totalIndexCount;
        entry.data.header.indexOffset = entry.data.header.primitiveOffset + primitiveChunkSize;
        entry.data.header.vertexCount = totalVertexCount;
        entry.data.header.vertexOffset = entry.data.header.indexOffset + indexChunkSize;

        uint32_t currentVertexOffset = entry.data.header.vertexOffset;
        uint32_t currentIndexOffset = entry.data.header.indexOffset;
        uint32_t currentPrimitiveOffset = entry.data.header.primitiveOffset;
        for (const auto& primitive : meshData.primitives) {
            uint32_t vertexCount = 0;
            for (const auto& attribute : primitive.attributes) {
                if (attribute.name == "POSITION") {
                    const auto& accessor = gltfLibrary.accessors[attribute.accessorIndex];
                    vertexCount = accessor.count;
                    if (vertexCount > 0) {
                        auto* itEl = new (entry.data.data.data() + currentVertexOffset) MeshData::Vertex[accessor.count];
                        auto* it = reinterpret_cast<std::byte*>(itEl);
                        for (uint32_t i = 0; i < accessor.count; i++, it += sizeof(MeshData::Vertex)) {
                            fastgltf::math::fvec3 pos = fastgltf::getAccessorElement<fastgltf::math::fvec3>(gltfLibrary, accessor, i);
                            std::memcpy(it + offsetof(MeshData::Vertex, x), &pos.x(), sizeof(float));
                            std::memcpy(it + offsetof(MeshData::Vertex, y), &pos.y(), sizeof(float));
                            std::memcpy(it + offsetof(MeshData::Vertex, z), &pos.z(), sizeof(float));
                        }
                    }
                }
            }
            if (vertexCount > 0) {
                auto* itEl = new (entry.data.data.data() + currentPrimitiveOffset) MeshData::Primitive;
                auto* it = reinterpret_cast<std::byte*>(itEl);
                std::memcpy(it + offsetof(MeshData::Primitive, count), &vertexCount, sizeof(vertexCount));
                std::memcpy(it + offsetof(MeshData::Primitive, offset), &currentVertexOffset, sizeof(currentVertexOffset));
                currentVertexOffset += vertexCount * sizeof(MeshData::Vertex);
                currentPrimitiveOffset += sizeof(MeshData::Primitive);
                if (primitive.indicesAccessor.has_value()) {
                    auto& accessor = gltfLibrary.accessors[*primitive.indicesAccessor];
                    auto* itEl = new (entry.data.data.data() + currentIndexOffset) MeshData::Index[accessor.count];
                    auto* it = reinterpret_cast<std::byte*>(itEl);
                    for (uint32_t i = 0; i < accessor.count; i++, it += sizeof(MeshData::Index)) {
                        MeshData::Index index = fastgltf::getAccessorElement<MeshData::Index>(gltfLibrary, accessor, i);
                        std::memcpy(it, &index, sizeof(index));
                    }
                    currentIndexOffset += accessor.count * sizeof(MeshData::Index);
                } else {
                    auto* itEl = new (entry.data.data.data() + currentIndexOffset) MeshData::Index[vertexCount];
                    auto* it = reinterpret_cast<std::byte*>(itEl);
                    for (uint32_t i = 0; i < vertexCount; i++, it += sizeof(MeshData::Index)) {
                        std::memcpy(it, &i, sizeof(i));
                    }
                    currentIndexOffset += vertexCount * sizeof(MeshData::Index);
                }
            }
        }

        entry.id = MeshIdentity(mMeshEntryCounter);
        mMeshEntryMap.insert_or_assign(entry.id, handle.entry);
        mMeshEntryCounter++;

        mLogger.debug() << "Constructed Mesh " << mMeshHandleCounter;
        TprMesh h = construct_basic_handle<TprMesh>(mMeshHandleCounter, 0, handle_type::mesh);
        mMeshHandleCounter++;
        return h;
    
    } catch (const std::runtime_error& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }

}

expected<TprMesh, TprResult> AssetStore::createMeshCapability(TprMesh mesh, TprMeshCapabilityFlags mask) noexcept {
    if (get_basic_handle_type(mesh) != handle_type::mesh) return unexpected(TPR_ERROR_INVALID_VALUE);
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        auto it = mMeshes.find(get_basic_handle_index(mesh));
        if (it == mMeshes.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        mMeshes.insert_or_assign(mMeshHandleCounter, MeshHandle{it->second.capability & mask, it->second.entry});
        mLogger.debug() << "Created Mesh capability " << mMeshHandleCounter;
        TprMesh h = construct_basic_handle<TprMesh>(mMeshHandleCounter, 0, handle_type::mesh);
        mMeshHandleCounter++;
        return h;

    } catch (const std::runtime_error& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return unexpected(TPR_PANIC);
    }
}

void AssetStore::destroyMesh(TprMesh mesh) noexcept {
    if (get_basic_handle_type(mesh) != handle_type::mesh) return;
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        if (get_basic_handle_index(mesh) > mMeshHandleCounter) return;
        auto it = mMeshes.find(get_basic_handle_index(mesh));
        if (it == mMeshes.end()) return;
        auto entry = it->second.entry;
        mMeshes.erase(it);
        if (entry.use_count() <= 2) {
            // Uses must be here in `entry` and in mMeshEntryMap
            mMeshEntryMap.erase(entry->id);
            unloadMesh(*entry.get());
        }

    } catch (const std::runtime_error& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return;
    }
}


TprResult AssetStore::requireMeshLoaded(TprMesh handle) noexcept {
    std::unique_lock<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        if (get_basic_handle_type(handle) != handle_type::mesh) return TPR_ERROR_INVALID_VALUE;
        if (get_basic_handle_index(handle) > mMeshHandleCounter) return TPR_ERROR_INVALID_VALUE;
        auto it = mMeshes.find(get_basic_handle_index(handle));
        if (it == mMeshes.end()) return TPR_ERROR_INVALID_VALUE;
        auto entry = it->second.entry;
        entry->requiredLoadedCounter++;
        {
            unlock_guard unlock(lock);
            if (entry->requiredLoadedCounter == 1) {
                if (auto r = mpGDev->loadMesh(entry->id); r != TPR_SUCCESS) {
                    return r;
                }
            }
        }
        return TPR_SUCCESS;

    } catch (const std::runtime_error& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}

TprResult AssetStore::unrequireMeshLoaded(TprMesh mesh) noexcept {
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    try {
        if (get_basic_handle_type(mesh) != handle_type::mesh) return TPR_ERROR_INVALID_VALUE;
        if (get_basic_handle_index(mesh) > mMeshHandleCounter) return TPR_ERROR_INVALID_VALUE;
        auto it = mMeshes.find(get_basic_handle_index(mesh));
        if (it == mMeshes.end()) return TPR_ERROR_INVALID_VALUE;
        auto& entry = it->second.entry;
        if (entry->requiredLoadedCounter == 1) {
            if (auto r = unloadMesh(*entry.get()); r != TPR_SUCCESS) {
                return r;
            }
        }
        entry->requiredLoadedCounter--;
        return TPR_SUCCESS;
        
    } catch (const std::runtime_error& e) {
        mLogger.panic() << "Exception: " << e.what();
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    } catch (...) {
        mLogger.panic() << "Unknown exception";
        mrRunResult.store(TPR_PANIC);
        return TPR_PANIC;
    }
}

TprResult AssetStore::unloadMesh(MeshEntry& mesh) {
    if (mesh.requiredLoadedCounter != 0) {
        if (auto r = mpGDev->unloadMesh(mesh.id); r != TPR_SUCCESS) {
            return r;
        }
    }
    return TPR_SUCCESS;
}


expected<MeshIdentity, TprResult> AssetStore::getMeshIdentity(TprMesh mesh) {
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    if (get_basic_handle_type(mesh) != handle_type::mesh) return unexpected(TPR_ERROR_INVALID_VALUE);
    auto it = mMeshes.find(get_basic_handle_index(mesh));
    if (it == mMeshes.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
    if (it->second.entry->requiredLoadedCounter == 0) return unexpected(TPR_ERROR_INVALID_VALUE);  // mesh isn't loaded
    return it->second.entry->id;
}

expected<const MeshData*, TprResult> AssetStore::getMesh(MeshIdentity id) {
    std::lock_guard<std::mutex> lock(mMutex);
    assert(mInitialised);
    auto it = mMeshEntryMap.find(id);
    if (it == mMeshEntryMap.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
    return &it->second->data;
}
