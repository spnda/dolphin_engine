#pragma once

#include <string>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "VkBootstrap.h"

#include "base/device.hpp"
#include "base/instance.hpp"
#include "base/physical_device.hpp"
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
        PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
        PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;
        PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
        PFN_vkCmdSetCheckpointNV vkCmdSetCheckpointNV = nullptr;
        PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
        PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
        PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
        PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
        PFN_vkGetQueueCheckpointDataNV vkGetQueueCheckpointDataNV = nullptr;
        PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
        PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT = nullptr;

    public:
        uint32_t width = 1920, height = 1080;

        Window* window = nullptr;
        VkSurfaceKHR surface = nullptr;
        VkQueue graphicsQueue = nullptr;
        VkCommandPool commandPool = nullptr;

        // Sync structures
        VkFence renderFence = nullptr;
        VkSemaphore presentCompleteSemaphore = nullptr;
        VkSemaphore renderCompleteSemaphore = nullptr;

        dp::Instance instance;
        dp::PhysicalDevice physicalDevice;
        dp::Device device;

        VkCommandBuffer drawCommandBuffer = nullptr;

        uint32_t currentImageIndex = 0;

        VmaAllocator vmaAllocator = nullptr;

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
        void setCheckpoint(VkCommandBuffer commandBuffer, const char* marker = nullptr) const;
        void traceRays(VkCommandBuffer commandBuffer, const dp::Buffer& raygenSbt, const dp::Buffer& missSbt, const dp::Buffer& hitSbt, uint32_t stride, VkExtent3D size) const;

        void buildRayTracingPipeline(VkPipeline *pPipelines, const std::vector<VkRayTracingPipelineCreateInfoKHR>& createInfos) const;
        void createAccelerationStructure(VkAccelerationStructureCreateInfoKHR createInfo, VkAccelerationStructureKHR* accelerationStructure) const;
        [[nodiscard]] auto createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags) const -> VkCommandPool;
        void createDescriptorPool(uint32_t maxSets, const std::vector<VkDescriptorPoolSize>& poolSizes, VkDescriptorPool* descriptorPool) const;
        void destroyAccelerationStructure(VkAccelerationStructureKHR handle) const;
        [[nodiscard]] auto getAccelerationStructureBuildSizes(uint32_t primitiveCount, const VkAccelerationStructureBuildGeometryInfoKHR& buildGeometryInfo) const -> VkAccelerationStructureBuildSizesInfoKHR;
        auto getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR handle) const -> VkDeviceAddress;
        [[nodiscard]] auto getBufferDeviceAddress(const VkBufferDeviceAddressInfoKHR& addressInfo) const -> uint32_t;
        auto getCheckpointData(VkQueue queue, uint32_t queryCount) const -> std::vector<VkCheckpointDataNV>;
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
