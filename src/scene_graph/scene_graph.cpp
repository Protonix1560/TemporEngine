
#include "scene_graph.hpp"
#include "archetype.hpp"
#include "core.hpp"
#include "logger.hpp"
#include "plugin_core.h"
#include "plugin_core_extender.hpp"
#include "settings.hpp"
#include "file_registry.hpp"

#include <cstdint>



SceneGraph::SceneGraph(Logger logger, Settings& rSettings, FileRegistry& rFileReg)
    : mLogger(logger), mrSettings(rSettings), mrFileReg(rFileReg) {
    auto size = mrSettings.createSettingIntegerOr(mrSettings.getRoot(), "ECS.componentChunkSize", 1024);
    if (size < 0) size = 1024;
    if (size > UINT32_MAX) size = 1024;
    mChunkSize = size;
}

SceneGraph::~SceneGraph() noexcept {}


expected<TprComponent, TprResult> SceneGraph::createComponent(uint32_t componentSize) noexcept {
    TprComponent handle;
    try {
        mComponents.try_emplace(mComponentCounter, componentSize);
        handle = construct_basic_handle<TprComponent>(mComponentCounter, 0, handle_type::component);
        mComponentCounter++;
        mLogger() << "Created component " << get_basic_handle_index(handle) << " with size " << componentSize << "\n";
    } catch (...) {
        return unexpected(TPR_UNKNOWN_ERROR);
    }
    return handle;
}


expected<uint32_t, TprResult> SceneGraph::getComponentSize(TprComponent component) noexcept {
    try {
        auto it = mComponents.find(get_basic_handle_index(component));
        if (it == mComponents.end()) return unexpected(TPR_ERROR_INVALID_VALUE);
        return it->second.size;
    } catch (...) {
        return unexpected(TPR_UNKNOWN_ERROR);
    }
}


void SceneGraph::destroyComponent(TprComponent component) noexcept {
    try {
        auto componentIt = mComponents.find(get_basic_handle_index(component));
        if (componentIt == mComponents.end()) return;

        /*
        // PROBABLY BUGGY, temporarily commenting it out
        // This was just an optimization
        // Next block of code doesn't rely on this block of code, so everything's fine

        for (auto& [components, archetypeEntry] : mArchetypes) {
            auto& archetype = archetypeEntry.archetype;
            if (archetype.contains(component)) {
                set_key<TprComponentWrapper> componentSet = components;
                componentSet.erase(component);
                auto downgradeIt = mArchetypes.find(componentSet);
                if (downgradeIt != mArchetypes.end()) {
                    Archetype& downgradeArchetype = downgradeIt->second.archetype;
                    // somehow move all entities from old archetype to new archetype
                    // or remove one line of components from old archetype and move all entities from new archetype to old archetype if that's more efficient

                    // if (downArchetype.size() < archetype.size()) {
                    //     archetype.destroyComponent(component);
                    //     archetype.reserve(archetype.size() + downArchetype.size());
                    //     for (size_t i = 0; i < downArchetype.size(); i++) {
                            
                    //     }
                    // }
                    // TODO: maybe fix that, will significantly optimize this case

                } else {
                    // just need to remove one line of components from that archetype,
                    // all Archetype* pointers of entities that belong to that archetype already point to it
                    archetype.destroyComponent(TprComponentWrapper{component});
                }
            }
        }
        */

        // temporarily just cycling through all entities and calling modifyEntitiyComponentSet
        // works, but isn't optimized at all
        for (size_t i = 0; i < mEntities.dense_size(); i++) {
            const EntityEntry& entry = mEntities[i];
            Archetype& archetype = entry.archetype->archetype;
            if (archetype.contains(component)) {
                std::vector<TprComponent> components;
                components.reserve(archetype.components().size() - 1);
                for (const auto& c : archetype.components()) {
                    if (c != component) {
                        components.push_back(c.wrapper.component);
                    }
                }
                modifyEntityComponentSet(TprEntity{mEntities.index(i)}, components.data(), components.size());

            }
        }

        mComponents.erase(componentIt);

        mLogger() << "Explicitly destroyed component " << get_basic_handle_index(component) << "\n";
    } catch (...) {}
}


