#pragma once

#include <string>

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include "VkBootstrap.h"

#include "../sdl/window.hpp"
#include "base/shader.hpp"
#include "resource/image.hpp"
#include "utils.hpp"

namespace dp {

// fwd.
class Buffer;
class Image;
class VulkanSwapchain;

// The global vulkan context. Includes the window, surface,
// Vulkan instance and devices.
struct Context {
private:
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;

public:
    Window* window;
    VkSurfaceKHR surface;
    VkQueue graphicsQueue;
    VkCommandPool commandPool;

    // Sync structures
    VkFence renderFence;
    VkSemaphore presentCompleteSemaphore;
    VkSemaphore renderCompleteSemaphore;

    vkb::Instance instance;
    vkb::PhysicalDevice physicalDevice;
    vkb::Device device;

    VkCommandBuffer drawCommandBuffer;

    uint32_t currentImageIndex;

    VmaAllocator vmaAllocator;

    Context();

    void getVulkanFunctions();

    ShaderModule createShader(std::string filename, dp::ShaderStage shaderStage);

    VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin, const std::string name = {}) const;

    void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue) const;

    void waitForFrame(const VulkanSwapchain& swapchain);

    void submitFrame(const VulkanSwapchain& swapchain);

    void buildAccelerationStructures(const VkCommandBuffer commandBuffer, const std::vector<VkAccelerationStructureBuildGeometryInfoKHR> buildGeometryInfos, std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos) const;
    void copyStorageImage(const VkCommandBuffer commandBuffer, VkExtent2D imageSize, const dp::Image& storageImage, VkImage destination) const;
    void traceRays(const VkCommandBuffer commandBuffer, const dp::Buffer& raygenSbt, const dp::Buffer& missSbt, const dp::Buffer& hitSbt, const uint32_t stride, const VkExtent3D size) const;
    
    void buildRayTracingPipeline(VkPipeline *pPipelines, const std::vector<VkRayTracingPipelineCreateInfoKHR> createInfos) const;
    void createAccelerationStructure(const VkAccelerationStructureCreateInfoKHR createInfo, VkAccelerationStructureKHR* accelerationStructure) const;
    void createDescriptorPool(const uint32_t maxSets, const std::vector<VkDescriptorPoolSize> poolSizes, VkDescriptorPool* descriptorPool) const;
    VkAccelerationStructureBuildSizesInfoKHR getAccelerationStructureBuildSizes(const uint32_t primitiveCount, const VkAccelerationStructureBuildGeometryInfoKHR& buildGeometryInfo) const;
    VkDeviceAddress getAccelerationStructureDeviceAddress(const VkAccelerationStructureKHR handle) const;
    void getBufferDeviceAddress(const VkBufferDeviceAddressInfoKHR addressInfo) const;
    void getRayTracingShaderGroupHandles(const VkPipeline& pipeline, uint32_t groupCount, uint32_t dataSize, std::vector<uint8_t>& shaderHandles) const;
    void waitForIdle() const;

    void setDebugUtilsName(const VkSemaphore& semaphore, const std::string name) const;
    void setDebugUtilsName(const VkBuffer& buffer, const std::string name) const;
    void setDebugUtilsName(const VkAccelerationStructureKHR& as, const std::string name) const;
    void setDebugUtilsName(const VkPipeline& pipeline, const std::string name) const;
    void setDebugUtilsName(const VkImage& image, const std::string name) const;
    void setDebugUtilsName(const VkRenderPass& renderPass, const std::string name) const;
    void setDebugUtilsName(const VkCommandBuffer& cmdBuffer, const std::string name) const;

    template <typename T>
    void setDebugUtilsName(const T& object, std::string name, VkObjectType objectType) const;
};

/// Context builder to create a context with a VkInstance, VkDevice(s), VkSurfaceKHR and window.
class ContextBuilder {
private:
    std::string name;
    int version = VK_MAKE_VERSION(1, 0, 0);
    uint32_t width = 1920;
    uint32_t height = 1080;

    void buildAllocator(Context& ctx);

    void buildSyncStructures(Context& ctx);

    ContextBuilder(std::string name);

public:
    static ContextBuilder create(std::string name);

    ContextBuilder& setDimensions(uint32_t width, uint32_t height);

    void setVersion(int version);

    Context build();
};

} // namespace dp
