#pragma once

#include <string>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "VkBootstrap.h"

#include "shaders/shader.hpp"

namespace dp {
    // fwd.
    class Buffer;
    class Image;
    class Swapchain;
    class Window;

    // The global vulkan context. Includes the window, surface,
    // Vulkan instance and devices.
    class Context {
        PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR{};
        PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR{};
        PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR{};
        PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR{};
        PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR{};
        PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR{};
        PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR{};
        PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR{};
        PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR{};
        PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT{};

    public:
        uint32_t width = 1920, height = 1080;

        Window* window{};
        VkSurfaceKHR surface{};
        VkQueue graphicsQueue{};
        VkCommandPool commandPool{};

        // Sync structures
        VkFence renderFence{};
        VkSemaphore presentCompleteSemaphore{};
        VkSemaphore renderCompleteSemaphore{};

        vkb::Instance instance;
        vkb::PhysicalDevice physicalDevice;
        vkb::Device device;

        VkCommandBuffer drawCommandBuffer{};

        uint32_t currentImageIndex{};

        VmaAllocator vmaAllocator{};

        Context();

        void destroy() const;

        void getVulkanFunctions();

        auto createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin, const std::string& name = {}) const -> VkCommandBuffer;
        void beginCommandBuffer(VkCommandBuffer commandBuffer) const;
        void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue) const;

        auto waitForFrame(const Swapchain& swapchain) -> VkResult;
        auto submitFrame(const Swapchain& swapchain) -> VkResult;

        void buildAccelerationStructures(VkCommandBuffer commandBuffer, const std::vector<VkAccelerationStructureBuildGeometryInfoKHR>& buildGeometryInfos, std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos) const;
        void copyStorageImage(VkCommandBuffer commandBuffer, VkExtent2D imageSize, const dp::Image& storageImage, VkImage destination) ;
        void traceRays(VkCommandBuffer commandBuffer, const dp::Buffer& raygenSbt, const dp::Buffer& missSbt, const dp::Buffer& hitSbt, uint32_t stride, VkExtent3D size) const;

        void buildRayTracingPipeline(VkPipeline *pPipelines, const std::vector<VkRayTracingPipelineCreateInfoKHR>& createInfos) const;
        void createAccelerationStructure(VkAccelerationStructureCreateInfoKHR createInfo, VkAccelerationStructureKHR* accelerationStructure) const;
        void createDescriptorPool(uint32_t maxSets, const std::vector<VkDescriptorPoolSize>& poolSizes, VkDescriptorPool* descriptorPool) const;
        void destroyAccelerationStructure(VkAccelerationStructureKHR handle) const;
        [[nodiscard]] auto getAccelerationStructureBuildSizes(uint32_t primitiveCount, const VkAccelerationStructureBuildGeometryInfoKHR& buildGeometryInfo) const -> VkAccelerationStructureBuildSizesInfoKHR;
        auto getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR handle) const -> VkDeviceAddress;
        void getBufferDeviceAddress(VkBufferDeviceAddressInfoKHR addressInfo) const;
        void getRayTracingShaderGroupHandles(const VkPipeline& pipeline, uint32_t groupCount, uint32_t dataSize, std::vector<uint8_t>& shaderHandles) const;
        void waitForFence(const VkFence* fence) const;
        void waitForIdle() const;

        void setDebugUtilsName(const VkAccelerationStructureKHR& as, const std::string& name) const;
        void setDebugUtilsName(const VkBuffer& buffer, const std::string& name) const;
        void setDebugUtilsName(const VkCommandBuffer& cmdBuffer, const std::string& name) const;
        void setDebugUtilsName(const VkImage& image, const std::string& name) const;
        void setDebugUtilsName(const VkPipeline& pipeline, const std::string& name) const;
        void setDebugUtilsName(const VkRenderPass& renderPass, const std::string& name) const;
        void setDebugUtilsName(const VkSemaphore& semaphore, const std::string& name) const;
        void setDebugUtilsName(const VkShaderModule& shaderModule, const std::string& name) const;

        template <typename T>
        void setDebugUtilsName(const T& object, const std::string& name, VkObjectType objectType) const;
    };

    /// Context builder to create a context with a VkInstance, VkDevice(s), VkSurfaceKHR and window.
    class ContextBuilder {
    private:
        std::string name;
        int version = VK_MAKE_VERSION(1, 0, 0);
        uint32_t width = 1920, height = 1080;

        static void buildAllocator(Context& ctx);

        static void buildSyncStructures(Context& ctx);

        explicit ContextBuilder(std::string name);

    public:
        static ContextBuilder create(std::string name);

        ContextBuilder& setDimensions(uint32_t width, uint32_t height);

        void setVersion(int version);

        Context build();
    };
} // namespace dp
