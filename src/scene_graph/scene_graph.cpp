
#include "scene_graph.hpp"
#include "archetype.hpp"
#include "core.hpp"
#include "logger.hpp"

#include <cstdint>



SceneGraph::SceneGraph(Logger& rLogger) : mrLogger(rLogger) {}

SceneGraph::~SceneGraph() noexcept {}

void SceneGraph::update() {}


expected<TprComponent, TprResult> SceneGraph::createComponent(uint32_t componentSize) noexcept {
    try {
        if (mComponentCounter == UINT32_MAX) return unexpected(TPR_COUNT_OVERFLOW);
        TprComponent handle = construct_basic_handle<TprComponent>(mComponentCounter++, 0, handle_type::component);
        mComponents.try_emplace(TprComponentWrapper{handle}, componentSize);
        mrLogger << logPrxScGr() << "Created component " << get_basic_handle_index(handle) << " with size " << componentSize << "\n";
        return handle;
    } catch (...) {
        return unexpected(TPR_UNKNOWN_ERROR);
    }
}


void SceneGraph::destroyComponent(TprComponent component) noexcept {
    try {

        for (auto& [components, archetype] : mArchetypes) {
            if (archetype.contains(TprComponentWrapper{component})) {
                set_key<TprComponentWrapper> componentSet = components;
                componentSet.erase(component);
                auto downgradeIt = mArchetypes.find(componentSet);
                if (downgradeIt != mArchetypes.end()) {
                    Archetype& downgradeArchetype = downgradeIt->second;
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

        // temporarily just cycling through all entities and calling modifyEntitiyComponentSet
        // works, but isn't optimized
        for (size_t i = 0; i < mEntities.dense_size(); i++) {
            const EntityEntry& entry = mEntities[i];
            Archetype& archetype = *entry.archetype;
            if (archetype.contains(TprComponentWrapper{component})) {
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

        mComponents.erase(mComponents.find(TprComponentWrapper{component}));

        mrLogger << logPrxScGr() << "Explicitly destroyed component " << get_basic_handle_index(component) << "\n";
    } catch (...) {
        return;
    }
}


expected<TprEntity, TprResult> SceneGraph::spawnEntity(const TprComponent* pComponents, uint32_t componentCount) noexcept {
    
    try {

        set_key<TprComponentWrapper> componentSet(pComponents, pComponents + componentCount);

        auto it = mArchetypes.find(componentSet);
        if (it == mArchetypes.end()) {
            set_key<TprComponentInfo> componentInfoSet;
            componentInfoSet.reserve(componentSet.size());
            for (const auto& component : componentSet) {
                componentInfoSet.insert({component, mComponents.at(component).size});
            }
            it = mArchetypes.try_emplace(componentSet, componentInfoSet).first;
        }
        auto& archetype = it->second;

        TprEntityWrapper local = archetype.spawn();
        TprEntityWrapper global = TprEntityWrapper{mEntities.insert(EntityEntry{&archetype, local})};
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
        Archetype& archetype = *entry.archetype;
        archetype.kill(entry.local);
        if (archetype.empty()) {
            auto it = mArchetypes.find(set_key<TprComponentWrapper>(archetype.components()));
            if (it != mArchetypes.end()) {
                mArchetypes.erase(it);
            }
        }
        mEntities.offset_erase(offset);

    } catch(...) {
        return;
    }
}



TprResult SceneGraph::copyEntityComponentData(TprEntity entity, TprComponent component, uint32_t start, uint32_t n, char* pData) noexcept {

    try {
        size_t offset = mEntities.offset(entity.id);
        if (offset == mEntities.null_offset) return TPR_INVALID_VALUE;
        const EntityEntry& entry = mEntities[offset];
        Archetype& archetype = *entry.archetype;

        auto dataExp = archetype.get(entry.local, TprComponentWrapper{component});
        if (!dataExp.has_value()) return dataExp.error();
        auto data = dataExp.value();

        auto widthExp = archetype.width(TprComponentWrapper{component});
        if (!widthExp.has_value()) return widthExp.error();
        uint32_t width = widthExp.value();
        if (offset + n > width) return TPR_INVALID_VALUE;

        if (n != 0) {
            std::memcpy(pData, data + start, n);
        } else {
            auto widthExp = archetype.width(TprComponentWrapper{component});
            if (!widthExp.has_value()) return widthExp.error();
            std::memcpy(pData, data + start, widthExp.value() - start);
        }

    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }

    return TPR_SUCCESS;
}


TprResult SceneGraph::writeEntityComponentData(TprEntity entity, TprComponent component, const char* pData, uint32_t start, uint32_t n) noexcept {

    try {
        size_t offset = mEntities.offset(entity.id);
        if (offset == mEntities.null_offset) return TPR_INVALID_VALUE;
        const EntityEntry& entry = mEntities[offset];
        Archetype& archetype = *entry.archetype;

        auto dataExp = archetype.get(entry.local, TprComponentWrapper{component});
        if (!dataExp.has_value()) return dataExp.error();
        auto data = dataExp.value();

        auto widthExp = archetype.width(TprComponentWrapper{component});
        if (!widthExp.has_value()) return widthExp.error();
        uint32_t width = widthExp.value();
        if (offset + n > width) return TPR_INVALID_VALUE;

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


expected<uint8_t, TprResult> SceneGraph::readEntityComponent8bit(TprEntity entity, TprComponent component, uint32_t start) noexcept {

    try {
        size_t offset = mEntities.offset(entity.id);
        if (offset == mEntities.null_offset) return TPR_INVALID_VALUE;
        const EntityEntry& entry = mEntities[offset];
        Archetype& archetype = *entry.archetype;

        auto dataExp = archetype.get(entry.local, TprComponentWrapper{component});
        if (!dataExp.has_value()) return dataExp.error();
        auto data = dataExp.value();

        uint8_t dest;

        auto widthExp = archetype.width(TprComponentWrapper{component});
        if (!widthExp.has_value()) return widthExp.error();
        uint32_t width = widthExp.value();
        if (start + sizeof(dest) > width) return TPR_INVALID_VALUE;

        std::memcpy(&dest, data + start, sizeof(dest));
        return dest;

    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }
}


expected<uint16_t, TprResult> SceneGraph::readEntityComponent16bit(TprEntity entity, TprComponent component, uint32_t start) noexcept {

    try {
        size_t offset = mEntities.offset(entity.id);
        if (offset == mEntities.null_offset) return TPR_INVALID_VALUE;
        const EntityEntry& entry = mEntities[offset];
        Archetype& archetype = *entry.archetype;

        auto dataExp = archetype.get(entry.local, TprComponentWrapper{component});
        if (!dataExp.has_value()) return dataExp.error();
        auto data = dataExp.value();

        uint16_t dest;

        auto widthExp = archetype.width(TprComponentWrapper{component});
        if (!widthExp.has_value()) return widthExp.error();
        uint32_t width = widthExp.value();
        if (start + sizeof(dest) > width) return TPR_INVALID_VALUE;

        std::memcpy(&dest, data + start, sizeof(dest));
        return dest;

    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }

}


expected<uint32_t, TprResult> SceneGraph::readEntityComponent32bit(TprEntity entity, TprComponent component, uint32_t start) noexcept {

    try {
        size_t offset = mEntities.offset(entity.id);
        if (offset == mEntities.null_offset) return TPR_INVALID_VALUE;
        const EntityEntry& entry = mEntities[offset];
        Archetype& archetype = *entry.archetype;

        auto dataExp = archetype.get(entry.local, TprComponentWrapper{component});
        if (!dataExp.has_value()) return dataExp.error();
        auto data = dataExp.value();

        uint32_t dest;

        auto widthExp = archetype.width(TprComponentWrapper{component});
        if (!widthExp.has_value()) return widthExp.error();
        uint32_t width = widthExp.value();
        if (start + sizeof(dest) > width) return TPR_INVALID_VALUE;

        std::memcpy(&dest, data + start, sizeof(dest));
        return dest;

    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }
}


expected<uint64_t, TprResult> SceneGraph::readEntityComponent64bit(TprEntity entity, TprComponent component, uint32_t start) noexcept {

    try {
        size_t offset = mEntities.offset(entity.id);
        if (offset == mEntities.null_offset) return TPR_INVALID_VALUE;
        const EntityEntry& entry = mEntities[offset];
        Archetype& archetype = *entry.archetype;

        auto dataExp = archetype.get(entry.local, TprComponentWrapper{component});
        if (!dataExp.has_value()) return dataExp.error();
        auto data = dataExp.value();

        uint64_t dest;

        auto widthExp = archetype.width(TprComponentWrapper{component});
        if (!widthExp.has_value()) return widthExp.error();
        uint32_t width = widthExp.value();
        if (start + sizeof(dest) > width) return TPR_INVALID_VALUE;

        std::memcpy(&dest, data + start, sizeof(dest));
        return dest;

    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }
}


TprResult SceneGraph::writeEntityComponent8bit(TprEntity entity, TprComponent component, uint8_t data, uint32_t start) noexcept {

    try {
        size_t offset = mEntities.offset(entity.id);
        if (offset == mEntities.null_offset) return TPR_INVALID_VALUE;
        const EntityEntry& entry = mEntities[offset];
        Archetype& archetype = *entry.archetype;

        auto dataExp = archetype.get(entry.local, TprComponentWrapper{component});
        if (!dataExp.has_value()) return dataExp.error();
        auto data = dataExp.value();

        auto widthExp = archetype.width(TprComponentWrapper{component});
        if (!widthExp.has_value()) return widthExp.error();
        uint32_t width = widthExp.value();
        if (start + sizeof(data) > width) return TPR_INVALID_VALUE;

        std::memcpy(data + start, &data, sizeof(data));

    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }

    return TPR_SUCCESS;
}


TprResult SceneGraph::writeEntityComponent16bit(TprEntity entity, TprComponent component, uint16_t data, uint32_t start) noexcept {

    try {
        size_t offset = mEntities.offset(entity.id);
        if (offset == mEntities.null_offset) return TPR_INVALID_VALUE;
        const EntityEntry& entry = mEntities[offset];
        Archetype& archetype = *entry.archetype;

        auto dataExp = archetype.get(entry.local, TprComponentWrapper{component});
        if (!dataExp.has_value()) return dataExp.error();
        auto data = dataExp.value();

        auto widthExp = archetype.width(TprComponentWrapper{component});
        if (!widthExp.has_value()) return widthExp.error();
        uint32_t width = widthExp.value();
        if (start + sizeof(data) > width) return TPR_INVALID_VALUE;

        std::memcpy(data + start, &data, sizeof(data));

    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }

    return TPR_SUCCESS;
}


TprResult SceneGraph::writeEntityComponent32bit(TprEntity entity, TprComponent component, uint32_t data, uint32_t start) noexcept {

    try {
        size_t offset = mEntities.offset(entity.id);
        if (offset == mEntities.null_offset) return TPR_INVALID_VALUE;
        const EntityEntry& entry = mEntities[offset];
        Archetype& archetype = *entry.archetype;

        auto dataExp = archetype.get(entry.local, TprComponentWrapper{component});
        if (!dataExp.has_value()) return dataExp.error();
        auto data = dataExp.value();

        auto widthExp = archetype.width(TprComponentWrapper{component});
        if (!widthExp.has_value()) return widthExp.error();
        uint32_t width = widthExp.value();
        if (start + sizeof(data) > width) return TPR_INVALID_VALUE;

        std::memcpy(data + start, &data, sizeof(data));

    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }

    return TPR_SUCCESS;
}


TprResult SceneGraph::writeEntityComponent64bit(TprEntity entity, TprComponent component, uint64_t data, uint32_t start) noexcept {

    try {
        size_t offset = mEntities.offset(entity.id);
        if (offset == mEntities.null_offset) return TPR_INVALID_VALUE;
        const EntityEntry& entry = mEntities[offset];
        Archetype& archetype = *entry.archetype;

        auto dataExp = archetype.get(entry.local, TprComponentWrapper{component});
        if (!dataExp.has_value()) return dataExp.error();
        auto data = dataExp.value();

        auto widthExp = archetype.width(TprComponentWrapper{component});
        if (!widthExp.has_value()) return widthExp.error();
        uint32_t width = widthExp.value();
        if (start + sizeof(data) > width) return TPR_INVALID_VALUE;

        std::memcpy(data + start, &data, sizeof(data));

    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }

    return TPR_SUCCESS;
}


TprResult SceneGraph::modifyEntityComponentSet(TprEntity entity, const TprComponent* pComponents, uint32_t componentCount) noexcept {

    try {
        size_t offset = mEntities.offset(entity.id);
        if (offset == mEntities.null_offset) return TPR_INVALID_VALUE;
        EntityEntry& entry = mEntities[offset];
        Archetype& oldArchetype = *entry.archetype;

        set_key<TprComponentWrapper> componentSet(pComponents, pComponents + componentCount);

        auto it = mArchetypes.find(componentSet);
        if (it == mArchetypes.end()) {
            set_key<TprComponentInfo> componentInfoSet;
            componentInfoSet.reserve(componentSet.size());
            for (const auto& component : componentSet) {
                componentInfoSet.insert({component, mComponents.at(component).size});
            }
            it = mArchetypes.try_emplace(componentSet, componentInfoSet).first;
        }
        auto& newArchetype = it->second;

        TprEntityWrapper newLocal = newArchetype.spawn();
        for (TprComponentInfo component : newArchetype.components()) {
            if (oldArchetype.contains(component)) {
                auto dstExp = newArchetype.get(newLocal, component);
                if (!dstExp.has_value()) {
                    newArchetype.kill(newLocal);
                    if (newArchetype.empty()) {
                        mArchetypes.erase(it);
                    }
                    return TPR_UNKNOWN_ERROR;
                }
                std::byte* dst = dstExp.value();

                auto srcExp = oldArchetype.get(entry.local, component);
                if (!srcExp.has_value()) {
                    newArchetype.kill(newLocal);
                    if (newArchetype.empty()) {
                        mArchetypes.erase(it);
                    }
                    return TPR_UNKNOWN_ERROR;
                }
                std::byte* src = srcExp.value();
                
                auto widthExp = newArchetype.width(component);
                if (!widthExp.has_value()) {
                    newArchetype.kill(newLocal);
                    if (newArchetype.empty()) {
                        mArchetypes.erase(it);
                    }
                    return TPR_UNKNOWN_ERROR;
                }
                std::memcpy(dst, src, widthExp.value());
            }
        }

        oldArchetype.kill(entry.local);
        if (oldArchetype.empty()) {
            auto it = mArchetypes.find(set_key<TprComponentWrapper>(oldArchetype.components()));
            if (it != mArchetypes.end()) {
                mArchetypes.erase(it);
            }
        }

        entry.archetype = &newArchetype;
        entry.local = newLocal;

    } catch(...) {
        return TPR_UNKNOWN_ERROR;
    }

    return TPR_SUCCESS;

    return TPR_SUCCESS;
}



