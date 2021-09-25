#include <fstream>
#include <sstream>

#include <vulkan/vulkan.h>

#define VMA_IMPLEMENTATION // Only needed in a single source file.
#include <vk_mem_alloc.h>

#include "VkBootstrap.h"

#include "../sdl/window.hpp"
#include "context.hpp"
#include "base/instance.hpp"
#include "base/device.hpp"
#include "resource/image.hpp"
#include "base/swapchain.hpp"
#include "resource/buffer.hpp"
#include "utils.hpp"

#define DEFAULT_FENCE_TIMEOUT 100000000000

dp::Context::Context() {
}

void dp::Context::destroy() {
    vkDestroySemaphore(device, presentCompleteSemaphore, nullptr);
    vkDestroySemaphore(device, renderCompleteSemaphore, nullptr);
    vkDestroyFence(device, renderFence, nullptr);

    vkDestroyCommandPool(device, commandPool, nullptr);

    vkb::destroy_device(device);
    vkb::destroy_instance(instance);
}

void dp::Context::getVulkanFunctions() {
    vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
    vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));
    vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));
    vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
    vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
    vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
    vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR"));
    vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));
    vkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"));
}

auto dp::Context::createShader(std::string filename, dp::ShaderStage shaderStage) -> dp::ShaderModule {
    std::ifstream is(filename, std::ios::binary);

    if (is.is_open()) {
        std::string shaderCode;
        std::stringstream stringBuffer;
        stringBuffer << is.rdbuf();
        shaderCode = stringBuffer.str();

        VkShaderModule shaderModule;
        VkShaderModuleCreateInfo moduleCreateInfo = {};
        moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleCreateInfo.pNext = nullptr;
        moduleCreateInfo.flags = 0;
        moduleCreateInfo.codeSize = shaderCode.size();
        moduleCreateInfo.pCode = (uint32_t*)shaderCode.c_str();

        vkCreateShaderModule(this->device, &moduleCreateInfo, nullptr, &shaderModule);

        return *new dp::ShaderModule(shaderModule, shaderStage);
    } else {
        throw std::runtime_error(std::string("Failed to open shader file: ") + filename);
    }
}

auto dp::Context::createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin, const std::string name) const -> VkCommandBuffer {
    VkCommandBufferAllocateInfo bufferAllocateInfo = {};
    bufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    bufferAllocateInfo.pNext = nullptr;
    bufferAllocateInfo.level = level;
    bufferAllocateInfo.commandPool = pool;
    bufferAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &bufferAllocateInfo, &commandBuffer);

    if (begin) {
        beginCommandBuffer(commandBuffer);
    }

    if (name.size() != 0) {
        setDebugUtilsName(commandBuffer, name);
    }

    return commandBuffer;
}

void dp::Context::beginCommandBuffer(VkCommandBuffer commandBuffer) const {
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
}

void dp::Context::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue) const {
    if (commandBuffer == VK_NULL_HANDLE) return;

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = 0;
    
    VkFence fence;
    vkCreateFence(device, &fenceInfo, nullptr, &fence);
    vkQueueSubmit(queue, 1, &submitInfo, fence);
    waitForFence(&fence);
    vkDestroyFence(device, fence, nullptr);
}

auto dp::Context::waitForFrame(const VulkanSwapchain& swapchain) -> VkResult {
    // Wait for fences, then acquire next image.
    waitForFence(&renderFence);
    return swapchain.aquireNextImage(presentCompleteSemaphore, &currentImageIndex);
}

auto dp::Context::submitFrame(const VulkanSwapchain& swapchain) -> VkResult {
    // VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_TRANSFER_BIT };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
    // VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }; // ??

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &presentCompleteSemaphore;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderCompleteSemaphore;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &drawCommandBuffer;
    VkResult result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, renderFence);
    if (result != VK_SUCCESS) {
        checkResult(result, "Failed to submit queue");
    }

    return swapchain.queuePresent(graphicsQueue, currentImageIndex, renderCompleteSemaphore);
}


