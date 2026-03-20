

#ifndef HARDWARE_LAYER_INTERFACE_LAYERS_VULKAN_HARDWARE_LAYER_HPP_
#define HARDWARE_LAYER_INTERFACE_LAYERS_VULKAN_HARDWARE_LAYER_HPP_



#include "core.hpp"
#include "plugin_core.h"
#include "hardware_layer_interface.hpp"
#include "logger.hpp"
#include "resource_registry.hpp"
#include "window_manager.hpp"
#include <SDL2/SDL_vulkan.h>
#include <array>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>




#define TOF(call) do { \
    auto _r = (call); \
    if (_r) throw Exception(ErrCode::InternalError, __FILE__ + ":"s + std::to_string(__LINE__) + " "s + #call + ": returned "s + std::to_string(_r)); \
} while(0)



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



struct Frame {

    inline VkCommandBuffer& renderCommandBuffer() noexcept { return commandBuffers[0]; }
    inline VkCommandBuffer& presentCommandBuffer() noexcept { return commandBuffers[1]; }

    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    VkFence inFlightFence = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffers[2] = {};
};


struct RenderPass {
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout debugLinesPipelineLayout = VK_NULL_HANDLE;
    VkPipeline debugLinesPipeline = VK_NULL_HANDLE;
};


struct WindowContext {

    TprWindow windowHandle;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;

    std::vector<VkSemaphore> semaphores;

    std::vector<VkImage> chainImages;
    std::vector<VkImageView> chainImageViews;
    VkFormat chainImageFormat;

    std::vector<VkImage> depthImages;
    std::vector<VkImageView> depthImageViews;
    std::vector<VkDeviceMemory> depthImageMemories;
    VkFormat depthImageFormat;

    VkExtent2D extent;
    uint32_t imageCount = 0;

    std::vector<VkFramebuffer> framebuffers;
    std::vector<Frame> frames;
    uint32_t poolQueueFamily;
    RenderPass renderPass;
};


struct Buffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void* map = nullptr;
    uint32_t size;
    VkBufferUsageFlags usage;
    VkMemoryPropertyFlags property;
    VkSharingMode sharingMode;
    std::vector<uint32_t> queueFamilyIndices;
    VkDeviceSize mapOffset;
    VkDeviceSize mapSize;
    VkMemoryMapFlags mapFlags;
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
};


class HardwareLayerVulkan : public HardwareLayer {

    public:

        HardwareLayerVulkan(
            Logger& rLogger, ResourceRegistry& rResReg, WindowManager& rWinMan, uint8_t engineVersionVariant,
            uint8_t engineVersionMajor, uint8_t engineVersionMinor, uint8_t engineVersionPatch
        );

        ~HardwareLayerVulkan() noexcept;

        void update() override;
        
        uint32_t getFrameWidth(TprWindow handle) const override { return mWindowContexts.at(get_basic_handle_index(handle)).extent.width; }
        uint32_t getFrameHeight(TprWindow handle) const override { return mWindowContexts.at(get_basic_handle_index(handle)).extent.height; }
        
        TprResult registerWindow(TprWindow handle) noexcept override;

        void unregisterWindow(TprWindow handle) noexcept override;

        TprResult render(const RenderGraph& renderGraph) override;

        [[deprecated]] void renderDebugLines(const std::vector<DebugLineVertex>& debugLinesVertices, CameraProject cameraProject = CameraProject::Ortho);
        [[deprecated]] void renderGUI(const GUIDrawDesc& desc);

    private:

        // buffer functions
        TprResult allocateBuffer(
            Buffer& buffer, uint32_t size, VkBufferUsageFlags usage, VkMemoryPropertyFlags property,
            VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE, const uint32_t* pQueueFamilyIndices = nullptr, uint32_t queueFamilyIndexCount = 0
        );
        TprResult mapBufferMemory(Buffer& buffer, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE, VkMemoryMapFlags flags = 0);
        void unmapBufferMemory(Buffer& buffer) noexcept;
        void freeBuffer(Buffer& buffer) noexcept;
        TprResult resizeBuffer(Buffer& buffer, uint32_t newSize);

        // window context functions
        TprResult constructWindowContext(
            WindowContext& ctx, uint32_t queueFamilyIndex, TprWindow window,
            bool constructSurface = true, bool constructRenderPass = true
        );
        TprResult reconstructWindowContext(WindowContext& ctx);
        void destroyWindowContext(WindowContext& ctx) noexcept;

        // utility functions
        expected<uint32_t, TprResult> findMemoryType(uint32_t memType, VkMemoryPropertyFlags property);

        Logger& mrLogger;
        ResourceRegistry& mrResReg;
        WindowManager& mrWinMan;
        VulkanSymbols mSym;

        VkInstance mInstance = VK_NULL_HANDLE;
        uint32_t mApiVer;
        VkDevice mDevice = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT mDebugMessenger = VK_NULL_HANDLE;
        std::vector<const char*> mInstanceExtensions;
        Buffer mDebugLinesBuffer;
        VkPhysicalDevice mPhysicalDevice;
        VkQueue mRenderQueue;

        std::unordered_map<uint32_t, WindowContext> mWindowContexts;
        uint32_t mFrameCounter = 0;
        uint32_t mMaxFramesInFlight;

};




#endif  // HARDWARE_LAYER_INTERFACE_LAYERS_VULKAN_HARDWARE_LAYER_HPP_

