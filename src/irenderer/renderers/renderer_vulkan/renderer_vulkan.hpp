

#ifndef IRENDERER_RENDERERS_RENDERER_VULKAN_RENDERER_VULKAN_HPP_
#define IRENDERER_RENDERERS_RENDERER_VULKAN_RENDERER_VULKAN_HPP_



#include "core.hpp"
#include "plugin_core.h"
#include "irenderer.hpp"

#include <SDL2/SDL_vulkan.h>
#include <array>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>




#define TOF(call) do { \
    auto _r = (call); \
    if (_r) throw Exception(ErrCode::InternalError, __FILE__ + ":"s + std::to_string(__LINE__) + " "s + #call + ": returned "s + std::to_string(_r)); \
} while(0)




struct Swapchain {

    public:
        void construct(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, TprWindow window);
        void destroy() noexcept;

        const VkSwapchainKHR& swapchain() const noexcept { return mSwapchain; }
        const VkSemaphore& getSemaphore(uint32_t index) const { return mSemaphores[index]; }
        const VkExtent2D& extent() const { return mExtent; }
        const uint32_t& imageCount() const { return mImageCount; }

        const VkImageView& getChainImageView(uint32_t index) const { return mChainImageViews[index]; }
        const VkImage& getChainImage(uint32_t index) const { return mChainImages[index]; }
        const VkFormat& chainFormat() const noexcept { return mChainImageFormat; }

        const VkImageView& getDepthImageView(uint32_t index) const { return mDepthImageViews[index]; }
        const VkImage& getDepthImage(uint32_t index) const { return mDepthImages[index]; }
        const VkFormat& depthFormat() const noexcept { return mDepthImageFormat; }

    private:
        VkDevice mDevice;
        VkSurfaceKHR mSurface;
        TprWindow mWindow;

        bool mConstructed = false;

        VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
        std::vector<VkSemaphore> mSemaphores;

        std::vector<VkImage> mChainImages;
        std::vector<VkImageView> mChainImageViews;
        VkFormat mChainImageFormat;

        std::vector<VkImage> mDepthImages;
        std::vector<VkImageView> mDepthImageViews;
        std::vector<VkDeviceMemory> mDepthImageMemories;
        VkFormat mDepthImageFormat;

        VkExtent2D mExtent;
        uint32_t mImageCount = 0;

};


struct Frame {

    public:
        void construct(VkDevice device, uint32_t queueFamilyIndex);
        void destroy() noexcept;

        inline VkCommandBuffer& renderCommandBuffer() noexcept { return mCommandBuffers[0]; }
        inline VkCommandBuffer& presentCommandBuffer() noexcept { return mCommandBuffers[1]; }

        VkCommandPool commandPool = VK_NULL_HANDLE;
        VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
        VkFence inFlightFence = VK_NULL_HANDLE;

    private:
        VkDevice mDevice;
        VkCommandBuffer mCommandBuffers[2] = {};
};


struct DebugLineVertexVk : public DebugLineVertex {

    static VkVertexInputBindingDescription getBindDesc() {
        VkVertexInputBindingDescription bindingDesc{};
        bindingDesc.binding = 0;
        bindingDesc.stride = sizeof(DebugLineVertexVk);
        bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDesc;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttrDesc() {
        std::array<VkVertexInputAttributeDescription, 2> attribs{};

        attribs[0].binding = 0;
        attribs[0].location = 0;
        attribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attribs[0].offset = offsetof(DebugLineVertex, pos);

        attribs[1].binding = 0;
        attribs[1].location = 1;
        attribs[1].format = VK_FORMAT_R8G8B8A8_UNORM;
        attribs[1].offset = offsetof(DebugLineVertex, colour);

        return attribs;
    }
};


struct DebugLinesPushConst {
    glm::mat4 mvp;
};



struct GUIRectVertex {

    glm::vec3 pos;
    uint32_t colour;

    static VkVertexInputBindingDescription getBindDesc() {
        VkVertexInputBindingDescription bindingDesc{};
        bindingDesc.binding = 0;
        bindingDesc.stride = sizeof(GUIRectVertex);
        bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDesc;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttrDesc() {
        std::array<VkVertexInputAttributeDescription, 2> attribs{};

        attribs[0].binding = 0;
        attribs[0].location = 0;
        attribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attribs[0].offset = offsetof(GUIRectVertex, pos);

        attribs[1].binding = 0;
        attribs[1].location = 1;
        attribs[1].format = VK_FORMAT_R8G8B8A8_UNORM;
        attribs[1].offset = offsetof(GUIRectVertex, colour);

        return attribs;
    }
};


struct GUIPushConst {
    glm::mat4 mvp;
};



inline uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t memType, VkMemoryPropertyFlags property) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memType & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & property) == property) {
            return i;
        }
    }

    throw Exception(ErrCode::InternalError, "Failed to find sufficient physical device memory type");
}


struct Buffer {
    public:
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* map = nullptr;

        void allocate(
            VkDevice device, VkPhysicalDevice physicalDevice, uint32_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags property,
            VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE, const uint32_t* pQueueFamilyIndices = nullptr, uint32_t queueFamilyIndexCount = 0
        ) {
            mDevice = device;
            mPhysicalDevice = physicalDevice;
            mSize = size;
            mUsage = usage;
            mProperty = property;
            mSharingMode = sharingMode;
            mQueueFamilyIndices.clear();
            mQueueFamilyIndices.insert(mQueueFamilyIndices.end(), pQueueFamilyIndices, pQueueFamilyIndices + queueFamilyIndexCount);
            mQueueFamilyIndices.shrink_to_fit();
            allocateInternal();
        }

