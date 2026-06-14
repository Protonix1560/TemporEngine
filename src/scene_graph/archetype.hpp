

#ifndef SCENE_MANAGER_ARCHETYPE_HPP_
#define SCENE_MANAGER_ARCHETYPE_HPP_


#include "core.hpp"
#include "matrix.hpp"
#include "plugin_core.h"
#include "set_key.hpp"
#include "plugin_core_extender.hpp"

#include <cstring>
#include <unordered_map>
#include <map>
#include <cassert>


struct Chunk {
    private:
        uint32_t mVersion;
        matrix<std::byte> mData;
        friend class Archetype;
    public:
        Chunk() = default;
        uint32_t version() const { return mVersion; }
        void incrementVersion() { mVersion++; }
        uint32_t count() const { return mData.height(); }
        uint32_t width() const { return mData.width(); }
        std::byte* data() { return mData.data(); }
};


class Archetype {

    public:
        Archetype(uint32_t chunkSize) : mChunkSize(chunkSize) {
            assert(chunkSize > 0);
        }

        Archetype(uint32_t chunkSize, const set_key<TprComponentInfo>& components)
            : mChunkSize(chunkSize), mComponents(components), mLayersDenseChunks(0, components.size()) {
            assert(chunkSize > 0);
            for (auto& component : mComponents) {
                mComponentMap.try_emplace(component.wrapper, mComponentMap.size());
            }
        }

        bool contains(TprComponentWrapper component) const { return mComponentMap.contains(component); }
        uint32_t size() const { return mLayersIndices.size(); }
        bool empty() const { return size() == 0; }
        const set_key<TprComponentInfo>& components() const { return mComponents; }

        [[nodiscard]] std::pair<uint32_t, std::vector<std::pair<uint32_t, TprComponentInfo>>> spawn() {
            std::vector<std::pair<uint32_t, TprComponentInfo>> addedChunks;
            if (mLayersSparse.size() % mChunkSize == 0) {
                mLayersDenseChunks.insert_column(mLayersDenseChunks.width());
                auto it = mComponents.begin();
                for (uint32_t i = 0; i < mComponents.size(); i++, it++) {
                    auto& chunk = mLayersDenseChunks[i][mLayersDenseChunks.width() - 1];
                    chunk.mData = matrix<std::byte>(it->size, 0);
                    chunk.mData.reserve(it->size * mChunkSize);
                    addedChunks.push_back(std::make_pair(mLayersDenseChunks.width() - 1, it->wrapper.component));
                }
            }
            for (size_t i = 0; i < mLayersDenseChunks.height(); i++) {
                auto& chunk = mLayersDenseChunks[i][mLayersDenseChunks.width() - 1];
                chunk.mData.insert_row(chunk.mData.height());
                chunk.incrementVersion();
            }
            uint32_t sparseSize = mLayersSparse.size();
            uint32_t indicesSize = mLayersIndices.size();
            mLayersIndices.push_back(sparseSize);
            mLayersSparse.push_back(indicesSize);
            return std::make_pair(sparseSize, addedChunks);
        }

        [[nodiscard]] std::vector<std::pair<uint32_t, TprComponentInfo>> kill(uint32_t entity) {
            if (entity >= mLayersSparse.size()) return {};
            uint32_t offset = mLayersSparse[entity];
            if (offset == UINT32_MAX) return {};
            if (offset != mLayersIndices.size() - 1) {
                for (size_t i = 0; i < mLayersDenseChunks.height(); i++) {
                    auto& chunk = mLayersDenseChunks[i][offset / mChunkSize];
                    auto& lastChunk = mLayersDenseChunks[i][mLayersDenseChunks.width() - 1];
                    std::memcpy(chunk.mData[offset % mChunkSize], lastChunk.mData[lastChunk.mData.height() - 1], chunk.mData.width());
                }
                mLayersIndices[offset] = mLayersIndices.back();
            }
            mLayersSparse[entity] = UINT32_MAX;
            std::vector<std::pair<uint32_t, TprComponentInfo>> removedChunks;
            if (!mLayersDenseChunks.empty() && mLayersDenseChunks[0][mLayersDenseChunks.width() - 1].mData.height() == 1) {
                auto it = mComponents.begin();
                for (uint32_t i = 0; i < mComponents.size(); i++, it++) {
                    removedChunks.push_back(std::make_pair(mLayersDenseChunks.width() - 1, it->wrapper.component));
                }
                mLayersDenseChunks.erase_column(mLayersDenseChunks.width() - 1);
            } else {
                for (size_t i = 0; i < mLayersDenseChunks.height(); i++) {
                    auto& chunk = mLayersDenseChunks[i][mLayersDenseChunks.width() - 1];
                    chunk.mData.erase_row(chunk.mData.height() - 1);
                    chunk.incrementVersion();
                }
            }
            mLayersIndices.pop_back();
            return removedChunks;
        }

        void destroyComponent(TprComponentWrapper component) {
            if (!contains(component)) return;
            size_t offset = mComponentMap.at(component);
            mLayersDenseChunks.erase_row(offset);
            mComponents.erase(component);
            mComponentMap.erase(component);
        }

        bool operator==(const Archetype& other) const { return mComponents == other.mComponents; }
        bool operator!=(const Archetype& other) const { return mComponents != other.mComponents; }

        expected<std::byte*, TprResult> get(TprEntityWrapper entity, TprComponentWrapper component) {
            if (!contains(component)) return unexpected(TPR_ERROR_INVALID_VALUE);
            if (entity.entity.id >= mLayersSparse.size()) return unexpected(TPR_ERROR_INVALID_VALUE);
            uint32_t offset = mLayersSparse[entity.entity.id];
            if (offset == UINT32_MAX) return unexpected(TPR_ERROR_INVALID_VALUE);
            auto& chunk = mLayersDenseChunks[mComponentMap.at(component)][offset / mChunkSize];
            return chunk.mData[offset % mChunkSize];
        }

        expected<Chunk*, TprResult> chunk(uint32_t index, TprComponentWrapper component) {
            if (!contains(component)) return unexpected(TPR_ERROR_INVALID_VALUE);
            return &mLayersDenseChunks[mComponentMap.at(component)][index];
        }

        expected<Chunk*, TprResult> entityChunk(TprEntityWrapper entity, TprComponentWrapper component) {
            if (!contains(component)) return unexpected(TPR_ERROR_INVALID_VALUE);
            if (entity.entity.id >= mLayersSparse.size()) return unexpected(TPR_ERROR_INVALID_VALUE);
            uint32_t offset = mLayersSparse[entity.entity.id];
            if (offset == UINT32_MAX) return unexpected(TPR_ERROR_INVALID_VALUE);
            return &mLayersDenseChunks[mComponentMap.at(component)][offset / mChunkSize];
        }

        expected<uint32_t, TprResult> width(TprComponentWrapper component) {
            if (!contains(component)) return unexpected(TPR_ERROR_INVALID_VALUE);
            return std::ranges::find_if(mComponents, [component](const TprComponentInfo& value) {
                return value == component;
            })->size;
        }
    
    private:
        struct ChunkEntry {
            TprComponentWrapper component;
            uint32_t index;
        };

        uint32_t mChunkSize;

        std::vector<uint32_t> mLayersSparse;
        std::vector<uint32_t> mLayersIndices;
        matrix<Chunk> mLayersDenseChunks;

        set_key<TprComponentInfo> mComponents;
        std::map<TprComponentWrapper, size_t> mComponentMap;

        std::unordered_map<uint32_t, ChunkEntry> mChunks;
};


#endif  // SCENE_MANAGER_ARCHETYPE_HPP_