expected<TprEntity, TprResult> SceneGraph::spawnEntity(const TprComponent* pComponents, uint32_t componentCount) noexcept {
    if (!pComponents) return unexpected(TPR_ERROR_INVALID_VALUE);

    try {

        set_key<TprComponentWrapper> componentSet(pComponents, pComponents + componentCount);

        auto it = mArchetypes.find(componentSet);
        if (it == mArchetypes.end()) {
            set_key<TprComponentInfo> componentInfoSet;
            for (const auto& component : componentSet) {
                if (!mComponents.contains(get_basic_handle_index(component.component))) return unexpected(TPR_ERROR_INVALID_VALUE);
                componentInfoSet.insert({component, mComponents.at(get_basic_handle_index(component.component)).size});
            }
            it = mArchetypes.try_emplace(componentSet, Archetype{mChunkSize, componentInfoSet}).first;
        }
        auto& archetypeEntry = it->second;

        auto [local, chunks] = archetypeEntry.archetype.spawn();
        for (auto& [chunk, component] : chunks) {
            uint32_t i = mChunks.insert({&archetypeEntry, chunk, component});
            archetypeEntry.chunks[get_basic_handle_index(component.wrapper.component)].try_emplace(chunk, i);
        }
        TprEntityWrapper global = TprEntityWrapper{mEntities.insert({&archetypeEntry, local})};
        return global.entity;

    } catch (...) {
        return unexpected(TPR_UNKNOWN_ERROR);
    }
}

 
void SceneGraph::killEntity(TprEntity entity) noexcept {
    try {
        size_t offset = mEntities.offset(entity.id);
        if (offset == mEntities.null_offset) return;
        const EntityEntry& entry = mEntities[offset];
        auto& archetypeEntry = *entry.archetype;
        auto& archetype = archetypeEntry.archetype;
        auto chunks = archetype.kill(entry.local);
        for (auto& [chunk, component] : chunks) {
            auto it = archetypeEntry.chunks.at(get_basic_handle_index(component.wrapper.component)).find(chunk);
            mChunks.index_erase(it->second);
            archetypeEntry.chunks.at(get_basic_handle_index(component.wrapper.component)).erase(it);
        }
        if (archetype.empty()) {
            auto it = mArchetypes.find(set_key<TprComponentWrapper>(archetype.components()));
            mArchetypes.erase(it);
        }
        mEntities.offset_erase(offset);
    } catch (...) {
        return;
    }
}


TprResult SceneGraph::copyEntityComponentData(TprEntity entity, TprComponent component, uint32_t start, uint32_t n, char* pData) noexcept {

    try {
        if (!mComponents.contains(get_basic_handle_index(component))) return TPR_ERROR_INVALID_VALUE;

        size_t offset = mEntities.offset(entity.id);
        if (offset == mEntities.null_offset) return TPR_ERROR_INVALID_VALUE;
        const EntityEntry& entry = mEntities[offset];
        Archetype& archetype = entry.archetype->archetype;

        auto dataExp = archetype.get(entry.local, TprComponentWrapper{component});
        if (!dataExp.has_value()) return dataExp.error();
        auto data = dataExp.value();

        auto widthExp = archetype.width(TprComponentWrapper{component});
        if (!widthExp.has_value()) return widthExp.error();
        uint32_t width = widthExp.value();
        if (offset + n > width) return TPR_ERROR_INVALID_VALUE;

        if (n != 0) {
            std::memcpy(pData, data + start, n);
        } else {
            std::memcpy(pData, data + start, width - start);
        }

    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }

    return TPR_SUCCESS;
}


TprResult SceneGraph::writeEntityComponentData(TprEntity entity, TprComponent component, const char* pData, uint32_t start, uint32_t n) noexcept {
    if (!pData) return TPR_ERROR_INVALID_VALUE;

    try {
        if (!mComponents.contains(get_basic_handle_index(component))) return TPR_ERROR_INVALID_VALUE;
        
        size_t offset = mEntities.offset(entity.id);
        if (offset == mEntities.null_offset) return TPR_ERROR_INVALID_VALUE;
        const EntityEntry& entry = mEntities[offset];
        Archetype& archetype = entry.archetype->archetype;

        auto dataExp = archetype.get(entry.local, component);
        if (!dataExp.has_value()) return dataExp.error();
        auto data = dataExp.value();

        auto widthExp = archetype.width(component);
        if (!widthExp.has_value()) return widthExp.error();
        uint32_t width = widthExp.value();
        if (offset + n > width) return TPR_ERROR_INVALID_VALUE;

        auto chunkExp = archetype.entityChunk(entry.local, component);
        if (!chunkExp.has_value()) return chunkExp.error();
        auto* chunk = chunkExp.value();
        chunk->incrementVersion();

        if (n != 0) {
            std::memcpy(data + start, pData, n);
        } else {
            std::memcpy(data + start, pData, width - start);
        }

    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }

    return TPR_SUCCESS;
}


