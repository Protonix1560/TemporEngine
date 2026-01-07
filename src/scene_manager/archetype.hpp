

#ifndef SCENE_MANAGER_ARCHETYPE_HPP_
#define SCENE_MANAGER_ARCHETYPE_HPP_


#include "core.hpp"
#include "logger.hpp"  // IWYU pragma: keep

#include <algorithm>
#include <vector>
#include <cstdint>
#include <cstring>




class PackedArray {

    public:
        PackedArray(size_t elSize = 1, size_t size = 0)
            : mElSize(elSize), mSize(size) {
                if (elSize == 0) {
                    throw Exception(ErrCode::InternalError, "PackedArray: elSize must be greater or equal to 1");
                }
                if (size != 0) {
                    mData.resize(mElSize * size);
                }
            }

            std::byte* data(size_t index) {
                return mData.data() + mElSize * index;
            }

            void resize(size_t newSize) { 
                if (newSize == 0) {
                    mData.clear();
                } else {
                    mData.resize(newSize * mElSize);
                }
                mSize = newSize;
            }

            size_t size() const {
                return mSize;
            }

            size_t elSize() const {
                return mElSize;
            }

    private:
        std::vector<std::byte> mData;
        size_t mElSize;
        size_t mSize;

};



struct ComponentDeclare {
    uint32_t id;
    uint32_t size;
};



struct Replace {
    uint32_t id;
    uint32_t newId;
};



class Archetype {


    public:

        void create(const std::vector<ComponentDeclare>& components) {

            mAlive = true;

            mEntityCount = 0;

            if (components.size() > UINT32_MAX) {
                throw Exception(ErrCode::InternalError, "Archetype::create: components.size() must be compatible with 32-bit integer");
            }

            mComponentsData.resize(components.size());

            if (components.size()) {
                uint32_t maxComponentId = std::max_element(
                    components.begin(), components.end(),
                    [](const ComponentDeclare& a, const ComponentDeclare& b) -> bool {
                        return a.size < b.size;
                    }
                )->id;
                if (maxComponentId == UINT32_MAX) {
                    throw Exception(ErrCode::InternalError, "Archetype::create: max allowed component id is 2^32-2");
                }
                mComponentsSparse.assign(maxComponentId + 1, UINT32_MAX);

            } else {
                mComponentsSparse.assign(0, UINT32_MAX);
            }

            for (size_t i = 0; i < components.size(); i++) {
                const ComponentDeclare& component = components[i];
                mComponentsSparse[component.id] = i;
                mComponentsData[i] = PackedArray(component.size, 0);
            }

        }

        bool hasComponent(uint32_t componentId) {
            if (componentId >= mComponentsSparse.size()) return false;
            uint32_t componentDatasIndex = mComponentsSparse[componentId];
            if (componentDatasIndex == UINT32_MAX) return false;
            return true;
        }

        std::byte* get(uint32_t entityId, uint32_t componentId) {

            if (componentId >= mComponentsSparse.size()) return nullptr;
            if (entityId >= mEntityCount) return nullptr;

            uint32_t componentDatasIndex = mComponentsSparse[componentId];

            if (componentDatasIndex == UINT32_MAX) return nullptr;

            return mComponentsData[componentDatasIndex].data(entityId);
            
        }

        uint32_t createEntity(uint32_t globalId) {
            for (auto& componentDatas : mComponentsData) {
                componentDatas.resize(componentDatas.size() + 1);
            }
            mEntityGlobalId.push_back(globalId);

            return mEntityCount++;
        }

        Replace destroyEntity(uint32_t id) {

            for (auto& array : mComponentsData) {
                if (mEntityCount > 1) {
                    std::byte* data = array.data(id);
                    std::memcpy(data, array.data(mEntityCount - 1), array.elSize());
                }
                array.resize(array.size() - 1);
            }

            uint32_t oldId = mEntityGlobalId[mEntityCount - 1];

            mEntityGlobalId.pop_back();
            mEntityCount--;

            return {oldId, id};
        }

        uint32_t entityCount() const {
            return mEntityCount;
        }

        void destroy() {
            mAlive = false;
        }

        bool alive() const {
            return mAlive;
        }


    private:
        std::vector<uint32_t> mComponentsSparse;
        std::vector<PackedArray> mComponentsData;
        std::vector<uint32_t> mEntityGlobalId;
        uint32_t mEntityCount;
        bool mAlive = false;


};




#endif  // SCENE_MANAGER_ARCHETYPE_HPP_
