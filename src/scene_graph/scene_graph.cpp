
#include "scene_graph.hpp"
#include "archetype.hpp"
#include "logger.hpp"
#include "plugin_core.h"

#include <cstdint>
#include <memory>



SceneGraph::SceneGraph(Logger& rLogger) : mrLogger(rLogger) {
}



SceneGraph::~SceneGraph() noexcept {

}



void SceneGraph::update() {
    
}





size_t SceneGraph::getArchetype(uint32_t componentCount, const uint32_t* components) {

    for (size_t i = 0; i < mArchetypes.size(); i++) {

        auto& archetype = mArchetypes[i];
        if (!archetype.alive()) continue;
        
        for (uint32_t j = 0; j < componentCount; j++) {
            if (!archetype.hasComponent(components[j])) goto not_matching_components;
        }

        // matching_components:
        return i;

        not_matching_components: ;
    }

    return mArchetypes.size();
}





TprResult SceneGraph::registerComponent(uint32_t componentSize, const char* componentName, uint32_t* pNewComponentId) noexcept {

    try {

        if (mComponentCounter == UINT32_MAX) return TPR_COUNT_OVERFLOW;
        mComponentNameRegister.emplace_back(componentName);
        *pNewComponentId = mComponentCounter;
        mComponentSizes.push_back(componentSize);
        mComponentCounter++;

    } catch (const std::exception& e) {
        mrLogger << e.what() << "\n";
        return TPR_UNKNOWN_ERROR;
    } catch (...) {
        return TPR_UNKNOWN_ERROR;
    }

    return TPR_SUCCESS;
}


TprResult SceneGraph::acquireComponent(const char* componentName, uint32_t* pComponentId) noexcept {

    try {

        if (mComponentCounter == UINT32_MAX) return TPR_COUNT_OVERFLOW;
        for (uint32_t i = 0; i < mComponentNameRegister.size(); i++) {
            if (std::strcmp(mComponentNameRegister[i].c_str(), componentName) == 0) {
                *pComponentId = i;
                return TPR_SUCCESS;
            }
        }
        return TPR_INVALID_VALUE;

    } catch (const std::exception& e) {
        mrLogger << e.what() << "\n";
        return TPR_UNKNOWN_ERROR;
    } catch (...) {
        return TPR_UNKNOWN_ERROR;
    }
}


TprResult SceneGraph::createEntity(uint32_t componentIdCount, const uint32_t* pComponentIds, TprEntity* pEntityHandle) noexcept {
    
    try {

        for (auto it = pComponentIds; it < pComponentIds + componentIdCount; it++) {
            if (*it >= mComponentCounter) return TPR_INVALID_VALUE;
        }

        size_t archetypeIndex = getArchetype(componentIdCount, pComponentIds);
        if (archetypeIndex == mArchetypes.size()) {

            if (!mFreeArchetypes.empty()) {
                archetypeIndex = mFreeArchetypes.back();
                mFreeArchetypes.pop_back();
            } else {
                archetypeIndex = mArchetypes.size();
                mArchetypes.emplace_back();
                mArchetypeMutexes.emplace_back(std::make_unique<std::shared_mutex>());
            }

            std::vector<ComponentDeclare> components;
            components.reserve(componentIdCount);
            for (auto it = pComponentIds; it < pComponentIds + componentIdCount; it++) {
                components.push_back({*it, mComponentSizes[*it]});
            }

            mArchetypes[archetypeIndex].create(components);
        }

        uint32_t entityIndex;
        if (!mFreeEntityEntries.empty()) {
            entityIndex = mFreeEntityEntries.back();
            mFreeEntityEntries.pop_back();
        } else {
            entityIndex = mEntityEntries.size();
            mEntityEntries.emplace_back();
        }

        Archetype& archetype = mArchetypes[archetypeIndex];

        pEntityHandle->id = entityIndex;
        EntityEntry& entry = mEntityEntries[entityIndex];
        entry.archetype = archetypeIndex;
        entry.id = archetype.createEntity(entityIndex);
        entry.gen++;
        pEntityHandle->gen = entry.gen;

    } catch (const std::exception& e) {
        mrLogger << e.what() << "\n";
        return TPR_UNKNOWN_ERROR;
    } catch (...) {
        return TPR_UNKNOWN_ERROR;
    }
    
    return TPR_SUCCESS;
}


