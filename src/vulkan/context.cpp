#include <vulkan/vulkan.h>

#define VMA_IMPLEMENTATION // Only needed in a single source file.
#include <vk_mem_alloc.h>

#include <utility>

#include "VkBootstrap.h"

#include "../sdl/window.hpp"
#include "context.hpp"
#include "resource/image.hpp"
#include "base/swapchain.hpp"
#include "resource/buffer.hpp"
#include "utils.hpp"

#define DEFAULT_FENCE_TIMEOUT 100000000000

dp::Context::Context(std::string name)
        : applicationName(std::move(name)),
          instance(*this),
          presentCompleteSemaphore(*this, "presentCompleteSemaphore"),
          renderCompleteSemaphore(*this, "renderCompleteSemaphore"),
          renderFence(*this, "renderFence"),
          graphicsQueue(*this, "graphicsQueue") {

}

void dp::Context::init() {
    window = new Window(applicationName, width, height);
    instance.addExtensions(window->getExtensions());
    instance.create(applicationName);
    surface = window->createSurface(instance);
    physicalDevice.create(instance, surface);
    device.create(physicalDevice);

    getVulkanFunctions();
    graphicsQueue.create(vkb::QueueType::graphics);
    commandPool = createCommandPool(device.getQueueIndex(vkb::QueueType::graphics), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    drawCommandBuffer = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, commandPool, false, 0, "drawCommandBuffer");
    buildSyncStructures();
    buildVmaAllocator();
}

void dp::Context::destroy() const {
    presentCompleteSemaphore.destroy();
    renderCompleteSemaphore.destroy();
    renderFence.destroy();

    vkDestroyCommandPool(device, commandPool, nullptr);

    vmaDestroyAllocator(vmaAllocator);

    device.destroy();
    instance.destroy();
}

void dp::Context::buildSyncStructures() {
    renderFence.create(VK_FENCE_CREATE_SIGNALED_BIT);

    presentCompleteSemaphore.create(0);
    renderCompleteSemaphore.create(0);

    setDebugUtilsName(renderCompleteSemaphore, "renderCompleteSemaphore");
    setDebugUtilsName(presentCompleteSemaphore, "presentCompleteSemaphore");
}

void dp::Context::buildVmaAllocator() {
    VmaAllocatorCreateInfo allocatorInfo = {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = physicalDevice,
        .device = device,
        .instance = instance,
    };
    vmaCreateAllocator(&allocatorInfo, &vmaAllocator);
}

void dp::Context::getVulkanFunctions() {
    vkCreateAccelerationStructureKHR = device.getFunctionAddress<PFN_vkCreateAccelerationStructureKHR>("vkCreateAccelerationStructureKHR");
    vkCreateRayTracingPipelinesKHR = device.getFunctionAddress<PFN_vkCreateRayTracingPipelinesKHR>("vkCreateRayTracingPipelinesKHR");
    vkCmdBuildAccelerationStructuresKHR = device.getFunctionAddress<PFN_vkCmdBuildAccelerationStructuresKHR>("vkCmdBuildAccelerationStructuresKHR");
    vkCmdSetCheckpointNV = device.getFunctionAddress<PFN_vkCmdSetCheckpointNV>("vkCmdSetCheckpointNV");
    vkCmdTraceRaysKHR = device.getFunctionAddress<PFN_vkCmdTraceRaysKHR>("vkCmdTraceRaysKHR");
    vkDestroyAccelerationStructureKHR = device.getFunctionAddress<PFN_vkDestroyAccelerationStructureKHR>("vkDestroyAccelerationStructureKHR");
    vkGetAccelerationStructureBuildSizesKHR = device.getFunctionAddress<PFN_vkGetAccelerationStructureBuildSizesKHR>("vkGetAccelerationStructureBuildSizesKHR");
    vkGetAccelerationStructureDeviceAddressKHR = device.getFunctionAddress<PFN_vkGetAccelerationStructureDeviceAddressKHR>("vkGetAccelerationStructureDeviceAddressKHR");
    vkGetQueueCheckpointDataNV = device.getFunctionAddress<PFN_vkGetQueueCheckpointDataNV>("vkGetQueueCheckpointDataNV");
    vkGetRayTracingShaderGroupHandlesKHR = device.getFunctionAddress<PFN_vkGetRayTracingShaderGroupHandlesKHR>("vkGetRayTracingShaderGroupHandlesKHR");
    vkSetDebugUtilsObjectNameEXT = instance.getFunctionAddress<PFN_vkSetDebugUtilsObjectNameEXT>("vkSetDebugUtilsObjectNameEXT");
}

