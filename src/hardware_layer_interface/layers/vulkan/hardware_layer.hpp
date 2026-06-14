

#ifndef HARDWARE_LAYER_INTERFACE_LAYERS_VULKAN_HARDWARE_LAYER_HPP_
#define HARDWARE_LAYER_INTERFACE_LAYERS_VULKAN_HARDWARE_LAYER_HPP_


#include "core.hpp"
#include "plugin_core.h"
#include "hardware_layer_interface.hpp"
#include "logger.hpp"
#include "resource_registry.hpp"
#include "window_manager.hpp"
#include "interval_union.hpp"

#include <SDL2/SDL_vulkan.h>

#include <array>
#include <glm/fwd.hpp>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>



struct Vertex {

    struct Push {
        glm::mat4 mvp;
    };

    static VkVertexInputBindingDescription bindDesc() {
        VkVertexInputBindingDescription bindingDesc{};
        bindingDesc.binding = 0;
        bindingDesc.stride = sizeof(Vertex);
        bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDesc;
    }

    static std::array<VkVertexInputAttributeDescription, 2> attributeDesc() {
        std::array<VkVertexInputAttributeDescription, 2> attribs{};

        attribs[0].binding = 0;
        attribs[0].location = 0;
        attribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attribs[0].offset = offsetof(Vertex, pos);

        attribs[1].binding = 0;
        attribs[1].location = 1;
        attribs[1].format = VK_FORMAT_R8G8B8A8_UNORM;
        attribs[1].offset = offsetof(Vertex, colour);

        return attribs;
    }

    glm::vec3 pos;
    uint32_t colour;
};


struct BasicVertexPushConst {
    glm::mat4 mvp;
};



struct VertexPosition {
    static constexpr VkFormat format = VK_FORMAT_R32G32B32_SFLOAT;
    glm::vec3 pos;
};



struct Frame {

    VkCommandBuffer& renderCommandBuffer() noexcept { return commandBuffers[0]; }
    VkCommandBuffer& presentCommandBuffer() noexcept { return commandBuffers[1]; }
    const VkCommandBuffer& renderCommandBuffer() const noexcept { return commandBuffers[0]; }
    const VkCommandBuffer& presentCommandBuffer() const noexcept { return commandBuffers[1]; }

    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    VkFence inFlightFence = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffers[2] = {};
};


struct RenderPass {
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipeline basicPipeline = VK_NULL_HANDLE;
};


#define SYM_FIELD(func) PFN_##func func

