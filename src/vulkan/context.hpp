#pragma once

#include <string>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "VkBootstrap.h"

#include "base/shader.hpp"
#include "base/fence.hpp"
#include "base/queue.hpp"

namespace dp {
    // fwd.
    class Buffer;
    class Image;
    class VulkanSwapchain;
    class Window;

    // The global vulkan context. Includes the window, surface,
    // Vulkan instance and devices.
    class Context {
        PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
        PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
        PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
        PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
        PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
        PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
        PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
        PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
        PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
        PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;

        void buildAllocator();
        void buildSyncStructures();

    public:
        std::string name;
        uint64_t version = VK_MAKE_VERSION(1, 0, 0);
        uint32_t width = 1920, height = 1080;

        Window* window;
        VkSurfaceKHR surface;
        dp::Queue graphicsQueue;
        VkCommandPool commandPool;

        // Sync structures
        dp::Fence renderFence;
        VkSemaphore presentCompleteSemaphore;
        VkSemaphore renderCompleteSemaphore;

        vkb::Instance instance;
        vkb::PhysicalDevice physicalDevice;
        vkb::Device device;

        VkCommandBuffer drawCommandBuffer;

        uint32_t currentImageIndex;

        VmaAllocator vmaAllocator;

        Context(const std::string name);
        Context(const Context& ctx) = default;

        void init();
        void destroy();

        void getVulkanFunctions();

        auto createShader(std::string filename, dp::ShaderStage shaderStage) -> dp::ShaderModule;

        auto createCommandBuffer(const VkCommandBufferLevel level, const VkCommandPool pool, bool begin, const std::string name = {}) const -> VkCommandBuffer;
        void beginCommandBuffer(VkCommandBuffer commandBuffer) const;
        void flushCommandBuffer(VkCommandBuffer commandBuffer, const dp::Queue& queue, const VkSemaphore& signalSemaphore = nullptr, const VkSemaphore& waitSemaphore = nullptr, const uint32_t waitMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) const;

        auto waitForFrame(const VulkanSwapchain& swapchain) -> VkResult;
        /** The caller is supposed to lock/unlock the queues. */
        auto submitFrame(const VulkanSwapchain& swapchain) -> VkResult;

        void buildAccelerationStructures(const VkCommandBuffer commandBuffer, const std::vector<VkAccelerationStructureBuildGeometryInfoKHR> infos, std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos) const;
        void copyStorageImage(const VkCommandBuffer commandBuffer, VkExtent2D imageSize, const dp::Image& storageImage, VkImage destination) const;
        void traceRays(const VkCommandBuffer commandBuffer, const dp::Buffer& raygenSbt, const dp::Buffer& missSbt, const dp::Buffer& hitSbt, const uint32_t stride, const VkExtent3D size) const;

        void buildRayTracingPipeline(VkPipeline *pPipelines, const std::vector<VkRayTracingPipelineCreateInfoKHR> createInfos) const;
        auto createAccelerationStructure(const VkAccelerationStructureCreateInfoKHR createInfo, VkAccelerationStructureKHR* accelerationStructure) const -> VkResult;
        auto createCommandPool(const uint32_t queueFamilyIndex, const VkCommandPoolCreateFlags flags) const -> VkCommandPool;
        void createDescriptorPool(const uint32_t maxSets, const std::vector<VkDescriptorPoolSize> poolSizes, VkDescriptorPool* descriptorPool) const;
        void destroyAccelerationStructure(const VkAccelerationStructureKHR handle) const;
        auto getAccelerationStructureBuildSizes(const uint32_t maxPrimitiveCount, const VkAccelerationStructureBuildGeometryInfoKHR& buildGeometryInfo) const -> VkAccelerationStructureBuildSizesInfoKHR;
        auto getAccelerationStructureDeviceAddress(const VkAccelerationStructureKHR handle) const -> VkDeviceAddress;
        void getBufferDeviceAddress(const VkBufferDeviceAddressInfoKHR addressInfo) const;
        auto getQueueIndex(const vkb::QueueType queueType) const -> uint32_t;
        void getRayTracingShaderGroupHandles(const VkPipeline& pipeline, uint32_t groupCount, uint32_t dataSize, std::vector<uint8_t>& shaderHandles) const;
        void waitForIdle() const;

        void setDebugUtilsName(const VkAccelerationStructureKHR& as, const std::string name) const;
        void setDebugUtilsName(const VkBuffer& buffer, const std::string name) const;
        void setDebugUtilsName(const VkFence& fence, const std::string name) const;
        void setDebugUtilsName(const VkCommandBuffer& cmdBuffer, const std::string name) const;
        void setDebugUtilsName(const VkImage& image, const std::string name) const;
        void setDebugUtilsName(const VkPipeline& pipeline, const std::string name) const;
        void setDebugUtilsName(const VkQueue& queue, const std::string name) const;
        void setDebugUtilsName(const VkRenderPass& renderPass, const std::string name) const;
        void setDebugUtilsName(const VkSemaphore& semaphore, const std::string name) const;
        void setDebugUtilsName(const VkShaderModule& shaderModule, const std::string name) const;

        template <typename T>
        void setDebugUtilsName(const T& object, std::string name, VkObjectType objectType) const;
    };
} // namespace dp
