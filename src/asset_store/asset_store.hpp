
#ifndef ASSET_STORE_ASSET_STORE_HPP_
#define ASSET_STORE_ASSET_STORE_HPP_


#include "plugin_core.h"
#include "core.hpp"

#include <string>
#include <cstdint>
#include <type_traits>
#include <vector>
#include <variant>



struct SlotConstraints {
    public:
        bool operator()() { return true; }
};



struct SlotPFunc {
    public:
        float operator()(float p);
};



struct Slot {
    std::string name;
    SlotConstraints constraints;
    uint32_t multMax;
    uint32_t multMin;
    SlotPFunc pFunc;
    float p;
};



struct Asset {
    std::vector<Slot> slots;
    std::vector<std::byte> data;
    uint32_t generation = 0;
    bool actual = true;
};

struct AssetModel : public Asset {

    struct Vertex {
        struct Pos {
            float x, y, z;
        } pos;
    };

    using Index = uint32_t;

    struct Header {
        uint32_t vertexOffset;
        uint32_t vertexCount;
        uint32_t indexOffset;
        uint32_t indexCount;
    } header;
    static_assert(std::is_standard_layout_v<Header>, "");
};



class AssetStore {

    public:
        AssetStore();

        void init();
        void update();
        void shutdown() noexcept;

        void validateHandle(TprAsset handle);
        Expected<TprAsset, TprResult> parseAsset(const TprAssetParseInfo* info) noexcept;
        Expected<TprAsset, TprResult> loadAsset(const TprAssetLoadInfo* info) noexcept;
        void destroyAsset(TprAsset asset) noexcept;


    private:
        ServiceLocator mLoc;

        std::vector<std::variant<
            AssetModel
        >> mAssets;
        std::vector<size_t> mFreeAssets;


};



#endif  // ASSET_STORE_ASSET_STORE_HPP_

