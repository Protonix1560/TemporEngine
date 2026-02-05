

#include "asset_store.hpp"
#include "core.hpp"
#include "plugin_core.h"
#include "logger.hpp"
#include "resource_registry.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define JSON_NOEXCEPTION
#include <tiny_gltf.h>

#include <cstring>



#define MEMCPY_SINGLE(d, s, f, m) std::memcpy((d) + offsetof(s, f), m, sizeof(s::f));



AssetStore::AssetStore() {
    mLoc = *gGetServiceLocator();
}


void AssetStore::init() {

}

void AssetStore::update() {

}

void AssetStore::shutdown() noexcept {

}



void AssetStore::validateHandle(TprAsset handle) {
    if (getBasicHandleType(handle) != HandleType::Asset) {
        throw Exception(ErrCode::WrongValueError, logPrxAStr() + "Handle type is not Asset");
    }
    if (getBasicHandleIndex(handle) >= mAssets.size()) {
        throw Exception(ErrCode::WrongValueError, logPrxAStr() + "Invalid handle");
    }
    Asset& asset = std::visit([](auto& asset) -> Asset& { return static_cast<Asset&>(asset); }, mAssets[getBasicHandleIndex(handle)]);
    if (!asset.actual) {
        throw Exception(ErrCode::WrongValueError, logPrxAStr() + "Destroyed asset");
    }
    if (asset.generation != getBasicHandleGeneration(handle)) {
        throw Exception(ErrCode::WrongValueError, logPrxAStr() + "Destroyed asset");
    }
}



Expected<TprAsset, TprResult> AssetStore::loadAsset(const TprAssetLoadInfo* info) noexcept {

    auto& rreg = mLoc.get<ResourceRegistry>();

    size_t index;
    if (!mFreeAssets.empty()) {
        index = mFreeAssets.back();
        mFreeAssets.pop_back();
    } else {
        index = mAssets.size();
        mAssets.emplace_back();
    }

    Asset& asset = std::visit([](auto& asset) -> Asset& { return static_cast<Asset&>(asset); }, mAssets[index]);

    asset.generation++;
    asset.data.reserve(rreg.sizeofResource(info->data));
    asset.data.resize(rreg.sizeofResource(info->data));
    std::memcpy(asset.data.data(), rreg.getResourceRawRODataPointer(info->data), rreg.sizeofResource(info->data));

    TprAsset handle = constructBasicHandle<TprAsset>(index, asset.generation, HandleType::Asset);

    return handle;
}



