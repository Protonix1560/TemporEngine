

#ifndef SCENE_GRAPH_SCENE_GRAPH_HPP_
#define SCENE_GRAPH_SCENE_GRAPH_HPP_



#include "archetype.hpp"
#include "core.hpp"
#include "set_key.hpp"
#include "plugin_core_extender.hpp"
#include "sparse_set.hpp"

#include <stdint.h>
#include <unordered_map>

#include <xxhash.h>



// from "logger.hpp"
class Logger;


class SceneGraph {

    public:
        SceneGraph(Logger& rLogger);
        ~SceneGraph() noexcept;
        void update();

        expected<TprComponent, TprResult> createComponent(uint32_t componentSize) noexcept;
        void destroyComponent(TprComponent component) noexcept;

        expected<TprEntity, TprResult> spawnEntity(const TprComponent* pComponents, uint32_t componentCount) noexcept;
        void killEntity(TprEntity entity) noexcept;

        TprResult modifyEntityComponentSet(TprEntity entity, const TprComponent* pComponents, uint32_t componentCount) noexcept;

        TprResult copyEntityComponentData(TprEntity entity, TprComponent component, uint32_t offset, uint32_t n, char* pData) noexcept;
        expected<uint8_t, TprResult> readEntityComponent8bit(TprEntity entity, TprComponent component, uint32_t offset) noexcept;
        expected<uint16_t, TprResult> readEntityComponent16bit(TprEntity entity, TprComponent component, uint32_t offset) noexcept;
        expected<uint32_t, TprResult> readEntityComponent32bit(TprEntity entity, TprComponent component, uint32_t offset) noexcept;
        expected<uint64_t, TprResult> readEntityComponent64bit(TprEntity entity, TprComponent component, uint32_t offset) noexcept;

        TprResult writeEntityComponentData(TprEntity entity, TprComponent component, const char* pData, uint32_t offset, uint32_t n) noexcept;
        TprResult writeEntityComponent8bit(TprEntity entity, TprComponent component, uint8_t data, uint32_t offset) noexcept;
        TprResult writeEntityComponent16bit(TprEntity entity, TprComponent component, uint16_t data, uint32_t offset) noexcept;
        TprResult writeEntityComponent32bit(TprEntity entity, TprComponent component, uint32_t data, uint32_t offset) noexcept;
        TprResult writeEntityComponent64bit(TprEntity entity, TprComponent component, uint64_t data, uint32_t offset) noexcept;

    private:

        struct EntityEntry {
            Archetype* archetype;
            TprEntityWrapper local;
        };

        struct ComponentEntry {
            uint32_t size;
        };

        Logger& mrLogger;

        std::unordered_map<set_key<TprComponentWrapper>, Archetype> mArchetypes;

        uint32_t mComponentCounter = 0;
        std::unordered_map<TprComponentWrapper, ComponentEntry> mComponents;
        sparse_set<EntityEntry> mEntities;

};

REGISTER_TYPE_NAME_S(SceneGraph, "ScGr");




#endif  // SCENE_GRAPH_SCENE_GRAPH_HPP_
