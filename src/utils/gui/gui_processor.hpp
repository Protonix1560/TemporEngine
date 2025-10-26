
#include "core.hpp"
#include "gpu_backend.hpp"

#include <filesystem>
#include <vector>
#include <cstdint>
#include <string>



class GUIBlock;



struct GUIBlock {

    public:

        enum class DType {
            Automatic = 0,
            Absolute = 1,
            ScreenPercentage = 2
        };

        enum class BlockType {
            Undefined = 0,
            Array = 1
        };

        enum class Orientation {
            Horizontal = 0,
            Vertical = 1
        };

        enum class ArrayAlign {
            Start = 0,
            End = 1,
            Centre = 2,
            SpaceBetween = 3,
            SpaceAround = 4
        };

        BlockType type = BlockType::Undefined;

        union TypeData {
            struct ArrayData {
                Orientation orientation = Orientation::Horizontal;
                ArrayAlign alignX = ArrayAlign::Start;
                ArrayAlign alignY = ArrayAlign::Start;
            } arrayData;
            TypeData() {}
        } typeData;

        float innerPadding = 0.0f;
        float elementsPadding = 0.0f;

        std::vector<uint32_t> childrenIndices;

        std::string text;

        float setWidth = 0.0f;
        float setHeight = 0.0f;
        DType widthType = DType::Automatic;
        DType heightType = DType::Automatic;

        GUIBlock() = default;

    private:
        float x;
        float y;
        float width;
        float height;
        uint32_t parentIndex = UINT32_MAX;

        friend class GUIProcessor;

};



struct GUISystem {
    std::vector<GUIBlock> blocks;
    GUIBlock::DType rootXType = GUIBlock::DType::Automatic;
    GUIBlock::DType rootYType = GUIBlock::DType::Automatic;
    float rootSetX = 0.0f;
    float rootSetY = 0.0f;
};



class TPMLParser {
    public:
        GUISystem parse(std::filesystem::path filepath);

        TPMLParser(const GlobalServiceLocator* serviceLocator)
            : mpServiceLocator(serviceLocator) {}

    private:
        const GlobalServiceLocator* mpServiceLocator;

};



class GUIProcessor {

    public:
        void init(const GlobalServiceLocator* serviceLocator, const GUISystem& system);
        void update(uint32_t windowWidth, uint32_t windowHeight);
        std::vector<DebugLineVertex> getDebugViewLines() const;
        void destroy() noexcept;

    private:
        const GlobalServiceLocator* mpServiceLocator;
        GUISystem mSystem;
        std::vector<uint32_t> mLeavesIndices;
        uint32_t mRootIndex = UINT32_MAX;
        uint32_t mWindowWidth;
        uint32_t mWindowHeight;
        float mDpi = 93.78f;

};