Expected<TprAsset, TprResult> AssetStore::parseAsset(const TprAssetParseInfo* parseInfo) noexcept {

    auto& logger = mLoc.get<Logger>();
    auto& rreg = mLoc.get<ResourceRegistry>();

    TprAssetLoadInfo loadInfo{};

    std::string warn;
    std::string err;
    tinygltf::TinyGLTF loader;
    tinygltf::Model scene;

    bool res = loader.LoadBinaryFromMemory(
        &scene, &err, &warn, 
        reinterpret_cast<const unsigned char*>(rreg.getResourceRawRODataPointer(parseInfo->resource)), 
        rreg.sizeofResource(parseInfo->resource)
    );

    if (!warn.empty()) {
        logger.warn(TPR_LOG_STYLE_WARN1) << "tinygltf: " << warn << "\n";
    }
    if (!res || !err.empty()) {
        logger.error(TPR_LOG_STYLE_ERROR1) << "tinygltf: " << err << "\n";
        return Unexpected(TPR_PARSE_ERROR);
    }

    switch (parseInfo->type) {

        case TPR_ASSET_TYPE_MODEL: {

            if (parseInfo->index >= scene.meshes.size()) return Unexpected(TPR_COUNT_OVERFLOW);
            const tinygltf::Mesh& mesh = scene.meshes[0];

            uint32_t assetIndicesSize = 0;
            uint32_t assetVerticesSize = 0;
            for (const auto& primitive : mesh.primitives) {
                const auto& indicesAccessor = scene.accessors[primitive.indices];
                const auto& verticesAccessor = scene.accessors[primitive.attributes.at("POSITION")];
                assetIndicesSize += indicesAccessor.count * sizeof(AssetModel::Index);
                assetVerticesSize += verticesAccessor.count * sizeof(AssetModel::Vertex);
            }
            uint32_t assetDataSize = sizeof(AssetModel::Header) + assetIndicesSize + assetVerticesSize;

            TprLifetime lifetime;
            lifetime.frames = UINT64_MAX;
            loadInfo.data = rreg.openResource(assetDataSize, 0, 1, &lifetime);
            std::byte* pData = rreg.getResourceRawRWDataPointer(loadInfo.data);
            uint32_t offset = sizeof(AssetModel::Header);

            // indices
            MEMCPY_SINGLE(pData, AssetModel::Header, indexCount, &assetIndicesSize);
            MEMCPY_SINGLE(pData, AssetModel::Header, indexOffset, &offset);
            uint32_t accumIndexCount = 0;
            for (const auto& primitive : mesh.primitives) {
                const auto& indicesAccessor = scene.accessors[primitive.indices];
                const auto& indicesBufferView = scene.bufferViews[indicesAccessor.bufferView];
                const auto& indicesBuffer = scene.buffers[indicesBufferView.buffer];
                uint32_t originalIndexSize = tinygltf::GetNumComponentsInType(indicesAccessor.type) * tinygltf::GetComponentSizeInBytes(indicesAccessor.componentType);
                for (uint32_t i = 0; i < indicesAccessor.count; i++) {
                    AssetModel::Index index;
                    std::memcpy(
                        &index,
                        indicesBuffer.data.data() + indicesBufferView.byteOffset + indicesAccessor.byteOffset + i * originalIndexSize,
                        originalIndexSize
                    );
                    index += accumIndexCount;
                    std::memcpy(
                        pData + offset,
                        &index,
                        sizeof(AssetModel::Index)
                    );
                    offset += sizeof(AssetModel::Index);
                }
                accumIndexCount += indicesAccessor.count;
            }

            // vertices
            MEMCPY_SINGLE(pData, AssetModel::Header, vertexCount, &assetVerticesSize);
            MEMCPY_SINGLE(pData, AssetModel::Header, vertexOffset, &offset);
            for (const auto& primitive : mesh.primitives) {
                const auto& verticesAccessor = scene.accessors[primitive.attributes.at("POSITION")];
                const auto& verticesBufferView = scene.bufferViews[verticesAccessor.bufferView];
                const auto& verticesBuffer = scene.buffers[verticesBufferView.buffer];
                uint32_t originalVertexPosSize = tinygltf::GetNumComponentsInType(verticesAccessor.type) * tinygltf::GetComponentSizeInBytes(verticesAccessor.componentType);
                for (uint32_t i = 0; i < verticesAccessor.count; i++) {
                    AssetModel::Vertex vertex;
                    std::memcpy(
                        &vertex.pos,
                        verticesBuffer.data.data() + verticesBufferView.byteOffset + verticesAccessor.byteOffset + i * originalVertexPosSize,
                        originalVertexPosSize
                    );
                    std::memcpy(
                        pData + offset,
                        &vertex,
                        sizeof(AssetModel::Vertex)
                    );
                    offset += sizeof(AssetModel::Vertex);
                }
            }

        }

    }
    
    auto assetHandle = loadAsset(&loadInfo);
    
    rreg.closeResource(loadInfo.data);

    return assetHandle;
}



void AssetStore::destroyAsset(TprAsset handle) noexcept {

    validateHandle(handle);

    Asset& asset = std::visit([](auto& asset) -> Asset& { return static_cast<Asset&>(asset); }, mAssets[getBasicHandleIndex(handle)]);

    asset.actual = false;

    mFreeAssets.push_back(getBasicHandleIndex(handle));

}


