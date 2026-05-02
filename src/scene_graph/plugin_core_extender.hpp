
#ifndef SCENE_GRAPH_PLUGIN_CORE_EXTENDER_HPP_
#define SCENE_GRAPH_PLUGIN_CORE_EXTENDER_HPP_


#include "plugin_core.h"

#include <functional>


struct TprComponentWrapper {
    TprComponent component{};
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