        void mapMemory(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE, VkMemoryMapFlags flags = 0) {
            mMapOffset = offset;
            mMapSize = size;
            mMapFlags = flags;
            TOF(vkMapMemory(mDevice, memory, offset, size, flags, &map));
        }

        void unmapMemory() {
            if (map) {
                vkUnmapMemory(mDevice, memory);
                map = nullptr;
            }
        }

        void free() {
            unmapMemory();
            if (memory) vkFreeMemory(mDevice, memory, nullptr);
            if (mDevice) vkDestroyBuffer(mDevice, buffer, nullptr);
        }

        uint32_t size() const { return mSize; }

        int resize(uint32_t newSize) {
            if (newSize == mSize) return 0;
            mSize = newSize;
            bool wasMapped = (map != nullptr);
            free();
            allocateInternal();
            if (wasMapped) mapMemory(mMapOffset, mMapSize, mMapFlags);
            return 1;
        }

    private:
        VkDevice mDevice;
        VkPhysicalDevice mPhysicalDevice;
        uint32_t mSize;
        VkBufferUsageFlags mUsage;
        VkMemoryPropertyFlags mProperty;
        VkSharingMode mSharingMode;
        std::vector<uint32_t> mQueueFamilyIndices;
        VkDeviceSize mMapOffset;
        VkDeviceSize mMapSize;
        VkMemoryMapFlags mMapFlags;

        void allocateInternal() {
            VkBufferCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            createInfo.size = mSize;
            createInfo.usage = mUsage;
            createInfo.sharingMode = mSharingMode;
            createInfo.pQueueFamilyIndices = mQueueFamilyIndices.data();
            createInfo.queueFamilyIndexCount = mQueueFamilyIndices.size();
            
            TOF(vkCreateBuffer(mDevice, &createInfo, nullptr, &buffer));

            VkMemoryRequirements memReq{};
            vkGetBufferMemoryRequirements(mDevice, buffer, &memReq);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = memReq.size;
            allocInfo.memoryTypeIndex = findMemoryType(mPhysicalDevice, memReq.memoryTypeBits, mProperty);

            TOF(vkAllocateMemory(mDevice, &allocInfo, nullptr, &memory));
            TOF(vkBindBufferMemory(mDevice, buffer, memory, 0));
        }

};


class FramebuffersHolder {
    public:
        void construct(Swapchain& swapchain, VkDevice device, VkRenderPass renderPass);
        void destroy() noexcept;
        const VkFramebuffer& getFramebuffer(uint32_t index) const { return mFramebuffers[index]; };
    private:
        std::vector<VkFramebuffer> mFramebuffers;
        VkDevice mDevice;
};


struct RenderPass {
    VkRenderPass mRenderPass = VK_NULL_HANDLE;
    VkPipelineLayout mDebugLinesPipelineLayout = VK_NULL_HANDLE;
    VkPipeline mDebugLinesPipeline = VK_NULL_HANDLE;
    VkPipelineLayout mGUIPipelineLayout = VK_NULL_HANDLE;
    VkPipeline mGUIPipeline = VK_NULL_HANDLE;
    VkDevice mDevice = VK_NULL_HANDLE;
    void construct(VkDevice device, Swapchain& swapchain);
    void destroy() noexcept;
};


struct WindowContext {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    Swapchain swapchain;
    FramebuffersHolder framebuffers;
    std::shared_ptr<RenderPass> renderPass;
    std::vector<Frame> frames;
    TprWindow handle;
};


class RendererVulkan : public IRenderer {

    public:
        void init() override;
        void shutdown() noexcept override;
        void update() override;
        
        GraphicsBackend getGraphicsBackend() const override { return GraphicsBackend::Vulkan; }
        uint32_t getFrameWidth(TprWindow handle) const override { return mWindowContexts.at(getBasicHandleIndex(handle)).swapchain.extent().width; }
        uint32_t getFrameHeight(TprWindow handle) const override { return mWindowContexts.at(getBasicHandleIndex(handle)).swapchain.extent().height; }
        
        TprResult registerWindow(TprWindow handle) noexcept override;

        void unregisterWindow(TprWindow handle) noexcept override;

        void render(const RenderGraph& renderGraph) override;

        [[deprecated]] void renderDebugLines(const std::vector<DebugLineVertex>& debugLinesVertices, CameraProject cameraProject = CameraProject::Ortho);
        [[deprecated]] void renderGUI(const GUIDrawDesc& desc);

    private:
        VkInstance mInstance = VK_NULL_HANDLE;
        uint32_t mApiVer;
        VkDevice mDevice = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT mDebugMessenger = VK_NULL_HANDLE;

        std::vector<const char*> mInstanceExtensions;

        Buffer mDebugLinesBuffer;
        Buffer mGUIVertexBuffer;
        Buffer mGUIIndexBuffer;
        
        VkPhysicalDevice mPhysicalDevice;
        VkQueue mRenderQueue;

        std::unordered_map<uint32_t, WindowContext> mWindowContexts;
        uint32_t mFrameCounter = 0;
        uint32_t mMaxFramesInFlight;

};




#endif  // IRENDERER_RENDERERS_RENDERER_VULKAN_RENDERER_VULKAN_HPP_

