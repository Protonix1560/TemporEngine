

#ifndef SCENE_MANAGER_ARCHETYPE_HPP_
#define SCENE_MANAGER_ARCHETYPE_HPP_


#include "core.hpp"
#include "matrix.hpp"
#include "set_key.hpp"
#include "plugin_core_extender.hpp"

#include <cstring>
#include <unordered_map>



class Archetype {

    public:
        Archetype() {}

        Archetype(const set_key<TprComponentInfo>& components) : mComponents(components) {
            mLayerDense.reserve(mComponents.size());
            mComponentMap.reserve(mComponents.size());
            for (auto& component : mComponents) {
                mLayerDense.emplace_back(component.size, 0);
                mComponentMap.try_emplace(component.wrapper, mComponentMap.size());
            }
        }

        bool contains(TprComponentWrapper component) const { return mComponentMap.contains(component); }
        uint32_t size() const { return mLayerIndices.size(); }
        bool empty() const { return size() == 0; }
        const set_key<TprComponentInfo>& components() const { return mComponents; }

        void reserve(size_t n) {
            for (auto& layer : mLayerDense) {
                layer.reserve(n);
            }
            mLayerIndices.reserve(n);
            if (n > mLayerIndices.size()) {
                mLayerSparse.reserve(n - mLayerIndices.size() + mLayerSparse.size());
            }
        }

        TprEntityWrapper spawn() {
            for (auto& layer : mLayerDense) {
                layer.emplace_back();
            }
            size_t sparseSize = mLayerSparse.size();
            size_t indicesSize = mLayerIndices.size();
            mLayerIndices.push_back(sparseSize);
            mLayerSparse.push_back(indicesSize);
            return TprEntityWrapper{TprEntity{static_cast<uint32_t>(sparseSize)}};
        }

        void kill(TprEntityWrapper entity) {
            assert(entity.entity.id < mLayerSparse.size());
            uint32_t offset = mLayerSparse[entity.entity.id];
            if (offset != UINT32_MAX) {
                if (offset != mLayerIndices.size() - 1) {
                    for (auto& layer : mLayerDense) {
                        layer[offset] = layer.back();
                    }
                    mLayerIndices[offset] = mLayerIndices.back();
                }
                mLayerSparse[entity.entity.id] = UINT32_MAX;
                for (auto& layer : mLayerDense) {
                    mLayerDense.pop_back();
                }
                mLayerIndices.pop_back();
            }
        }

        void destroyComponent(TprComponentWrapper component) {
            mComponents.erase(component);
            mComponentMap.erase(component);
        }

        bool operator==(const Archetype& other) const { return mComponents == other.mComponents; }
        bool operator!=(const Archetype& other) const { return mComponents != other.mComponents; }

        expected<std::byte*, TprResult> get(TprEntityWrapper entity, TprComponentWrapper component) {
            if (!contains(component)) return unexpected(TPR_INVALID_VALUE);
            return &mLayerDense[mComponentMap.at(component)][mLayerSparse[entity.entity.id]].front();
        }

        expected<uint32_t, TprResult> width(TprComponentWrapper component) {
            if (!contains(component)) return unexpected(TPR_INVALID_VALUE);
            return mLayerDense[mComponentMap.at(component)].sizes()[0];
        }
    
    private:
        std::vector<uint32_t> mLayerSparse;
        std::vector<uint32_t> mLayerIndices;
        std::vector<matrix<2, std::byte>> mLayerDense;

        set_key<TprComponentInfo> mComponents;
        std::unordered_map<TprComponentWrapper, size_t> mComponentMap;
};


#endif  // SCENE_MANAGER_ARCHETYPE_HPP_