struct VulkanSymbols {
    SYM_FIELD(vkEnumerateInstanceVersion);
    SYM_FIELD(vkEnumerateInstanceLayerProperties);
    SYM_FIELD(vkEnumerateInstanceExtensionProperties);
    SYM_FIELD(vkCreateInstance);
    SYM_FIELD(vkCreateDebugUtilsMessengerEXT);
    SYM_FIELD(vkEnumeratePhysicalDevices);
    SYM_FIELD(vkGetPhysicalDeviceProperties);
    SYM_FIELD(vkCreateDevice);
    SYM_FIELD(vkGetDeviceQueue);
    SYM_FIELD(vkGetPhysicalDeviceMemoryProperties);
    SYM_FIELD(vkCreateBuffer);
    SYM_FIELD(vkGetBufferMemoryRequirements);
    SYM_FIELD(vkAllocateMemory);
    SYM_FIELD(vkBindBufferMemory);
    SYM_FIELD(vkMapMemory);
    SYM_FIELD(vkUnmapMemory);
    SYM_FIELD(vkFreeMemory);
    SYM_FIELD(vkDestroyBuffer);
    SYM_FIELD(vkCreateSemaphore);
    SYM_FIELD(vkCreateFence);
    SYM_FIELD(vkCreateCommandPool);
    SYM_FIELD(vkAllocateCommandBuffers);
    SYM_FIELD(vkDestroyCommandPool);
    SYM_FIELD(vkDestroyFence);
    SYM_FIELD(vkDestroySemaphore);
    SYM_FIELD(vkDestroySurfaceKHR);
    SYM_FIELD(vkGetPhysicalDeviceSurfaceFormatsKHR);
    SYM_FIELD(vkGetPhysicalDeviceSurfacePresentModesKHR);
    SYM_FIELD(vkDestroySwapchainKHR);
    SYM_FIELD(vkCreateSwapchainKHR);
    SYM_FIELD(vkGetSwapchainImagesKHR);
    SYM_FIELD(vkCreateImageView);
    SYM_FIELD(vkCreateImage);
    SYM_FIELD(vkGetImageMemoryRequirements);
    SYM_FIELD(vkBindImageMemory);
    SYM_FIELD(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    SYM_FIELD(vkDestroyImageView);
    SYM_FIELD(vkDestroyImage);
    SYM_FIELD(vkCreateFramebuffer);
    SYM_FIELD(vkDestroyFramebuffer);
    SYM_FIELD(vkCreateRenderPass);
    SYM_FIELD(vkCreatePipelineLayout);
    SYM_FIELD(vkCreateShaderModule);
    SYM_FIELD(vkCreateGraphicsPipelines);
    SYM_FIELD(vkDestroyDebugUtilsMessengerEXT);
    SYM_FIELD(vkResetCommandBuffer);
    SYM_FIELD(vkBeginCommandBuffer);
    SYM_FIELD(vkEndCommandBuffer);
    SYM_FIELD(vkWaitForFences);
    SYM_FIELD(vkResetCommandPool);
    SYM_FIELD(vkAcquireNextImageKHR);
    SYM_FIELD(vkCmdBeginRenderPass);
    SYM_FIELD(vkCmdSetScissor);
    SYM_FIELD(vkCmdSetViewport);
    SYM_FIELD(vkCmdEndRenderPass);
    SYM_FIELD(vkResetFences);
    SYM_FIELD(vkQueueSubmit);
    SYM_FIELD(vkQueuePresentKHR);
    SYM_FIELD(vkDeviceWaitIdle);
    SYM_FIELD(vkCmdCopyBuffer);
    SYM_FIELD(vkDestroyDevice);
    SYM_FIELD(vkDestroyInstance);
    SYM_FIELD(vkCreateDescriptorSetLayout);
    SYM_FIELD(vkDestroyPipelineLayout);
    SYM_FIELD(vkDestroyPipeline);
    SYM_FIELD(vkDestroyRenderPass);
    SYM_FIELD(vkDestroyDescriptorSetLayout);
    SYM_FIELD(vkDestroyShaderModule);
    SYM_FIELD(vkCreateDescriptorPool);
    SYM_FIELD(vkDestroyDescriptorPool);
    SYM_FIELD(vkAllocateDescriptorSets);
    SYM_FIELD(vkUpdateDescriptorSets);
    SYM_FIELD(vkCmdBindDescriptorSets);
    SYM_FIELD(vkCmdBindPipeline);
    SYM_FIELD(vkCmdDrawIndexed);
    SYM_FIELD(vkCmdBindVertexBuffers);
    SYM_FIELD(vkCmdBindIndexBuffer);
};


struct Allocation {
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize offset = 0;
};

struct Allocator {
    private:
        Logger& mrLogger;
        VulkanSymbols& mrSym;
        std::vector<VkMemoryType> mMemoryTypes;
        VkPhysicalDevice mPhysicalDevice;
        VkDevice mDevice;
        uint32_t mAllocCount = 0;
        uint32_t mMaxAllocCount;
        
        struct Memory {
            void* map = nullptr;
        };
        std::unordered_map<VkDeviceMemory, Memory> mMemories;

    public:
        Allocator(Logger& rLogger, VulkanSymbols& rSym, VkPhysicalDevice physicalDevice, VkDevice device);

        expected<uint32_t, TprResult> findMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags property);

        expected<Allocation, TprResult> allocate(VkMemoryRequirements requirements, VkMemoryPropertyFlags property);
        void free(Allocation allocation) noexcept;

        expected<void*, TprResult> map(Allocation alloc);
        bool mapped(Allocation alloc) const noexcept;
        void unmap(Allocation alloc) noexcept;
};


struct BufferResources {
    Logger& mrLogger;
    VulkanSymbols& mrSym;
    Allocator& mrAlloc;
    VkPhysicalDevice mPhysicalDevice;
    VkDevice mDevice;

    VkBuffer mBuffer = VK_NULL_HANDLE;
    Allocation mAllocation;
    VkDeviceSize mSize;

    void* mMap = nullptr;
    VkDeviceSize mMapOffset;
    VkDeviceSize mMapSize;
    VkMemoryMapFlags mMapFlags;

    VkBufferUsageFlags mUsage;
    VkMemoryPropertyFlags mProperty;
    VkSharingMode mSharing;
    std::vector<uint32_t> mQueueFamilyIndices;
};

struct Buffer : private BufferResources {
    private:
        bool freeResources = true;

    public:
        void* mapping() const { return mMap; }
        bool mapped() const { return mMap != nullptr; }
        uint32_t size() const { return mSize; }

