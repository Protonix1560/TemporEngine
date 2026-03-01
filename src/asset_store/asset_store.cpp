

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



AssetStore::AssetStore(Logger& rLogger, ResourceRegistry& rRegReg)
    : mrLogger(rLogger), mrResReg(rRegReg)
{
    mLoc = *gGetServiceLocator();
}


AssetStore::~AssetStore() noexcept {

}


void AssetStore::update() {

}



TprResult AssetStore::validateHandle(TprAsset handle) {
    if (get_basic_handle_type(handle) != handle_type::asset) {
        return TPR_INVALID_VALUE;
    }
    if (get_basic_handle_index(handle) >= mAssets.size()) {
        return TPR_INVALID_VALUE;
    }
    Asset& asset = std::visit([](auto& asset) -> Asset& { return static_cast<Asset&>(asset); }, mAssets[get_basic_handle_index(handle)]);
    if (!asset.actual) {
        return TPR_INVALID_VALUE;
    }
    if (asset.generation != get_basic_handle_generation(handle)) {
        return TPR_INVALID_VALUE;
    }
    return TPR_SUCCESS;
}



expected<TprAsset, TprResult> AssetStore::loadAsset(const TprAssetLoadInfo* info) noexcept {

    auto& mrResReg = mLoc.get<ResourceRegistry>();

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
    asset.data.reserve(mrResReg.sizeofResource(info->data).value());
    asset.data.resize(mrResReg.sizeofResource(info->data).value());
    std::memcpy(asset.data.data(), mrResReg.getResourceConstPointer(info->data).value(), mrResReg.sizeofResource(info->data).value());

    TprAsset handle = construct_basic_handle<TprAsset>(index, asset.generation, handle_type::asset);

    return handle;
}



expected<TprAsset, TprResult> AssetStore::parseAsset(const TprAssetParseInfo* parseInfo) noexcept {

    TprAssetLoadInfo loadInfo{};

    std::string warn;
    std::string err;
    tinygltf::TinyGLTF loader;
    tinygltf::Model scene;

    bool res = loader.LoadBinaryFromMemory(
        &scene, &err, &warn, 
        reinterpret_cast<const unsigned char*>(mrResReg.getResourceConstPointer(parseInfo->resource).value()), 
        mrResReg.sizeofResource(parseInfo->resource).value()
    );

    if (!warn.empty()) {
        mrLogger.warn(TPR_LOG_STYLE_WARN1) << "tinygltf: " << warn << "\n";
    }
    if (!res || !err.empty()) {
        mrLogger.error(TPR_LOG_STYLE_ERROR1) << "tinygltf: " << err << "\n";
        return unexpected(TPR_PARSE_ERROR);
    }

    switch (parseInfo->type) {

        case TPR_ASSET_TYPE_MODEL: {

            if (parseInfo->index >= scene.meshes.size()) return unexpected(TPR_COUNT_OVERFLOW);
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

            loadInfo.data = mrResReg.openResource(assetDataSize, 0, 1).value();
            std::byte* pData = mrResReg.getResourceRawDataPointer(loadInfo.data).value();
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

        default: ;

    }
    
    auto assetHandle = loadAsset(&loadInfo);
    
    mrResReg.closeResource(loadInfo.data);

    return assetHandle;
}



void AssetStore::destroyAsset(TprAsset handle) noexcept {

    if (validateHandle(handle) < 0) return;

    Asset& asset = std::visit([](auto& asset) -> Asset& { return static_cast<Asset&>(asset); }, mAssets[get_basic_handle_index(handle)]);

    asset.actual = false;

    mFreeAssets.push_back(get_basic_handle_index(handle));

}