auto dp::Context::createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin, VkCommandBufferUsageFlags bufferUsageFlags, const std::string& name) const -> VkCommandBuffer {
    VkCommandBufferAllocateInfo bufferAllocateInfo = {};
    bufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    bufferAllocateInfo.pNext = nullptr;
    bufferAllocateInfo.level = level;
    bufferAllocateInfo.commandPool = pool;
    bufferAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    auto result = vkAllocateCommandBuffers(device, &bufferAllocateInfo, &commandBuffer);
    checkResult(*this, result, "Failed to allocate command buffers");

    if (begin) {
        beginCommandBuffer(commandBuffer, bufferUsageFlags);
    }

    if (!name.empty()) {
        setDebugUtilsName(commandBuffer, name);
    }

    return commandBuffer;
}

void dp::Context::beginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags bufferUsageFlags) const {
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = bufferUsageFlags,
    };
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
}

void dp::Context::flushCommandBuffer(VkCommandBuffer commandBuffer, const dp::Queue& queue) const {
    if (commandBuffer == VK_NULL_HANDLE) return;

    auto result = vkEndCommandBuffer(commandBuffer);
    checkResult(*this, result, "Failed to end command buffer");

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
    };

    auto guard = std::move(queue.getLock());

    dp::Fence fence(*this);
    fence.create(0);
    result = queue.submit(fence, &submitInfo);
    checkResult(*this, result, "Failed to submit queue while flushing command buffer");
    fence.wait();
    fence.destroy();
}

void dp::Context::oneTimeSubmit(const dp::Queue& queue, VkCommandPool pool,
                                const std::function<void(VkCommandBuffer)>& callback) const {
    auto cmdBuffer = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, pool, true, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    callback(cmdBuffer);
    flushCommandBuffer(cmdBuffer, queue);
    vkFreeCommandBuffers(device, pool, 1, &cmdBuffer);
}

auto dp::Context::waitForFrame(const Swapchain& swapchain) -> VkResult {
    // Wait for fences, then acquire next image.
    renderFence.wait();
    renderFence.reset();
    return swapchain.acquireNextImage(presentCompleteSemaphore, &currentImageIndex);
}

auto dp::Context::submitFrame(const Swapchain& swapchain) -> VkResult {
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT };

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &presentCompleteSemaphore.getHandle(),
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &drawCommandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &renderCompleteSemaphore.getHandle(),
    };
    auto result = graphicsQueue.submit(renderFence, &submitInfo);
    if (result != VK_SUCCESS) {
        checkResult(*this, result, "Failed to submit queue");
    }

    return swapchain.queuePresent(graphicsQueue, currentImageIndex, renderCompleteSemaphore);
}


void dp::Context::buildAccelerationStructures(const VkCommandBuffer commandBuffer, uint32_t geometryCount, VkAccelerationStructureBuildGeometryInfoKHR* buildGeometryInfos, std::vector<VkAccelerationStructureBuildRangeInfoKHR*>& buildRangeInfos) const {
    vkCmdBuildAccelerationStructuresKHR(
        commandBuffer,
        geometryCount,
        buildGeometryInfos,
        buildRangeInfos.data()
    );
}