void SceneGraph::destroyEntity(const TprEntity* handle) noexcept {

    auto& log = mrLogger;

    try {

        if (mEntityEntries.size() <= handle->id) return;

        EntityEntry entry = mEntityEntries[handle->id];
        if (entry.id == UINT32_MAX) return;
        if (entry.gen != handle->id) return;

        Archetype& archetype = mArchetypes[entry.archetype];

        Replace replace = archetype.destroyEntity(entry.id);

        if (replace.id != handle->id) {
            mEntityEntries[replace.id].id = replace.newId;
        }
        if (mEntityEntries.size() - 1 == handle->id) {
            mEntityEntries.pop_back();
        } else {
            mFreeEntityEntries.push_back(handle->id);
        }

        if (archetype.entityCount() == 0) {
            archetype.destroy();
            if (entry.archetype == mArchetypes.size() - 1) {
                mArchetypes.pop_back();
                mArchetypeMutexes.pop_back();
            } else {
                mFreeArchetypes.push_back(entry.archetype);
            }
        }

    } catch (const std::exception& e) {
        mrLogger << e.what() << "\n";
        return;
    } catch(...) {
        return;
    }

    return;
}



TprResult SceneGraph::copyEntityComponentData(const TprEntity* handle, uint32_t componentId, uint32_t start, uint32_t end, char* componentData) noexcept {

    try {

        if (mEntityEntries.size() <= handle->id) return TPR_INVALID_VALUE;
        if (componentId >= mComponentCounter) return TPR_INVALID_VALUE;
        if (end <= start && end != 0) return TPR_INVALID_VALUE;
        if ((end - start) > mComponentSizes[componentId]) return TPR_INVALID_VALUE;

        EntityEntry entry = mEntityEntries[handle->id];
        if (entry.id == UINT32_MAX) return TPR_INVALID_VALUE;
        if (entry.gen != handle->id) return TPR_INVALID_VALUE;

        Archetype& archetype = mArchetypes[entry.archetype];

        char* data = reinterpret_cast<char*>(archetype.get(entry.id, componentId));

        if (end != 0) {
            std::memcpy(componentData, data + start, end - start);
        } else {
            std::memcpy(componentData, data + start, mComponentSizes[componentId]);
        }

    } catch (const std::exception& e) {
        mrLogger << e.what() << "\n";
        return TPR_UNKNOWN_ERROR;
    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }

    return TPR_SUCCESS;
}



TprResult SceneGraph::writeEntityComponentData(const TprEntity* handle, uint32_t componentId, const char* componentData, uint32_t start, uint32_t end) noexcept {

    try {

        if (mEntityEntries.size() <= handle->id) return TPR_INVALID_VALUE;
        if (componentId >= mComponentCounter) return TPR_INVALID_VALUE;
        if (end <= start && end != 0) return TPR_INVALID_VALUE;
        if (end > mComponentSizes[componentId]) return TPR_INVALID_VALUE;

        EntityEntry entry = mEntityEntries[handle->id];
        if (entry.id == UINT32_MAX) return TPR_INVALID_VALUE;
        if (entry.gen != handle->id) return TPR_INVALID_VALUE;

        Archetype& archetype = mArchetypes[entry.archetype];

        char* data = reinterpret_cast<char*>(archetype.get(entry.id, componentId));

        if (end != 0) {
            std::memcpy(data + start, componentData, end - start);
        } else {
            std::memcpy(data + start, componentData, mComponentSizes[componentId]);
        }

    } catch (const std::exception& e) {
        mrLogger << e.what() << "\n";
        return TPR_UNKNOWN_ERROR;
    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }

    return TPR_SUCCESS;
}


