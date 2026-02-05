

#ifndef GUI_GUI_PROCESSOR_HPP_
#define GUI_GUI_PROCESSOR_HPP_



#include "hardware_common_structs.hpp"

#include <vector>
#include <cstdint>
#include <string>



class GUIBlock;



struct GUIBlock {

    public:

        enum class DType {
            Automatic = 0,
            Absolute = 1,
            ScreenPercentage = 2,
            ScreenRelative = 3,
            Expand = 4
        };

        enum class BlockType {
            Plain = 0,
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

        BlockType type = BlockType::Plain;

        union TypeData {
            struct {
                Orientation orientation = Orientation::Horizontal;
                ArrayAlign alignX = ArrayAlign::Start;
                ArrayAlign alignY = ArrayAlign::Start;
            } array;
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

        uint32_t bgColour = 0x888888FF;

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



class GUIProcessor {

    public:
        void init(const GUISystem& system);
        void shutdown() noexcept;
        void update(uint32_t windowWidth, uint32_t windowHeight);
        std::vector<DebugLineVertex> getDebugViewLines() const;
        GUIDrawDesc getDrawDesc() const;

    private:
        GUISystem mSystem;
        std::vector<uint32_t> mLeavesIndices;
        uint32_t mRootIndex = UINT32_MAX;
        uint32_t mWindowWidth;
        uint32_t mWindowHeight;
        float mDpi = 93.78f;

};




#endif  // GUI_GUI_PROCESSOR_HPP_

