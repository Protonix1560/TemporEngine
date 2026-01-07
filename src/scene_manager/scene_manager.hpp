

#ifndef SCENE_MANAGER_SCENE_MANAGER_HPP_
#define SCENE_MANAGER_SCENE_MANAGER_HPP_



#include "archetype.hpp"
#include "plugin_core.h"

#include <stdint.h>
#include <deque>
#include <vector>
#include <shared_mutex>
#include <atomic>
#include <memory>




class SceneManager {

    public:
        void init();
        void shutdown() noexcept;
        void update();

        TprResult registerComponent(uint32_t componentSize, const char* componentName, uint32_t* pNewComponentId) noexcept;
        TprResult acquireComponent(const char* componentName, uint32_t* pComponentId) noexcept;
        TprResult createEntity(uint32_t componentIdCount, const uint32_t* pComponentIds, TprEntityHandle* pEntityHandle) noexcept;
        void destroyEntity(const TprEntityHandle* entityHandle) noexcept;
        TprResult copyEntityComponentData(const TprEntityHandle* entityHandle, uint32_t componentId, uint32_t start, uint32_t end, char* componentData) noexcept;
        TprResult readEntityComponent8bit(const TprEntityHandle* entityHandle, uint32_t componentId, uint32_t offset, uint8_t* data) noexcept;
        TprResult readEntityComponent16bit(const TprEntityHandle* entityHandle, uint32_t componentId, uint32_t offset, uint16_t* data) noexcept;
        TprResult readEntityComponent32bit(const TprEntityHandle* entityHandle, uint32_t componentId, uint32_t offset, uint32_t* data) noexcept;
        TprResult readEntityComponent64bit(const TprEntityHandle* entityHandle, uint32_t componentId, uint32_t offset, uint64_t* data) noexcept;
        TprResult writeEntityComponentData(const TprEntityHandle* entityHandle, uint32_t componentId, const char* componentData, uint32_t start, uint32_t end) noexcept;
        TprResult writeEntityComponent8bit(const TprEntityHandle* entityHandle, uint32_t componentId, uint8_t data, uint32_t offset) noexcept;
        TprResult writeEntityComponent16bit(const TprEntityHandle* entityHandle, uint32_t componentId, uint16_t data, uint32_t offset) noexcept;
        TprResult writeEntityComponent32bit(const TprEntityHandle* entityHandle, uint32_t componentId, uint32_t data, uint32_t offset) noexcept;
        TprResult writeEntityComponent64bit(const TprEntityHandle* entityHandle, uint32_t componentId, uint64_t data, uint32_t offset) noexcept;

    private:

        struct EntityEntry {
            size_t archetype;
            uint32_t id;
            uint32_t gen = 0;
        };

        size_t getArchetype(uint32_t componentCount, const uint32_t* components);

        std::shared_mutex mArchetypeArraysMutex;
        std::deque<Archetype> mArchetypes;
        std::vector<std::unique_ptr<std::shared_mutex>> mArchetypeMutexes;
        std::vector<size_t> mFreeArchetypes;

        std::shared_mutex mComponentInfoMutex;
        std::atomic<uint32_t> mComponentCounter = 0;
        std::vector<uint32_t> mComponentSizes;
        std::vector<std::string> mComponentNameRegister;

        std::shared_mutex mEntityEntriesMutex;
        std::atomic<uint32_t> mEntityCounter = 0;
        std::vector<EntityEntry> mEntityEntries;
        std::vector<uint32_t> mFreeEntityEntries;

};




#endif  // SCENE_MANAGER_SCENE_MANAGER_HPP_