TprResult SceneGraph::readEntityComponent8bit(const TprEntity* handle, uint32_t componentId, uint32_t offset, uint8_t* data) noexcept {
    try {
        if (mEntityEntries.size() <= handle->id) return TPR_INVALID_VALUE;
        if (componentId >= mComponentCounter) return TPR_INVALID_VALUE;
        if ((offset + sizeof(uint8_t)) > mComponentSizes[componentId]) return TPR_INVALID_VALUE;
        EntityEntry entry = mEntityEntries[handle->id];
        if (entry.id == UINT32_MAX) return TPR_INVALID_VALUE;
        if (entry.gen != handle->id) return TPR_INVALID_VALUE;
        Archetype& archetype = mArchetypes[entry.archetype];
        char* d = reinterpret_cast<char*>(archetype.get(entry.id, componentId));
        std::memcpy(data, d + offset, sizeof(uint8_t));
    } catch (const std::exception& e) {
        mrLogger << e.what() << "\n";
        return TPR_UNKNOWN_ERROR;
    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}


TprResult SceneGraph::readEntityComponent16bit(const TprEntity* handle, uint32_t componentId, uint32_t offset, uint16_t* data) noexcept {
    try {
        if (mEntityEntries.size() <= handle->id) return TPR_INVALID_VALUE;
        if (componentId >= mComponentCounter) return TPR_INVALID_VALUE;
        if ((offset + sizeof(uint16_t)) > mComponentSizes[componentId]) return TPR_INVALID_VALUE;
        EntityEntry entry = mEntityEntries[handle->id];
        if (entry.id == UINT32_MAX) return TPR_INVALID_VALUE;
        if (entry.gen != handle->id) return TPR_INVALID_VALUE;
        Archetype& archetype = mArchetypes[entry.archetype];
        char* d = reinterpret_cast<char*>(archetype.get(entry.id, componentId));
        std::memcpy(data, d + offset, sizeof(uint16_t));
    } catch (const std::exception& e) {
        mrLogger << e.what() << "\n";
        return TPR_UNKNOWN_ERROR;
    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}


TprResult SceneGraph::readEntityComponent32bit(const TprEntity* handle, uint32_t componentId, uint32_t offset, uint32_t* data) noexcept {
    try {
        if (mEntityEntries.size() <= handle->id) return TPR_INVALID_VALUE;
        if (componentId >= mComponentCounter) return TPR_INVALID_VALUE;
        if ((offset + sizeof(uint32_t)) > mComponentSizes[componentId]) return TPR_INVALID_VALUE;
        EntityEntry entry = mEntityEntries[handle->id];
        if (entry.id == UINT32_MAX) return TPR_INVALID_VALUE;
        if (entry.gen != handle->id) return TPR_INVALID_VALUE;
        Archetype& archetype = mArchetypes[entry.archetype];
        char* d = reinterpret_cast<char*>(archetype.get(entry.id, componentId));
        std::memcpy(data, d + offset, sizeof(uint32_t));
    } catch (const std::exception& e) {
        mrLogger << e.what() << "\n";
        return TPR_UNKNOWN_ERROR;
    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}


TprResult SceneGraph::readEntityComponent64bit(const TprEntity* handle, uint32_t componentId, uint32_t offset, uint64_t* data) noexcept {
    try {
        if (mEntityEntries.size() <= handle->id) return TPR_INVALID_VALUE;
        if (componentId >= mComponentCounter) return TPR_INVALID_VALUE;
        if ((offset + sizeof(uint64_t)) > mComponentSizes[componentId]) return TPR_INVALID_VALUE;
        EntityEntry entry = mEntityEntries[handle->id];
        if (entry.id == UINT32_MAX) return TPR_INVALID_VALUE;
        if (entry.gen != handle->id) return TPR_INVALID_VALUE;
        Archetype& archetype = mArchetypes[entry.archetype];
        char* d = reinterpret_cast<char*>(archetype.get(entry.id, componentId));
        std::memcpy(data, d + offset, sizeof(uint64_t));
    } catch (const std::exception& e) {
        mrLogger << e.what() << "\n";
        return TPR_UNKNOWN_ERROR;
    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}


TprResult SceneGraph::writeEntityComponent8bit(const TprEntity* handle, uint32_t componentId, uint8_t data, uint32_t offset) noexcept {
    try {
        if (mEntityEntries.size() <= handle->id) return TPR_INVALID_VALUE;
        if (componentId >= mComponentCounter) return TPR_INVALID_VALUE;
        if ((offset + sizeof(uint8_t)) > mComponentSizes[componentId]) return TPR_INVALID_VALUE;
        EntityEntry entry = mEntityEntries[handle->id];
        if (entry.id == UINT32_MAX) return TPR_INVALID_VALUE;
        if (entry.gen != handle->id) return TPR_INVALID_VALUE;
        Archetype& archetype = mArchetypes[entry.archetype];
        char* d = reinterpret_cast<char*>(archetype.get(entry.id, componentId));
        std::memcpy(d + offset, &data, sizeof(uint8_t));
    } catch (const std::exception& e) {
        mrLogger << e.what() << "\n";
        return TPR_UNKNOWN_ERROR;
    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}


TprResult SceneGraph::writeEntityComponent16bit(const TprEntity* handle, uint32_t componentId, uint16_t data, uint32_t offset) noexcept {
    try {
        if (mEntityEntries.size() <= handle->id) return TPR_INVALID_VALUE;
        if (componentId >= mComponentCounter) return TPR_INVALID_VALUE;
        if ((offset + sizeof(uint16_t)) > mComponentSizes[componentId]) return TPR_INVALID_VALUE;
        EntityEntry entry = mEntityEntries[handle->id];
        if (entry.id == UINT32_MAX) return TPR_INVALID_VALUE;
        if (entry.gen != handle->id) return TPR_INVALID_VALUE;
        Archetype& archetype = mArchetypes[entry.archetype];
        char* d = reinterpret_cast<char*>(archetype.get(entry.id, componentId));
        std::memcpy(d + offset, &data, sizeof(uint16_t));
    } catch (const std::exception& e) {
        mrLogger << e.what() << "\n";
        return TPR_UNKNOWN_ERROR;
    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}


TprResult SceneGraph::writeEntityComponent32bit(const TprEntity* handle, uint32_t componentId, uint32_t data, uint32_t offset) noexcept {
    try {
        if (mEntityEntries.size() <= handle->id) return TPR_INVALID_VALUE;
        if (componentId >= mComponentCounter) return TPR_INVALID_VALUE;
        if ((offset + sizeof(uint32_t)) > mComponentSizes[componentId]) return TPR_INVALID_VALUE;
        EntityEntry entry = mEntityEntries[handle->id];
        if (entry.id == UINT32_MAX) return TPR_INVALID_VALUE;
        if (entry.gen != handle->id) return TPR_INVALID_VALUE;
        Archetype& archetype = mArchetypes[entry.archetype];
        char* d = reinterpret_cast<char*>(archetype.get(entry.id, componentId));
        std::memcpy(d + offset, &data, sizeof(uint32_t));
    } catch (const std::exception& e) {
        mrLogger << e.what() << "\n";
        return TPR_UNKNOWN_ERROR;
    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}


TprResult SceneGraph::writeEntityComponent64bit(const TprEntity* handle, uint32_t componentId, uint64_t data, uint32_t offset) noexcept {
    try {
        if (mEntityEntries.size() <= handle->id) return TPR_INVALID_VALUE;
        if (componentId >= mComponentCounter) return TPR_INVALID_VALUE;
        if ((offset + sizeof(uint64_t)) > mComponentSizes[componentId]) return TPR_INVALID_VALUE;
        EntityEntry entry = mEntityEntries[handle->id];
        if (entry.id == UINT32_MAX) return TPR_INVALID_VALUE;
        if (entry.gen != handle->id) return TPR_INVALID_VALUE;
        Archetype& archetype = mArchetypes[entry.archetype];
        char* d = reinterpret_cast<char*>(archetype.get(entry.id, componentId));
        std::memcpy(d + offset, &data, sizeof(uint64_t));
    } catch (const std::exception& e) {
        mrLogger << e.what() << "\n";
        return TPR_UNKNOWN_ERROR;
    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}



