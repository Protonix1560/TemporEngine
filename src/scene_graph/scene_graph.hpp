

#ifndef SCENE_GRAPH_SCENE_GRAPH_HPP_
#define SCENE_GRAPH_SCENE_GRAPH_HPP_



#include "archetype.hpp"
#include "core.hpp"
#include "plugin_core.h"
#include "set_key.hpp"
#include "plugin_core_extender.hpp"
#include "sparse_set.hpp"

#include <stdint.h>
#include <unordered_map>

#include <xxhash.h>



// from "logger.hpp"
class Logger;

// from "settings.hpp"
class Settings;

// from "resource_registry.hpp"
class ResourceRegistry;


class SceneGraph {

    public:
        SceneGraph(Logger& rLogger, Settings& rSettings, ResourceRegistry& rResReg);
        ~SceneGraph() noexcept;

        expected<TprComponent, TprResult> createComponent(uint32_t componentSize) noexcept;
        expected<uint32_t, TprResult> getComponentSize(TprComponent component) noexcept;
        void destroyComponent(TprComponent component) noexcept;

        expected<TprEntity, TprResult> spawnEntity(const TprComponent* pComponents, uint32_t componentCount) noexcept;
        void killEntity(TprEntity entity) noexcept;

        TprResult modifyEntityComponentSet(TprEntity entity, const TprComponent* pComponents, uint32_t componentCount) noexcept;

        TprResult copyEntityComponentData(TprEntity entity, TprComponent component, uint32_t offset, uint32_t n, char* pData) noexcept;
        TprResult writeEntityComponentData(TprEntity entity, TprComponent component, const char* pData, uint32_t offset, uint32_t n) noexcept;

        TprResult getComponentChunkHandles(TprComponent component, TprResource resource) noexcept;
        uint32_t getComponentChunkMaxElementCount() noexcept;
        expected<uint32_t, TprResult> getComponentChunkElementCount(TprComponentChunk chunk) noexcept;
        expected<uint32_t, TprResult> getComponentChunkVersion(TprComponentChunk chunk) noexcept;
        TprResult copyComponentChunkData(TprComponentChunk chunk, uint32_t offset, uint32_t n, char* pData) noexcept;
        TprResult writeComponentChunkData(TprComponentChunk chunk, uint32_t version, const char* pData, uint32_t offset, uint32_t n) noexcept;

    private:

        struct ComponentEntry {
            uint32_t size;
        };

        struct ArchetypeEntry {
            Archetype archetype;
            std::unordered_map<uint32_t, std::unordered_map<uint32_t, uint32_t>> chunks;
        };

        struct EntityEntry {
            ArchetypeEntry* archetype;
            uint32_t local;
        };

        struct ChunkEntry {
            ArchetypeEntry* archetype;
            uint32_t local;
            TprComponentWrapper component;
        };

        Logger& mrLogger;
        Settings& mrSettings;
        ResourceRegistry& mrResReg;

        uint32_t mChunkSize;
        uint32_t mChunkCounter = 0;

        std::unordered_map<set_key<TprComponentWrapper>, ArchetypeEntry> mArchetypes;

        uint32_t mComponentCounter = 0;
        std::unordered_map<uint32_t, ComponentEntry> mComponents;
        sparse_set<EntityEntry> mEntities;

        sparse_set<ChunkEntry> mChunks;

};

REGISTER_TYPE_NAME_S(SceneGraph, "ScGr");




#endif  // SCENE_GRAPH_SCENE_GRAPH_HPP_