TprResult SceneGraph::modifyEntityComponentSet(TprEntity entity, const TprComponent* pComponents, uint32_t componentCount) noexcept {
    if (!pComponents) return TPR_ERROR_INVALID_VALUE;

    // TODO: increment chunks versions

    try {
        size_t offset = mEntities.offset(entity.id);
        if (offset == mEntities.null_offset) return TPR_ERROR_INVALID_VALUE;
        EntityEntry& entry = mEntities[offset];
        ArchetypeEntry& oldArchetypeEntry = *entry.archetype;
        Archetype& oldArchetype = entry.archetype->archetype;

        set_key<TprComponentWrapper> componentSet(pComponents, pComponents + componentCount);

        auto it = mArchetypes.find(componentSet);
        if (it == mArchetypes.end()) {
            set_key<TprComponentInfo> componentInfoSet;
            for (const auto& component : componentSet) {
                if (!mComponents.contains(get_basic_handle_index(component.component))) return TPR_ERROR_INVALID_VALUE;
                componentInfoSet.insert({component, mComponents.at(get_basic_handle_index(component.component)).size});
            }
            it = mArchetypes.try_emplace(componentSet, Archetype{mChunkSize, componentInfoSet}).first;
        }
        auto& newArchetypeEntry = it->second;
        auto& newArchetype = newArchetypeEntry.archetype;

        auto [newLocal, chunks] = newArchetype.spawn();
        for (auto& [chunk, component] : chunks) {
            uint32_t i = mChunks.insert({&newArchetypeEntry, chunk, component});
            newArchetypeEntry.chunks.try_emplace(chunk, i);
        }
        for (TprComponentInfo component : newArchetype.components()) {
            if (oldArchetype.contains(component)) {
                auto dstExp = newArchetype.get(newLocal, component);
                if (!dstExp.has_value()) {
                    auto chunks = newArchetype.kill(newLocal);
                    for (auto& [chunk, component] : chunks) {
                        newArchetypeEntry.chunks.erase(chunk);
                    }
                    if (newArchetype.empty()) {
                        mArchetypes.erase(it);
                    }
                    return TPR_UNKNOWN_ERROR;
                }
                std::byte* dst = dstExp.value();

                auto srcExp = oldArchetype.get(entry.local, component);
                if (!srcExp.has_value()) {
                    auto chunks = newArchetype.kill(newLocal);
                    for (auto& [chunk, component] : chunks) {
                        newArchetypeEntry.chunks.erase(chunk);
                    }
                    if (newArchetype.empty()) {
                        mArchetypes.erase(it);
                    }
                    return TPR_UNKNOWN_ERROR;
                }
                std::byte* src = srcExp.value();
                
                auto widthExp = newArchetype.width(component);
                if (!widthExp.has_value()) {
                    auto chunks = newArchetype.kill(newLocal);
                    for (auto& [chunk, component] : chunks) {
                        newArchetypeEntry.chunks.erase(chunk);
                    }
                    if (newArchetype.empty()) {
                        mArchetypes.erase(it);
                    }
                    return TPR_UNKNOWN_ERROR;
                }
                std::memcpy(dst, src, widthExp.value());
            }
        }

        auto oldChunks = oldArchetype.kill(entry.local);
        for (auto& [chunk, component] : oldChunks) {
            oldArchetypeEntry.chunks.erase(chunk);
        }
        if (oldArchetype.empty()) {
            auto it = mArchetypes.find(set_key<TprComponentWrapper>(oldArchetype.components()));
            if (it != mArchetypes.end()) {
                mArchetypes.erase(it);
            }
        }

        entry.archetype = &newArchetypeEntry;
        entry.local = newLocal;

    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }

    return TPR_SUCCESS;
}