void dp::Context::setCheckpoint(VkCommandBuffer commandBuffer, const char* marker) const {
#ifdef WITH_NV_AFTERMATH
    if (vkCmdSetCheckpointNV != nullptr)
        vkCmdSetCheckpointNV(commandBuffer, marker);
#endif // #ifdef WITH_NV_AFTERMATH
}

void dp::Context::traceRays(const VkCommandBuffer commandBuffer, const dp::Buffer& raygenSbt, const dp::Buffer& missSbt, const dp::Buffer& hitSbt, const uint32_t stride, const VkExtent3D size) const {
    VkStridedDeviceAddressRegionKHR raygenShaderSbtEntry = {};
    raygenShaderSbtEntry.deviceAddress = raygenSbt.getHostAddress().deviceAddress;
    raygenShaderSbtEntry.stride = stride;
    raygenShaderSbtEntry.size = stride;

    VkStridedDeviceAddressRegionKHR missShaderSbtEntry = {};
    missShaderSbtEntry.deviceAddress = missSbt.getHostAddress().deviceAddress;
    missShaderSbtEntry.stride = stride;
    missShaderSbtEntry.size = stride;

    VkStridedDeviceAddressRegionKHR hitShaderSbtEntry = {};
    hitShaderSbtEntry.deviceAddress = hitSbt.getHostAddress().deviceAddress;
    hitShaderSbtEntry.stride = stride;
    hitShaderSbtEntry.size = stride;

    VkStridedDeviceAddressRegionKHR callableShaderSbtEntry = {};

    vkCmdTraceRaysKHR(
        commandBuffer,
        &raygenShaderSbtEntry,
        &missShaderSbtEntry,
        &hitShaderSbtEntry,
        &callableShaderSbtEntry,
        size.width, size.height, size.depth
    );
}


void dp::Context::buildRayTracingPipeline(VkPipeline* pPipelines, const std::vector<VkRayTracingPipelineCreateInfoKHR>& createInfos) const {
    vkCreateRayTracingPipelinesKHR(
        device,
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        createInfos.size(),
        createInfos.data(),
        nullptr,
        pPipelines
    );
}

void dp::Context::createAccelerationStructure(const VkAccelerationStructureCreateInfoKHR createInfo, VkAccelerationStructureKHR* accelerationStructure) const {
    auto result = vkCreateAccelerationStructureKHR(
        device, &createInfo, nullptr, accelerationStructure);
    checkResult(*this, result, "Failed to create acceleration structure");
}

auto dp::Context::createCommandPool(const uint32_t queueFamilyIndex, const VkCommandPoolCreateFlags flags) const -> VkCommandPool {
    VkCommandPoolCreateInfo commandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = flags,
        .queueFamilyIndex = queueFamilyIndex,
    };
    VkCommandPool pool = nullptr;
    vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &pool);
    return pool;
}

void dp::Context::createDescriptorPool(const uint32_t maxSets, const std::vector<VkDescriptorPoolSize>& poolSizes, VkDescriptorPool* descriptorPool) const {
    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.maxSets = maxSets;
    descriptorPoolCreateInfo.poolSizeCount = poolSizes.size();
    descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();
    vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, descriptorPool);
}

void dp::Context::destroyAccelerationStructure(const VkAccelerationStructureKHR handle) const {
    vkDestroyAccelerationStructureKHR(device, handle, nullptr);
}

auto dp::Context::getAccelerationStructureBuildSizes(const uint32_t primitiveCount, const VkAccelerationStructureBuildGeometryInfoKHR& buildGeometryInfo) const -> VkAccelerationStructureBuildSizesInfoKHR {
    VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo = {};
    buildSizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildGeometryInfo,
        &primitiveCount,
        &buildSizeInfo);

    return buildSizeInfo;
}

auto dp::Context::getAccelerationStructureDeviceAddress(const VkAccelerationStructureKHR handle) const -> VkDeviceAddress {
    VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo = {};
    accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    accelerationDeviceAddressInfo.accelerationStructure = handle;
    return vkGetAccelerationStructureDeviceAddressKHR(device, &accelerationDeviceAddressInfo);
}