        Buffer(
            Logger& rLogger, VulkanSymbols& rSym, Allocator& rAlloc, VkPhysicalDevice physicalDevice, VkDevice device,
            VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags property,
            VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE, std::span<uint32_t> queueFamilyIndices = {}
        );

        Buffer(const Buffer& other) = delete;

        Buffer(Buffer&& other) noexcept : BufferResources(std::move(other)) {
            other.freeResources = false;
        }

        TprResult mapMemory();
        void unmapMemory() noexcept;

        TprResult reallocate(VkDeviceSize newSize);

        VkBuffer handle() const;

        ~Buffer() noexcept;
};


struct WindowContextResources {
    Logger& mrLogger;
    Allocator& mrAlloc;
    WindowManager& mrWinMan;
    ResourceRegistry& mrResReg;
    VulkanSymbols& mrSym;

    VkInstance mInstance;
    VkPhysicalDevice mPhysicalDevice;
    VkDevice mDevice;
    VkPipelineLayout mBasicPipelineLayout;
    VkDescriptorSetLayout mObjectSetLayout;

    TprWindow mWindowHandle;
    VkSurfaceKHR mSurface = VK_NULL_HANDLE;
    VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;

    std::vector<VkSemaphore> mSemaphores;

    std::vector<VkImage> mChainImages;
    std::vector<VkImageView> mChainImageViews;
    VkFormat mChainImageFormat;

    std::vector<VkImage> mDepthImages;
    std::vector<VkImageView> mDepthImageViews;
    std::vector<Allocation> mDepthImageMemories;
    VkFormat mDepthImageFormat;

    VkExtent2D mExtent;
    uint32_t mImageCount = 0;

    std::vector<VkFramebuffer> mFramebuffers;
    std::vector<Frame> mFrames;
    uint32_t mMaxFramesInFlight;
    uint32_t mPoolQueueFamily;
    RenderPass mRenderPass;

    std::vector<VkDescriptorSet> mObjectSets;
    VkDescriptorPool mDescPool;
};

struct WindowContext : private WindowContextResources {
    private:
        bool mFreeResources = true;

        TprResult constructPersistent();
        TprResult constructInvalidatable();

    public:
        WindowContext(
            Logger& rLogger, Allocator& rAlloc, WindowManager& rWinMan, ResourceRegistry& rResReg,
            VulkanSymbols& rSym, VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device,
            uint32_t queueFamilyIndex, uint32_t maxFramesInFlight, TprWindow window, VkPipelineLayout layout,
            VkDescriptorSetLayout objectSetLayout
        );

        WindowContext(const WindowContext& other) = delete;

        WindowContext(WindowContext&& other) noexcept : WindowContextResources(std::move(other)) {
            other.mFreeResources = false;
        }

        TprResult recreate();

        ~WindowContext();

        VkExtent2D extent() const { return mExtent; }
        VkSwapchainKHR swapchain() const { return mSwapchain; }
        const RenderPass& renderPass() const { return mRenderPass; }
        TprWindow handle() const { return mWindowHandle; }

        std::span<const Frame> frames() const { return std::span(mFrames.begin(), mFrames.end()); }
        std::span<const VkFramebuffer> framebuffers() const { return std::span(mFramebuffers.begin(), mFramebuffers.end()); }
        std::span<const VkImage> chainImages() const { return std::span(mChainImages.begin(), mChainImages.end()); }
        std::span<const VkSemaphore> semaphores() const { return std::span(mSemaphores.begin(), mSemaphores.end()); }
        std::span<const VkDescriptorSet> objectSets() const { return std::span(mObjectSets.begin(), mObjectSets.end()); }
};


struct GeometryIndexBuffer {
    Buffer indices;
    interval_union<uint32_t> free;
};

struct GeometryVertexBuffer {
    Buffer positions;
    interval_union<uint32_t> free;
};


struct DepthDomain {
};

struct RenderTarget {
    TprViewport viewport;
    TprScissor scissor;
    TprWindow window;
    uint32_t domain;
    std::vector<uint32_t> objectImages;
};

struct ObjectImage {
    std::vector<uint32_t> renderTargets;
    std::vector<uint32_t> objectDataIndices;
    uint32_t instanceIndicesOffset;
    TprMesh mesh;
};

struct Mesh {
    uint32_t indexBuffer;
    uint32_t vertexBuffer;
    interval<uint32_t> indicesInterval;
    interval<uint32_t> verticesInterval;
    std::vector<uint32_t> images;
};


struct ObjectData {
    glm::mat4 matrix;
};

struct ChunkEntry {
    uint32_t cachedVersion;
    uint32_t offset;
    uint32_t count = 0;
};


