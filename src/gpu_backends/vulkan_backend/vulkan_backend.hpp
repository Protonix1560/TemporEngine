#pragma once

#include "core.hpp"
#include "window_manager.hpp"
#include "gpu_backend.hpp"

#include <array>
#include <vector>

#include <SDL2/SDL_messagebox.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>




#define TOF(call) do { \
    auto _r = (call); \
    if (_r) throw Exception(ErrCode::InternalError, __FILE__ + ":"s + std::to_string(__LINE__) + " "s + #call + ": returned "s + std::to_string(_r)); \
} while(0)




class Swapchain {

    public:
        void construct(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, const WindowManager* windowManager);
        void destroy() noexcept;

        inline const VkSwapchainKHR& swapchain() const noexcept { return mSwapchain; }
        inline const VkFormat& format() const noexcept { return mFormat; }
        inline const VkSemaphore& getSemaphore(uint32_t index) const { return mSemaphores[index]; }
        inline const VkImageView& getImageView(uint32_t index) const { return mImageViews[index]; }
        inline const VkImage& getImage(uint32_t index) const { return mImages[index]; }
        inline const VkExtent2D& extent() const { return mExtent; }
        inline const uint32_t& imageCount() const { return mImageCount; }

    private:
        VkDevice mDevice;
        VkSurfaceKHR mSurface;
        const WindowManager* mpWindowManager;

        bool mConstructed = false;

        VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
        std::vector<VkSemaphore> mSemaphores;
        std::vector<VkImage> mImages;
        std::vector<VkImageView> mImageViews;

        VkFormat mFormat;
        VkExtent2D mExtent;
        uint32_t mImageCount = 0;

};


struct Frame {

    public:
        void construct(VkDevice device, uint32_t queueFamilyIndex);
        void destroy() noexcept;

        inline VkCommandBuffer& renderCommandBuffer() noexcept { return mCommandBuffers[0]; }
        inline VkCommandBuffer& presentCommandBuffer() noexcept { return mCommandBuffers[1]; }

        VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
        VkFence inFlightFence = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;

    private:
        VkDevice mDevice;
        VkCommandBuffer mCommandBuffers[2] = {};
};


class FramebuffersHolder {

    public:
        void construct(Swapchain& swapchain, VkDevice device, VkRenderPass renderPass);
        void destroy() noexcept;

        inline const VkFramebuffer& getFramebuffer(uint32_t index) const { return mFramebuffers[index]; };

    private:
        std::vector<VkFramebuffer> mFramebuffers;
        VkDevice mDevice;

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


struct VertexBuffer {
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

};


class BackendVk : public Backend {

    public:
        void init(const WindowManager *windowManager, const GlobalServiceLocator* serviceLocator) override;
        void destroy() noexcept override;
        void update() override;
        void beginRenderPass(const Scissor* pScissor = nullptr, const Viewport* pViewport = nullptr) override;
        void renderDebugLines(const std::vector<DebugLineVertex>& debugLinesVertices) override;
        void endRenderPass() override;
    private:
        VkInstance mInstance = VK_NULL_HANDLE;
        uint32_t mApiVer;
        VkSurfaceKHR mSurface = VK_NULL_HANDLE;
        VkDevice mDevice = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT mDebugMessenger = VK_NULL_HANDLE;
        VkRenderPass mMainRenderPass = VK_NULL_HANDLE;
        VkPipelineLayout mDebugLinesPipelineLayout = VK_NULL_HANDLE;
        VkPipeline mDebugLinesPipeline = VK_NULL_HANDLE;

        VertexBuffer mDebugLinesBuffer;
        
        VkPhysicalDevice mPhysicalDevice;
        VkQueue mRenderQueue;

        const WindowManager* mpWindowManager;
        const GlobalServiceLocator* mpServiceLocator;

        Swapchain mSwapchain;
        FramebuffersHolder mFramebuffers;
        std::vector<Frame> mFrames;
        uint32_t mFrameCounter = 0;
        uint32_t mCurrentSwapchainImage;
        uint32_t mMaxFramesInFlight;

};

