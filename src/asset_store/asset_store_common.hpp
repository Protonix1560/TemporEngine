
#ifndef ASSET_STORE_COMMON_HPP_
#define ASSET_STORE_COMMON_HPP_


#include <cstdint>
#include <ostream>
#include <vector>


struct MeshData {

    struct Primitive {
        uint32_t offset = 0;
        uint32_t count = 0;
    };

    struct Vertex {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    using Index = uint32_t;

    struct {
        uint32_t primitiveCount = 0;
        uint32_t primitiveOffset = 0;
        uint32_t indexCount = 0;
        uint32_t indexOffset = 0;
        uint32_t vertexCount = 0;
        uint32_t vertexOffset = 0;
    } header;
    std::vector<std::byte> data;
};


class AssetStore;

struct MeshIdentity {
    private:
        uint32_t id;
        MeshIdentity(uint32_t id) noexcept : id(id) {}
        friend class AssetStore;
        friend class std::hash<MeshIdentity>;

    public:
        MeshIdentity() noexcept : id(0) {}

        std::strong_ordering operator<=>(const MeshIdentity& other) const noexcept {
            return id <=> other.id;
        }
        bool operator==(const MeshIdentity& other) const noexcept = default;

        friend std::ostream& operator<<(std::ostream& stream, const MeshIdentity& id) {
            stream << id.id;
            return stream;
        }
};

template <>
class std::hash<MeshIdentity> {
    public:
        size_t operator()(const MeshIdentity& id) const noexcept {
            return id.id;
        }
};


#endif  // ASSET_STORE_COMMON_HPP_