TprResult SceneGraph::getComponentChunkHandles(TprComponent component, TprFile file) noexcept {
    try {
        if (!mComponents.contains(get_basic_handle_index(component))) return TPR_ERROR_INVALID_VALUE;
        std::vector<TprComponentChunk> handles;
        for (auto& [componentSet, archetypeEntry] : mArchetypes) {
            auto& archetype = archetypeEntry.archetype;
            if (archetype.contains(component)) {
                for (auto& [local, global] : archetypeEntry.chunks[get_basic_handle_index(component)]) {
                    handles.push_back(construct_basic_handle<TprComponentChunk>(global, 0, handle_type::component_chunk));
                }
            }
        }
        TprResult result;
        result = mrFileReg.resize(file, handles.size() * sizeof(TprComponentChunk));
        if (result != TPR_SUCCESS) return result;
        result = mrFileReg.writeAt(file, 0, handles.size() * sizeof(TprComponentChunk), reinterpret_cast<const std::byte*>(handles.data()));

    } catch (...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}


uint32_t SceneGraph::getComponentChunkMaxElementCount() noexcept {
    return mChunkSize;
}


expected<uint32_t, TprResult> SceneGraph::getComponentChunkElementCount(TprComponentChunk chunk) noexcept {
    try {
        auto offset = mChunks.offset(get_basic_handle_index(chunk));
        if (offset == mChunks.null_offset) return TPR_ERROR_INVALID_VALUE;
        ChunkEntry& entry = mChunks[offset];
        auto exp = entry.archetype->archetype.chunk(entry.local, entry.component);
        if (!exp.has_value()) return exp.error();
        auto* chunk = exp.value();
        return chunk->count();
    } catch (...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}


TprResult SceneGraph::copyComponentChunkData(TprComponentChunk chunk, uint32_t start, uint32_t n, char* pData) noexcept {
    try {
        auto offset = mChunks.offset(get_basic_handle_index(chunk));
        if (offset == mChunks.null_offset) return TPR_ERROR_INVALID_VALUE;
        ChunkEntry& entry = mChunks[offset];
        auto exp = entry.archetype->archetype.chunk(entry.local, entry.component);
        if (!exp.has_value()) return exp.error();
        auto* chunk = exp.value();
        uint32_t size = chunk->count() * chunk->width();
        if (offset + n > size) return TPR_ERROR_INVALID_VALUE;
        if (n != 0) {
            std::memcpy(pData, chunk->data() + start, n);
        } else {
            std::memcpy(pData, chunk->data() + start, size - start);
        }
    } catch (...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}


TprResult SceneGraph::writeComponentChunkData(TprComponentChunk chunk, uint32_t version, const char* pData, uint32_t start, uint32_t n) noexcept {
    try {
        auto offset = mChunks.offset(get_basic_handle_index(chunk));
        if (offset == mChunks.null_offset) return TPR_ERROR_INVALID_VALUE;
        ChunkEntry& entry = mChunks[offset];
        auto exp = entry.archetype->archetype.chunk(entry.local, entry.component);
        if (!exp.has_value()) return exp.error();
        auto* chunk = exp.value();
        if (chunk->version() != version) return TPR_VERSION_MISMATCH;
        uint32_t size = chunk->count() * chunk->width();
        if (offset + n > size) return TPR_ERROR_INVALID_VALUE;
        if (n != 0) {
            std::memcpy(chunk->data() + start, pData, n);
        } else {
            std::memcpy(chunk->data() + start, pData, size - start);
        }
    } catch (...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}

expected<uint32_t, TprResult> SceneGraph::getComponentChunkVersion(TprComponentChunk chunk) noexcept {
    try {
        auto offset = mChunks.offset(get_basic_handle_index(chunk));
        if (offset == mChunks.null_offset) return TPR_ERROR_INVALID_VALUE;
        ChunkEntry& entry = mChunks[offset];
        auto exp = entry.archetype->archetype.chunk(entry.local, entry.component);
        if (!exp.has_value()) return exp.error();
        auto* chunk = exp.value();
        return chunk->version();
    } catch (...) {
        return TPR_UNKNOWN_ERROR;
    }
    return TPR_SUCCESS;
}