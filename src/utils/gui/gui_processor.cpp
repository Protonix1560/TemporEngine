
#include "gui_processor.hpp"
#include "core.hpp"
#include "gpu_backend.hpp"
#include "logger.hpp"

#include <cstdint>
#include <glm/packing.hpp>
#include <set>



void GUIProcessor::init(const GlobalServiceLocator* serviceLocator, const GUISystem& system) {

    mpServiceLocator = serviceLocator;
    auto& log = mpServiceLocator->get<Logger>();

    mSystem = system;

    // setting parent index
    for (uint32_t i = 0; i < mSystem.blocks.size(); i++) {
        for (const auto& blockChildIndex : mSystem.blocks[i].childrenIndices) {
            mSystem.blocks[blockChildIndex].parentIndex = i;
        }
    }
    
    // finding the root block
    for (uint32_t i = 0; i < mSystem.blocks.size(); i++) {
        if (mSystem.blocks[i].parentIndex == UINT32_MAX) {
            if (mRootIndex != UINT32_MAX) {
                throw Exception(ErrCode::FormatError, "Invalid GUI system block tree: detected multiple root blocks.\nThe GUI system block tree must be a valid graph tree");
            }
            mRootIndex = i;
            break;
        }
    }
    if (mRootIndex == UINT32_MAX) throw Exception(ErrCode::FormatError, "Invalid GUI system block tree: detected no root blocks.\nThe GUI system block tree must be a valid graph tree");

    // finding leaves of the system block graph tree
    // and determining if the graph is a tree or not
    {
        std::vector<bool> visited(mSystem.blocks.size(), false);
        std::vector<std::pair<uint32_t, uint32_t>> stack = {{mRootIndex, 0}};

        while (!stack.empty()) {
            auto [index, depth] = stack.back();
            stack.pop_back();

            if (visited[index]) throw Exception(ErrCode::FormatError, "Invalid GUI system block tree: detected a cycle.\nThe GUI system block tree must be a valid graph tree");
            visited[index] = true;

            GUIBlock& block = mSystem.blocks[index];
            if (block.childrenIndices.empty()) mLeavesIndices.push_back(index);

            for (uint32_t child : block.childrenIndices) {
                stack.push_back({child, depth + 1});
            }
        }

        for (bool v : visited) {
            if (!v) throw Exception(ErrCode::FormatError, "Invalid GUI system block tree: detected unreachable blocks.\nThe GUI system block tree must be a valid graph tree");
        }
    }
}



