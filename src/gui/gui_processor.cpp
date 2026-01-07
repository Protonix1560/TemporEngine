
#include "core.hpp"
#include "gui_processor.hpp"
#include "logger.hpp"

#include <cstdint>
#include <glm/packing.hpp>
#include <set>



void GUIProcessor::init(const GUISystem& system) {

    auto& log = gGetServiceLocator()->get<Logger>();

    mSystem = system;

    if (mSystem.blocks.size() == 0) return;

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
                throw Exception(ErrCode::FormatError, "Invalid GUI system block tree: detected multiple root blocks.\nThe GUI system block tree must be a valid graph tree or an empty graph");
            }
            mRootIndex = i;
            break;
        }
    }
    if (mRootIndex == UINT32_MAX) throw Exception(ErrCode::FormatError, "Invalid GUI system block tree: detected no root blocks.\nThe GUI system block tree must be a valid graph tree or an empty graph");

    // finding leaves of the system block graph tree
    // and determining if the graph is a tree or not
    {
        std::vector<bool> visited(mSystem.blocks.size(), false);
        std::vector<std::pair<uint32_t, uint32_t>> stack = {{mRootIndex, 0}};

        while (!stack.empty()) {
            auto [index, depth] = stack.back();
            stack.pop_back();

            if (visited[index]) throw Exception(ErrCode::FormatError, "Invalid GUI system block tree: detected a cycle.\nThe GUI system block tree must be a valid graph tree or an empty graph");
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

    if (mSystem.blocks.size() == 0) return;

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

                    float blockContentWidth = 0.0f;
                    float blockContentHeight = 0.0f;
                    for (uint32_t childIndex : block.childrenIndices) {
                        GUIBlock& child = mSystem.blocks[childIndex];
                        if (block.typeData.array.orientation == GUIBlock::Orientation::Horizontal) {
                            blockContentWidth += child.width;
                            if (child.height > blockContentHeight) blockContentHeight = child.height;
                        } else {
                            blockContentHeight += child.height;
                            if (child.width > blockContentWidth) blockContentWidth = child.width;
                        }
                    }

                    if (block.typeData.array.orientation == GUIBlock::Orientation::Horizontal) {
                        blockContentWidth += block.elementsPadding * static_cast<float>(block.childrenIndices.size() - 1);
                    } else {
                        blockContentHeight += block.elementsPadding * static_cast<float>(block.childrenIndices.size() - 1);
                    }

                    switch (block.widthType) {
                        case GUIBlock::DType::Automatic: block.width = blockContentWidth + block.innerPadding * 2.0f; break;
                        case GUIBlock::DType::Absolute: block.width = block.setWidth; break;
                        case GUIBlock::DType::ScreenPercentage: block.width = mWindowWidth * block.setWidth / 100.0f / mDpi * 2.54f; break;
                        case GUIBlock::DType::ScreenRelative: block.width = mWindowWidth / mDpi * 2.54f + block.setWidth; break;
                        case GUIBlock::DType::Expand: block.width = blockContentWidth + block.innerPadding * 2.0f; break;
                    }
                    switch (block.heightType) {
                        case GUIBlock::DType::Automatic: block.height = blockContentHeight + block.innerPadding * 2.0f; break;
                        case GUIBlock::DType::Absolute: block.height = block.setHeight; break;
                        case GUIBlock::DType::ScreenPercentage: block.height = mWindowHeight * block.setHeight / 100.0f / mDpi * 2.54f; break;
                        case GUIBlock::DType::ScreenRelative: block.height = mWindowHeight / mDpi * 2.54f + block.setHeight; break;
                        case GUIBlock::DType::Expand: block.height = blockContentHeight + block.innerPadding * 2.0f; break;
                    }

                    float prevX;
                    float prevY;
                    switch (block.typeData.array.alignX) {
                        case GUIBlock::ArrayAlign::Start: prevX = block.innerPadding - block.elementsPadding; break;
                        case GUIBlock::ArrayAlign::End: prevX = block.width - blockContentWidth - block.innerPadding - block.elementsPadding; break;
                        case GUIBlock::ArrayAlign::Centre: prevX = (block.width - blockContentWidth) / 2.0f - block.elementsPadding; break;
                        case GUIBlock::ArrayAlign::SpaceBetween: prevX = block.innerPadding - block.elementsPadding; break;
                        case GUIBlock::ArrayAlign::SpaceAround: prevX = block.innerPadding - block.elementsPadding + (block.width - block.innerPadding * 2.0f - blockContentWidth) / (block.childrenIndices.size() + 1.0f); break;
                    }
                    switch (block.typeData.array.alignY) {
                        case GUIBlock::ArrayAlign::Start: prevY = block.innerPadding - block.elementsPadding; break;
                        case GUIBlock::ArrayAlign::End: prevY = block.height - blockContentHeight - block.innerPadding - block.elementsPadding; break;
                        case GUIBlock::ArrayAlign::Centre: prevY = (block.height - blockContentHeight) / 2.0f - block.elementsPadding; break;
                        case GUIBlock::ArrayAlign::SpaceBetween: prevY = block.innerPadding - block.elementsPadding; break;
                        case GUIBlock::ArrayAlign::SpaceAround: prevY = block.innerPadding - block.elementsPadding + (block.height - block.innerPadding * 2.0f - blockContentHeight) / (block.childrenIndices.size() + 1.0f); break;
                    }

                    for (uint32_t childIndex : block.childrenIndices) {
                        GUIBlock& child = mSystem.blocks[childIndex];

                        switch (block.typeData.array.alignX) {
                            case GUIBlock::ArrayAlign::Start:
                                child.x = prevX + block.elementsPadding;
                                if (block.typeData.array.orientation == GUIBlock::Orientation::Horizontal) {
                                    prevX = child.x + child.width;
                                }
                                break;

                            case GUIBlock::ArrayAlign::Centre:
                                child.x = prevX + block.elementsPadding;
                                if (block.typeData.array.orientation == GUIBlock::Orientation::Horizontal) {
                                    prevX = child.x + child.width;
                                } else {
                                    child.y += (blockContentWidth - child.width) * 0.5f;
                                }
                                break;

                            case GUIBlock::ArrayAlign::End:
                                child.x = prevX + block.elementsPadding;
                                if (block.typeData.array.orientation == GUIBlock::Orientation::Horizontal) {
                                    prevX = child.x + child.width;
                                } else {
                                    child.y += blockContentWidth - child.width;
                                }
                                break;

                            case GUIBlock::ArrayAlign::SpaceBetween:
                                if (block.childrenIndices.size() > 1) {
                                    if (block.typeData.array.orientation == GUIBlock::Orientation::Horizontal) {
                                        child.x = prevX + block.elementsPadding;
                                        prevX = child.x + child.width + (block.width - block.innerPadding * 2.0f - blockContentWidth) / (block.childrenIndices.size() - 1.0f);                                        
                                    } else {
                                        child.x = (block.width - child.width) / 2.0f;
                                    }
                                } else {
                                    child.x = (block.width - blockContentWidth) / 2.0f;
                                }
                                break;
                                
                            case GUIBlock::ArrayAlign::SpaceAround:
                                if (block.typeData.array.orientation == GUIBlock::Orientation::Horizontal) {
                                    child.x = prevX + block.elementsPadding;
                                    prevX = child.x + child.width + (block.width - block.innerPadding * 2.0f - blockContentWidth) / (block.childrenIndices.size() + 1.0f);
                                } else {
                                    child.x = (block.width - child.width) / 2.0f;
                                }
                                break;
                        }

                        switch (block.typeData.array.alignY) {
                            case GUIBlock::ArrayAlign::Start:
                                child.y = prevY + block.elementsPadding;
                                if (block.typeData.array.orientation == GUIBlock::Orientation::Vertical) {
                                    prevY = child.y + child.height;
                                }
                                break;

                            case GUIBlock::ArrayAlign::Centre:
                                child.y = prevY + block.elementsPadding;
                                if (block.typeData.array.orientation == GUIBlock::Orientation::Vertical) {
                                    prevY = child.y + child.height;
                                } else {
                                    child.y += (blockContentHeight - child.height) * 0.5f;
                                }
                                break;

                            case GUIBlock::ArrayAlign::End:
                                child.y = prevY + block.elementsPadding;
                                if (block.typeData.array.orientation == GUIBlock::Orientation::Vertical) {
                                    prevY = child.y + child.height;
                                } else {
                                    child.y += blockContentHeight - child.height;
                                }
                                break;

                            case GUIBlock::ArrayAlign::SpaceBetween:
                                if (block.childrenIndices.size() > 1) {
                                    if (block.typeData.array.orientation == GUIBlock::Orientation::Vertical) {
                                    child.y = prevY + block.elementsPadding;
                                        prevY = child.y + child.height + (block.height - block.innerPadding * 2.0f - blockContentHeight) / (block.childrenIndices.size() - 1.0f);                                        
                                    } else {
                                        child.y = (block.height - child.height) / 2.0f;
                                    }
                                } else {
                                    child.y = (block.height - blockContentHeight) / 2.0f;
                                }
                                break;
                                
                            case GUIBlock::ArrayAlign::SpaceAround:
                                if (block.typeData.array.orientation == GUIBlock::Orientation::Vertical) {
                                    child.y = prevY + block.elementsPadding;
                                    prevY = child.y + child.height + (block.height - block.innerPadding * 2.0f - blockContentHeight) / (block.childrenIndices.size() + 1.0f);
                                } else {
                                    child.y = (block.height - child.height) / 2.0f;
                                }
                                break;
                        }
                    }
                }

                if (block.type == GUIBlock::BlockType::Plain) {
                    block.width = 5.0f;
                    block.height = 5.0f;
                }

                if (block.parentIndex != UINT32_MAX) nextStack.insert(block.parentIndex);
            }
            stack.clear();
            stack = std::move(nextStack);
        }

        switch (mSystem.rootXType) {
            case GUIBlock::DType::Automatic: mSystem.blocks[mRootIndex].x = 0.0f - mSystem.blocks[mRootIndex].width / 2.0f; break;
            case GUIBlock::DType::Absolute: mSystem.blocks[mRootIndex].x = mSystem.rootSetX; break;
            case GUIBlock::DType::ScreenPercentage: mSystem.blocks[mRootIndex].x = (mWindowWidth * mSystem.rootSetX / 100.0f - mWindowWidth * 0.5f) / mDpi * 2.54f; break;
            case GUIBlock::DType::ScreenRelative: mSystem.blocks[mRootIndex].x = mWindowWidth / mDpi * 2.54f + mSystem.rootSetX; break;
            case GUIBlock::DType::Expand: break;
        }

        switch (mSystem.rootYType) {
            case GUIBlock::DType::Automatic: mSystem.blocks[mRootIndex].y = 0.0f - mSystem.blocks[mRootIndex].height / 2.0f; break;
            case GUIBlock::DType::Absolute: mSystem.blocks[mRootIndex].y = mSystem.rootSetY; break;
            case GUIBlock::DType::ScreenPercentage: mSystem.blocks[mRootIndex].y = (mWindowHeight * mSystem.rootSetY / 100.0f - mWindowHeight * 0.5f) / mDpi * 2.54f; break;
            case GUIBlock::DType::ScreenRelative: mSystem.blocks[mRootIndex].y = mWindowHeight / mDpi * 2.54f + mSystem.rootSetY; break;
            case GUIBlock::DType::Expand: break;
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

    // expanding blocks
    {
        std::vector<uint32_t> stack;
        stack.insert(stack.end(), mSystem.blocks[mRootIndex].childrenIndices.begin(), mSystem.blocks[mRootIndex].childrenIndices.end());

        while (!stack.empty()) {
            uint32_t index = stack.back();
            stack.pop_back();
            GUIBlock& block = mSystem.blocks[index];
            GUIBlock& parent = mSystem.blocks[block.parentIndex];

            if (block.widthType == GUIBlock::DType::Expand) {
                if (
                    parent.type == GUIBlock::BlockType::Array && (
                        parent.typeData.array.orientation == GUIBlock::Orientation::Vertical ||
                        parent.childrenIndices.size() == 1
                    )
                ) {
                    block.x = parent.x + parent.innerPadding;
                    block.width = parent.width - parent.innerPadding * 2.0f;
                }
            }

            if (block.heightType == GUIBlock::DType::Expand) {
                if (
                    parent.type == GUIBlock::BlockType::Array && (
                        parent.typeData.array.orientation == GUIBlock::Orientation::Horizontal ||
                        parent.childrenIndices.size() == 1
                    )
                ) {
                    block.y = parent.y + parent.innerPadding;
                    block.height = parent.height - parent.innerPadding * 2.0f;
                }
            }

            for (uint32_t child : block.childrenIndices) {
                stack.push_back(child);
            }
        }
    }

}