uint32_t dp::Context::getBufferDeviceAddress(const VkBufferDeviceAddressInfoKHR& addressInfo) const {
    return vkGetBufferDeviceAddress(device, &addressInfo);
}

auto dp::Context::getCheckpointData(const dp::Queue& queue, uint32_t queryCount) const -> std::vector<VkCheckpointDataNV> {
#ifdef WITH_NV_AFTERMATH
    if (vkGetQueueCheckpointDataNV == nullptr) return {};

    uint32_t count = queryCount;
    vkGetQueueCheckpointDataNV(queue, &count, nullptr); // We first get the count.

    std::vector<VkCheckpointDataNV> checkpoints(count, { VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV });
    vkGetQueueCheckpointDataNV(queue, &count, checkpoints.data()); // Then allocate the array and get the checkpoints.
    return { checkpoints.begin(), checkpoints.end() };
#else
    return {};
#endif // #ifdef WITH_NV_AFTERMATH
}

void dp::Context::getRayTracingShaderGroupHandles(const VkPipeline& pipeline, uint32_t groupCount, uint32_t dataSize, std::vector<uint8_t>& shaderHandles) const {
    vkGetRayTracingShaderGroupHandlesKHR(device, pipeline, 0, groupCount, dataSize, shaderHandles.data());
}


void dp::Context::setDebugUtilsName(const VkAccelerationStructureKHR& as, const std::string& name) const {
    setDebugUtilsName<VkAccelerationStructureKHR>(as, name, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR);
}

void dp::Context::setDebugUtilsName(const VkBuffer& buffer, const std::string& name) const {
    setDebugUtilsName<VkBuffer>(buffer, name, VK_OBJECT_TYPE_BUFFER);
}

void dp::Context::setDebugUtilsName(const VkCommandBuffer& cmdBuffer, const std::string& name) const {
    setDebugUtilsName<VkCommandBuffer>(cmdBuffer, name, VK_OBJECT_TYPE_COMMAND_BUFFER);   
}

void dp::Context::setDebugUtilsName(const VkFence& fence, const std::string& name) const {
    setDebugUtilsName<VkFence>(fence, name, VK_OBJECT_TYPE_FENCE);
}

void dp::Context::setDebugUtilsName(const VkImage& image, const std::string& name) const {
    setDebugUtilsName<VkImage>(image, name, VK_OBJECT_TYPE_IMAGE);
}

void dp::Context::setDebugUtilsName(const VkPipeline& pipeline, const std::string& name) const {
    setDebugUtilsName<VkPipeline>(pipeline, name, VK_OBJECT_TYPE_PIPELINE);
}

void dp::Context::setDebugUtilsName(const VkQueue& queue, const std::string& name) const {
    setDebugUtilsName<VkQueue>(queue, name, VK_OBJECT_TYPE_QUEUE);
}

void dp::Context::setDebugUtilsName(const VkRenderPass& renderPass, const std::string& name) const {
    setDebugUtilsName<VkRenderPass>(renderPass, name, VK_OBJECT_TYPE_RENDER_PASS);
}

void dp::Context::setDebugUtilsName(const VkSemaphore& semaphore, const std::string& name) const {
    setDebugUtilsName<VkSemaphore>(semaphore, name, VK_OBJECT_TYPE_SEMAPHORE);
}

void dp::Context::setDebugUtilsName(const VkShaderModule& shaderModule, const std::string& name) const {
    setDebugUtilsName<VkShaderModule>(shaderModule, name, VK_OBJECT_TYPE_SHADER_MODULE);
}

template <typename T>
void dp::Context::setDebugUtilsName(const T& object, const std::string& name, VkObjectType objectType) const {
    if (vkSetDebugUtilsObjectNameEXT == nullptr) return;

    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.pNext = nullptr;
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = reinterpret_cast<const uint64_t&>(object);
    nameInfo.pObjectName = name.c_str();

    auto result = vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
    checkResult(*this, result, "Failed to set debug utils object name");
}