void GUIProcessor::update(uint32_t windowWidth, uint32_t windowHeight) {

    mWindowWidth = windowWidth;
    mWindowHeight = windowHeight;

    // setting relative position
    {
        std::set<uint32_t> stack;
        std::set<uint32_t> nextStack;
        stack.insert(mLeavesIndices.begin(), mLeavesIndices.end());

        while (!stack.empty()) {
            for (uint32_t index : stack) {
                GUIBlock& block = mSystem.blocks[index];

                // calculating children's positions and block's dimensions
                if (block.type == GUIBlock::BlockType::Array) {
                    if (block.typeData.arrayData.orientation == GUIBlock::Orientation::Horizontal) {
                        float blockContentWidth = 0.0f;
                        float blockContentHeight = 0.0f;
                        for (uint32_t childIndex : block.childrenIndices) {
                            GUIBlock& child = mSystem.blocks[childIndex];
                            blockContentWidth += child.width;
                            if (child.height > blockContentHeight) blockContentHeight = child.height;
                        }
                        blockContentWidth += block.elementsPadding * static_cast<float>(block.childrenIndices.size() - 1);
                        switch (block.widthType) {
                            case GUIBlock::DType::Automatic: block.width = blockContentWidth + block.innerPadding * 2.0f; break;
                            case GUIBlock::DType::Absolute: block.width = block.setWidth; break;
                            case GUIBlock::DType::ScreenPercentage: block.width = mWindowWidth * block.setWidth / 100.0f / mDpi * 2.54f; break;
                        }
                        switch (block.heightType) {
                            case GUIBlock::DType::Automatic: block.height = blockContentHeight + block.innerPadding * 2.0f; break;
                            case GUIBlock::DType::Absolute: block.height = block.setHeight; break;
                            case GUIBlock::DType::ScreenPercentage: block.height = mWindowHeight * block.setHeight / 100.0f / mDpi * 2.54f; break;
                        }

                        float prevX;
                        switch (block.typeData.arrayData.alignX) {
                            case GUIBlock::ArrayAlign::Start: prevX = block.innerPadding - block.elementsPadding; break;
                            case GUIBlock::ArrayAlign::End: prevX = block.width - blockContentWidth - block.innerPadding - block.elementsPadding; break;
                            case GUIBlock::ArrayAlign::Centre: prevX = (block.width - blockContentWidth) / 2.0f - block.elementsPadding; break;
                            case GUIBlock::ArrayAlign::SpaceBetween: prevX = block.innerPadding - block.elementsPadding; break;
                            case GUIBlock::ArrayAlign::SpaceAround: prevX = block.innerPadding - block.elementsPadding + (block.width - block.innerPadding * 2.0f - blockContentWidth) / (block.childrenIndices.size() + 1.0f); break;
                        }

                        for (uint32_t childIndex : block.childrenIndices) {
                            GUIBlock& child = mSystem.blocks[childIndex];

                            switch (block.typeData.arrayData.alignX) {
                                case GUIBlock::ArrayAlign::Start:
                                case GUIBlock::ArrayAlign::End:
                                case GUIBlock::ArrayAlign::Centre:
                                    child.x = prevX + block.elementsPadding;
                                    prevX = child.x + child.width;
                                    break;
                                case GUIBlock::ArrayAlign::SpaceBetween:
                                    if (block.childrenIndices.size() > 1) {
                                        child.x = prevX + block.elementsPadding;
                                        prevX = child.x + child.width + (block.width - block.innerPadding * 2.0f - blockContentWidth) / (block.childrenIndices.size() - 1.0f);                                        
                                    } else {
                                        child.x = (block.width - blockContentWidth) / 2.0f;
                                    }
                                    break;
                                case GUIBlock::ArrayAlign::SpaceAround:
                                    child.x = prevX + block.elementsPadding;
                                    prevX = child.x + child.width + (block.width - block.innerPadding * 2.0f - blockContentWidth) / (block.childrenIndices.size() + 1.0f);
                                    break;
                            }

                            switch (block.typeData.arrayData.alignY) {
                                case GUIBlock::ArrayAlign::Start: child.y = block.innerPadding; break;
                                case GUIBlock::ArrayAlign::End: child.y = block.height - child.height - block.innerPadding; break;
                                case GUIBlock::ArrayAlign::Centre:
                                case GUIBlock::ArrayAlign::SpaceAround:
                                case GUIBlock::ArrayAlign::SpaceBetween:
                                    child.y = (block.height - child.height) / 2.0f; break;
                            }
                        }
                    }
                }

                if (block.type == GUIBlock::BlockType::Undefined) {
                    block.width = 5.0f;
                    block.height = 5.0f;
                }

                if (block.parentIndex != UINT32_MAX) nextStack.insert(block.parentIndex);
            }
            stack.clear();
            stack = std::move(nextStack);
        }

        switch (mSystem.rootXType) {
            case GUIBlock::DType::Automatic:
                mSystem.blocks[mRootIndex].x = 0.0f - mSystem.blocks[mRootIndex].width / 2.0f;
                break;

            case GUIBlock::DType::Absolute:
                mSystem.blocks[mRootIndex].x = mSystem.rootSetX;
                break;

            case GUIBlock::DType::ScreenPercentage:
                mSystem.blocks[mRootIndex].x = (mWindowWidth * mSystem.rootSetX / 100.0f - mWindowWidth * 0.5f) / mDpi * 2.54f;
                break;
        }

        switch (mSystem.rootYType) {
            case GUIBlock::DType::Automatic:
                mSystem.blocks[mRootIndex].y = 0.0f - mSystem.blocks[mRootIndex].height / 2.0f;
                break;

            case GUIBlock::DType::Absolute:
                mSystem.blocks[mRootIndex].y = mSystem.rootSetY;
                break;

            case GUIBlock::DType::ScreenPercentage:
                mSystem.blocks[mRootIndex].y = (mWindowHeight * mSystem.rootSetY / 100.0f - mWindowHeight * 0.5f) / mDpi * 2.54f;
        }
    }

    // adjusting to absolute position
    {
        struct StackEntry {
            uint32_t index;
            float x;
            float y;
        };

        std::vector<StackEntry> stack;
        stack.push_back({mRootIndex, 0.0f, 0.0f});

        while (!stack.empty()) {

            auto [index, x, y] = stack.back();
            stack.pop_back();

            GUIBlock& block = mSystem.blocks[index];
            block.x += x;
            block.y += y;

            for (uint32_t child : block.childrenIndices) {
                stack.push_back({child, block.x, block.y});
            }

        }
    }

}



std::vector<DebugLineVertex> GUIProcessor::getDebugViewLines() const {

    std::vector<DebugLineVertex> vertices;

    float scale = mDpi / 2.54f * 2.0f / mWindowHeight;

    for (uint32_t i = 0; i < mSystem.blocks.size(); i++) {
        const GUIBlock& block = mSystem.blocks[i];
        uint32_t colour = glm::packUnorm4x8(glm::vec4((((i + 12) * 53) % 256) / 255.0f, (((i + 11) * 57) % 256) / 255.0f, (((i + 10) * 61) % 256) / 255.0f, 1.0f));

        vertices.push_back({glm::vec3(block.x * scale, block.y * scale, 1.0f), colour});
        vertices.push_back({glm::vec3((block.x + block.width) * scale, block.y * scale, 1.0f), colour});
        
        vertices.push_back({glm::vec3((block.x + block.width) * scale, block.y * scale, 1.0f), colour});
        vertices.push_back({glm::vec3((block.x + block.width) * scale, (block.y + block.height) * scale, 1.0f), colour});
        
        vertices.push_back({glm::vec3((block.x + block.width) * scale, (block.y + block.height) * scale, 1.0f), colour});
        vertices.push_back({glm::vec3(block.x * scale, (block.y + block.height) * scale, 1.0f), colour});
        
        vertices.push_back({glm::vec3(block.x * scale, (block.y + block.height) * scale, 1.0f), colour});
        vertices.push_back({glm::vec3(block.x * scale, block.y * scale, 1.0f), colour});
    }

    return vertices;
}



void GUIProcessor::destroy() noexcept {

}