std::vector<DebugLineVertex> GUIProcessor::getDebugViewLines() const {

    std::vector<DebugLineVertex> vertices;

    if (mSystem.blocks.size() == 0) return vertices;

    float scale = mDpi / 2.54f * 2.0f / mWindowHeight;

    for (uint32_t i = 0; i < mSystem.blocks.size(); i++) {
        const GUIBlock& block = mSystem.blocks[i];
        uint32_t colour = glm::packUnorm4x8(glm::vec4((((i + 12) * 53) % 256) / 255.0f, (((i + 11) * 57) % 256) / 255.0f, (((i + 10) * 61) % 256) / 255.0f, 1.0f));
        float z = 10.0f;

        vertices.push_back({glm::vec3(block.x * scale, block.y * scale, z), colour});
        vertices.push_back({glm::vec3((block.x + block.width) * scale, block.y * scale, z), colour});
        
        vertices.push_back({glm::vec3((block.x + block.width) * scale, block.y * scale, z), colour});
        vertices.push_back({glm::vec3((block.x + block.width) * scale, (block.y + block.height) * scale, z), colour});
        
        vertices.push_back({glm::vec3((block.x + block.width) * scale, (block.y + block.height) * scale, z), colour});
        vertices.push_back({glm::vec3(block.x * scale, (block.y + block.height) * scale, z), colour});
        
        vertices.push_back({glm::vec3(block.x * scale, (block.y + block.height) * scale, z), colour});
        vertices.push_back({glm::vec3(block.x * scale, block.y * scale, z), colour});
    }

    return vertices;
}



GUIDrawDesc GUIProcessor::getDrawDesc() const {

    GUIDrawDesc desc;

    if (mSystem.blocks.size() == 0) return desc;

    desc.rects.reserve(mSystem.blocks.size());

    float scale = mDpi / 2.54f * 2.0f / mWindowHeight;

    for (const auto& block : mSystem.blocks) {
        desc.rects.push_back({
            .x = block.x * scale,
            .y = block.y * scale,
            .w = block.width * scale,
            .h = block.height * scale,
            .bgColour = block.bgColour
        });
    }

    return desc;
}



void GUIProcessor::shutdown() noexcept {

}