class HardwareLayerVulkan : public HardwareLayer {
    public:
        HardwareLayerVulkan(
            Logger& rLogger, ResourceRegistry& rResReg, WindowManager& rWinMan, Settings& rSettings, SceneGraph& rScGr, TprComponent componentRenderable,
            uint8_t engineVersionVariant, uint8_t engineVersionMajor, uint8_t engineVersionMinor, uint8_t engineVersionPatch
        );
        ~HardwareLayerVulkan() noexcept;
        TprResult update() override;
        
        uint32_t getFrameWidth(TprWindow handle) const override { return mWindowContexts.at(get_basic_handle_index(handle)).extent().width; }
        uint32_t getFrameHeight(TprWindow handle) const override { return mWindowContexts.at(get_basic_handle_index(handle)).extent().height; }
        TprResult registerWindow(TprWindow handle) noexcept override;
        void unregisterWindow(TprWindow handle) noexcept override;
        TprResult render() override;
        TprResult loadMesh(const AssetMesh& mesh) noexcept override;
        void unloadMesh(TprMesh mesh) noexcept override;
        expected<TprDepthDomain, TprResult> createDepthDomain(const TprDepthDomainCreateInfo* pInfo) noexcept override;
        void destroyDepthDomain(TprDepthDomain domain) noexcept override;
        expected<TprRenderTarget, TprResult> createRenderTarget(const TprRenderTargetCreateInfo* pInfo) noexcept override;
        void destroyRenderTarget(TprRenderTarget target) noexcept override;
        expected<TprObjectImage, TprResult> createObjectImage(const TprObjectImageCreateInfo* pInfo) noexcept override;
        void destroyObjectImage(TprObjectImage image) noexcept override;

    private:

        Allocator createAllocator();
        
        expected<Buffer, TprResult> createBuffer(
            uint32_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags property,
            VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE, std::span<uint32_t> queueFamilyIndices = {}
        );

        expected<WindowContext, TprResult> createWindowContext(uint32_t queueFamilyIndex, TprWindow window);

        Logger& mrLogger;
        ResourceRegistry& mrResReg;
        WindowManager& mrWinMan;
        Settings& mrSettings;
        SceneGraph& mrScGr;
        TprComponent mComponentRenderable;
        uint32_t mObjectChunkSize;
        double mObjectBufferGrowth;

        VulkanSymbols mSym;
        std::optional<Allocator> mAlloc;

        VkInstance mInstance = VK_NULL_HANDLE;
        uint32_t mApiVer;
        VkDevice mDevice = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT mDebugMessenger = VK_NULL_HANDLE;
        std::vector<const char*> mInstanceExtensions;
        VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
        VkQueue mRenderQueue = VK_NULL_HANDLE;
        VkCommandPool mCommandPool = VK_NULL_HANDLE;
        VkCommandBuffer mImmidiateCopyCmdBuffer = VK_NULL_HANDLE;
        VkFence mImmidiateCopyFence = VK_NULL_HANDLE;
        VkDescriptorSetLayout mObjectDataSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout mBasicPipelinelayout = VK_NULL_HANDLE;
        // VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
        // VkDescriptorSet mObjectDataSet = VK_NULL_HANDLE;

        std::unordered_map<uint64_t, WindowContext> mWindowContexts;
        uint32_t mFrameCounter = 0;
        uint32_t mMaxFramesInFlight;

        std::unordered_map<uint32_t, GeometryIndexBuffer> mIndexBuffers;
        uint32_t mGeoIndexBufferCount = 0;
        std::unordered_map<uint32_t, GeometryVertexBuffer> mVertexBuffers;
        uint32_t mGeoVertexBufferCount = 0;
        uint32_t mGeometryBufferSize;

        std::unordered_map<uint32_t, DepthDomain> mDepthDomains;
        uint32_t mDepthDomainCounter = 0;
        std::vector<uint32_t> mDepthDomainOrder;

        std::unordered_map<uint32_t, RenderTarget> mRenderTargets;
        uint32_t mRenderTargetCounter = 0;

        std::unordered_map<uint64_t, Mesh> mMeshes;

        std::unordered_map<uint32_t, ObjectImage> mObjectImages;
        uint32_t mObjectImageCounter = 0;

        std::unordered_map<uint64_t, ChunkEntry> mChunks;
        TprResource mRenderableChunkFetchResource;
        std::optional<Buffer> mObjectBuffer;
        std::optional<Buffer> mObjectIndicesBuffer;
        std::vector<uint32_t> mObjectsFreeList;

};




#endif  // HARDWARE_LAYER_INTERFACE_LAYERS_VULKAN_HARDWARE_LAYER_HPP_