void dp::Context::buildAccelerationStructures(const VkCommandBuffer commandBuffer, const std::vector<VkAccelerationStructureBuildGeometryInfoKHR> buildGeometryInfos, std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos) const {
    vkCmdBuildAccelerationStructuresKHR(
        commandBuffer,
        buildGeometryInfos.size(),
        buildGeometryInfos.data(),
        buildRangeInfos.data()
    );
}

void dp::Context::copyStorageImage(const VkCommandBuffer commandBuffer, VkExtent2D imageSize, const dp::Image& storageImage, VkImage destination) const {
    VkImageCopy copyRegion = {};
    copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copyRegion.srcOffset = { 0, 0, 0 };
    copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copyRegion.dstOffset = { 0, 0, 0 };
    copyRegion.extent = { imageSize.width, imageSize.height, 1 };

    vkCmdCopyImage(commandBuffer, storageImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
}

void dp::Context::traceRays(const VkCommandBuffer commandBuffer, const dp::Buffer& raygenSbt, const dp::Buffer& missSbt, const dp::Buffer& hitSbt, const uint32_t stride, const VkExtent3D size) const {
    VkStridedDeviceAddressRegionKHR raygenShaderSbtEntry = {};
    raygenShaderSbtEntry.deviceAddress = raygenSbt.address;
    raygenShaderSbtEntry.stride = stride;
    raygenShaderSbtEntry.size = stride;

    VkStridedDeviceAddressRegionKHR missShaderSbtEntry = {};
    missShaderSbtEntry.deviceAddress = missSbt.address;
    missShaderSbtEntry.stride = stride;
    missShaderSbtEntry.size = stride;

    VkStridedDeviceAddressRegionKHR hitShaderSbtEntry = {};
    hitShaderSbtEntry.deviceAddress = hitSbt.address;
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


void dp::Context::buildRayTracingPipeline(VkPipeline* pPipelines, const std::vector<VkRayTracingPipelineCreateInfoKHR> createInfos) const {
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
    vkCreateAccelerationStructureKHR(
        device, &createInfo, nullptr, accelerationStructure);
}

void dp::Context::createDescriptorPool(const uint32_t maxSets, const std::vector<VkDescriptorPoolSize> poolSizes, VkDescriptorPool* descriptorPool) const {
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

void dp::Context::getBufferDeviceAddress(const VkBufferDeviceAddressInfoKHR addressInfo) const {
    vkGetBufferDeviceAddressKHR(device, &addressInfo);
}

void dp::Context::getRayTracingShaderGroupHandles(const VkPipeline& pipeline, uint32_t groupCount, uint32_t dataSize, std::vector<uint8_t>& shaderHandles) const {
    vkGetRayTracingShaderGroupHandlesKHR(device, pipeline, 0, groupCount, shaderHandles.size(), shaderHandles.data());
}

void dp::Context::waitForFence(const VkFence* fence) const {
    vkWaitForFences(device, 1, fence, true, DEFAULT_FENCE_TIMEOUT);
    vkResetFences(device, 1, fence);
}

void dp::Context::waitForIdle() const {
    vkDeviceWaitIdle(device);
}


void dp::Context::setDebugUtilsName(const VkAccelerationStructureKHR& as, const std::string name) const {
    setDebugUtilsName<VkAccelerationStructureKHR>(as, name, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR);
}

void dp::Context::setDebugUtilsName(const VkBuffer& buffer, const std::string name) const {
    setDebugUtilsName<VkBuffer>(buffer, name, VK_OBJECT_TYPE_BUFFER);
}

void dp::Context::setDebugUtilsName(const VkCommandBuffer& cmdBuffer, const std::string name) const {
    setDebugUtilsName<VkCommandBuffer>(cmdBuffer, name, VK_OBJECT_TYPE_COMMAND_BUFFER);   
}

void dp::Context::setDebugUtilsName(const VkImage& image, const std::string name) const {
    setDebugUtilsName<VkImage>(image, name, VK_OBJECT_TYPE_IMAGE);
}

void dp::Context::setDebugUtilsName(const VkPipeline& pipeline, const std::string name) const {
    setDebugUtilsName<VkPipeline>(pipeline, name, VK_OBJECT_TYPE_PIPELINE);
}

void dp::Context::setDebugUtilsName(const VkRenderPass& renderPass, const std::string name) const {
    setDebugUtilsName<VkRenderPass>(renderPass, name, VK_OBJECT_TYPE_RENDER_PASS);
}

void dp::Context::setDebugUtilsName(const VkSemaphore& semaphore, const std::string name) const {
    setDebugUtilsName<VkSemaphore>(semaphore, name, VK_OBJECT_TYPE_SEMAPHORE);
}

void dp::Context::setDebugUtilsName(const VkShaderModule& shaderModule, const std::string name) const {
    setDebugUtilsName<VkShaderModule>(shaderModule, name, VK_OBJECT_TYPE_SHADER_MODULE);
}

template <typename T>
void dp::Context::setDebugUtilsName(const T& object, std::string name, VkObjectType objectType) const {
    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.pNext = nullptr;
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = reinterpret_cast<const uint64_t&>(object);
    nameInfo.pObjectName = name.c_str();

    vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
}


void dp::ContextBuilder::buildAllocator(Context& ctx) {
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = ctx.physicalDevice;
    allocatorInfo.device = ctx.device;
    allocatorInfo.instance = ctx.instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &ctx.vmaAllocator);
}

void dp::ContextBuilder::buildSyncStructures(Context& ctx) {
    ctx.renderFence = {};
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(ctx.device, &fenceCreateInfo, nullptr, &ctx.renderFence);

    ctx.presentCompleteSemaphore = {};
    ctx.renderCompleteSemaphore = {};
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.flags = 0;
    vkCreateSemaphore(ctx.device, &semaphoreCreateInfo, nullptr, &ctx.presentCompleteSemaphore);
    vkCreateSemaphore(ctx.device, &semaphoreCreateInfo, nullptr, &ctx.renderCompleteSemaphore);

    ctx.setDebugUtilsName(ctx.renderCompleteSemaphore, "renderCompleteSemaphore");
    ctx.setDebugUtilsName(ctx.presentCompleteSemaphore, "presentCompleteSemaphore");
}

dp::ContextBuilder::ContextBuilder(std::string name) : name(name) {}

dp::ContextBuilder dp::ContextBuilder::create(std::string name) {
    dp::ContextBuilder builder(name);
    return builder;
}

dp::ContextBuilder& dp::ContextBuilder::setDimensions(uint32_t width, uint32_t height) {
    this->width = width; this->height = height;
    return *this;
}

void dp::ContextBuilder::setVersion(int version) {
    this->version = version;
}

dp::Context dp::ContextBuilder::build() {
    dp::Context context;
    context.window = new Window(name, width, height);
    context.instance = dp::VulkanInstance::buildInstance(name, version, context.window->getExtensions());
    context.surface = context.window->createSurface(context.instance);
    context.physicalDevice = dp::Device::getPhysicalDevice(context.instance, context.surface);
    context.device = dp::Device::getLogicalDevice(context.instance, context.physicalDevice);
    context.graphicsQueue = getFromVkbResult(context.device.get_queue(vkb::QueueType::graphics));
    context.getVulkanFunctions();
    // We want the pool to have resettable command buffers.
    context.commandPool = dp::Device::createDefaultCommandPool(context.device, getFromVkbResult(context.device.get_queue_index(vkb::QueueType::graphics)), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    context.drawCommandBuffer = context.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, context.commandPool, false, "drawCommandBuffer");
    buildSyncStructures(context);
    buildAllocator(context);
    return context;
}
