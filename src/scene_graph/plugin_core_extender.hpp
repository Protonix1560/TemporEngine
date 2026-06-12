
#ifndef SCENE_GRAPH_PLUGIN_CORE_EXTENDER_HPP_
#define SCENE_GRAPH_PLUGIN_CORE_EXTENDER_HPP_


#include "plugin_core.h"

#include <functional>


struct TprComponentWrapper {
    TprComponent component{};

    TprComponentWrapper() = default;
    TprComponentWrapper(const TprComponent& c) : component(c) {}
    TprComponentWrapper(uint64_t c) { component._d = c; }

    bool operator==(const TprComponent& other) const { return component._d == other._d; }
    bool operator==(const TprComponentWrapper& other) const { return component._d == other.component._d; }
    bool operator!=(const TprComponent& other) const { return component._d != other._d; }
    bool operator!=(const TprComponentWrapper& other) const { return component._d != other.component._d; }
    bool operator<(const TprComponent& other) const { return component._d < other._d; }
    bool operator<(const TprComponentWrapper& other) const { return component._d < other.component._d; }
    bool operator>(const TprComponent& other) const { return component._d > other._d; }
    bool operator>(const TprComponentWrapper& other) const { return component._d > other.component._d; }
    bool operator<=(const TprComponent& other) const { return component._d <= other._d; }
    bool operator<=(const TprComponentWrapper& other) const { return component._d <= other.component._d; }
    bool operator>=(const TprComponent& other) const { return component._d >= other._d; }
    bool operator>=(const TprComponentWrapper& other) const { return component._d >= other.component._d; }
};

template <>
struct std::hash<TprComponentWrapper> {
    size_t operator()(const TprComponentWrapper& c) const {
        return c.component._d;
    }
};


struct TprComponentInfo {
    TprComponentWrapper wrapper{};
    uint32_t size = 0;

    TprComponentInfo() = default;
    TprComponentInfo(uint64_t c) : wrapper(c) {}
    TprComponentInfo(const TprComponent& c) : wrapper(c) {}
    TprComponentInfo(const TprComponentWrapper& w) : wrapper(w) {}
    TprComponentInfo(uint64_t c, uint32_t size) : wrapper(c), size(size) {}
    TprComponentInfo(const TprComponent& c, uint32_t size) : wrapper(c), size(size) {}
    TprComponentInfo(const TprComponentWrapper& w, uint32_t size) : wrapper(w), size(size) {}
    
    operator TprComponentWrapper() const { return wrapper; }

    bool operator==(const TprComponent& other) const { return wrapper.component._d == other._d; }
    bool operator==(const TprComponentWrapper& other) const { return wrapper.component._d == other.component._d; }
    bool operator==(const TprComponentInfo& other) const { return wrapper.component._d == other.wrapper.component._d; }
    bool operator!=(const TprComponent& other) const { return wrapper.component._d != other._d; }
    bool operator!=(const TprComponentWrapper& other) const { return wrapper.component._d != other.component._d; }
    bool operator!=(const TprComponentInfo& other) const { return wrapper.component._d != other.wrapper.component._d; }
    bool operator<(const TprComponent& other) const { return wrapper.component._d < other._d; }
    bool operator<(const TprComponentWrapper& other) const { return wrapper.component._d < other.component._d; }
    bool operator<(const TprComponentInfo& other) const { return wrapper.component._d < other.wrapper.component._d; }
    bool operator>(const TprComponent& other) const { return wrapper.component._d > other._d; }
    bool operator>(const TprComponentWrapper& other) const { return wrapper.component._d > other.component._d; }
    bool operator>(const TprComponentInfo& other) const { return wrapper.component._d > other.wrapper.component._d; }
    bool operator<=(const TprComponent& other) const { return wrapper.component._d <= other._d; }
    bool operator<=(const TprComponentWrapper& other) const { return wrapper.component._d <= other.component._d; }
    bool operator<=(const TprComponentInfo& other) const { return wrapper.component._d <= other.wrapper.component._d; }
    bool operator>=(const TprComponent& other) const { return wrapper.component._d >= other._d; }
    bool operator>=(const TprComponentWrapper& other) const { return wrapper.component._d >= other.component._d; }
    bool operator>=(const TprComponentInfo& other) const { return wrapper.component._d >= other.wrapper.component._d; }
};

template <>
struct std::hash<TprComponentInfo> {
    size_t operator()(const TprComponentInfo& c) const {
        return c.wrapper.component._d;
    }
};


struct TprEntityWrapper {
    TprEntity entity{};

    TprEntityWrapper() = default;
    TprEntityWrapper(uint32_t e) { entity.id = e; }
    TprEntityWrapper(const TprEntity& e) : entity(e) {}

    bool operator==(const TprEntity& other) const { return entity.id == other.id; }
    bool operator==(const TprEntityWrapper& other) const { return entity.id == other.entity.id; }
    bool operator!=(const TprEntity& other) const { return entity.id != other.id; }
    bool operator!=(const TprEntityWrapper& other) const { return entity.id != other.entity.id; }
    bool operator<(const TprEntity& other) const { return entity.id < other.id; }
    bool operator<(const TprEntityWrapper& other) const { return entity.id < other.entity.id; }
    bool operator>(const TprEntity& other) const { return entity.id > other.id; }
    bool operator>(const TprEntityWrapper& other) const { return entity.id > other.entity.id; }
    bool operator<=(const TprEntity& other) const { return entity.id <= other.id; }
    bool operator<=(const TprEntityWrapper& other) const { return entity.id <= other.entity.id; }
    bool operator>=(const TprEntity& other) const { return entity.id >= other.id; }
    bool operator>=(const TprEntityWrapper& other) const { return entity.id >= other.entity.id; }
};

template <>
struct std::hash<TprEntityWrapper> {
    size_t operator()(const TprEntityWrapper& c) const {
        return c.entity.id;
    }
};


#endif  // SCENE_GRAPH_PLUGIN_CORE_EXTENDER_HPP_
