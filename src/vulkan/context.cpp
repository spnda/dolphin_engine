#include <fstream>
#include <sstream>

#include <vulkan/vulkan.h>

#define VMA_IMPLEMENTATION // Only needed in a single source file.
#include <vk_mem_alloc.h>

#include "VkBootstrap.h"

#include "context.hpp"
#include "base/instance.hpp"
#include "base/device.hpp"
#include "base/swapchain.hpp"
#include "resource/buffer.hpp"
#include "utils.hpp"

#define DEFAULT_FENCE_TIMEOUT 100000000000

namespace dp {

dp::Context::Context() {
    // vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device.device, "vkCmdTraceRaysKHR"));
}

dp::ShaderModule dp::Context::createShader(std::string filename, dp::ShaderStage shaderStage) {
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

        vkCreateShaderModule(this->device.device, &moduleCreateInfo, nullptr, &shaderModule);

        return *new dp::ShaderModule(shaderModule, shaderStage);
    } else {
        throw std::runtime_error(std::string("Failed to open shader file: ") + filename);
    }
}

VkCommandBuffer dp::Context::createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin) const {
    VkCommandBufferAllocateInfo bufferAllocateInfo = {};
    bufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    bufferAllocateInfo.pNext = nullptr;
    bufferAllocateInfo.level = level;
    bufferAllocateInfo.commandPool = pool;
    bufferAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device.device, &bufferAllocateInfo, &commandBuffer);

    if (begin) {
        VkCommandBufferBeginInfo bufferBeginInfo = {};
        bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(commandBuffer, &bufferBeginInfo);
    }

    return commandBuffer;
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
    vkCreateFence(device.device, &fenceInfo, nullptr, &fence);
    vkQueueSubmit(queue, 1, &submitInfo, fence);
    vkWaitForFences(device.device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT);
    vkDestroyFence(device.device, fence, nullptr);
}

void dp::Context::waitForFrame(const VulkanSwapchain& swapchain) {
    // Wait for fences.
    vkWaitForFences(device.device, 1, &renderFence, true, UINT64_MAX);
    vkResetFences(device.device, 1, &renderFence);

    // Acquire next swapchain image
    swapchain.aquireNextImage(presentCompleteSemaphore, &currentImageIndex);
}

void dp::Context::submitFrame(const VulkanSwapchain& swapchain) {
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
        printf("vkQueueSubmit: %d\n", result);
    }

    // vkWaitForFences(device.device, 1, &renderFence, true, UINT64_MAX);

    swapchain.queuePresent(graphicsQueue, currentImageIndex, renderCompleteSemaphore);
}

void dp::Context::copyStorageImage(const VkCommandBuffer commandBuffer, VkExtent2D imageSize, VkImage storageImage, VkImage destination) {
    VkImageCopy copyRegion = {};
    copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copyRegion.srcOffset = { 0, 0, 0 };
    copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copyRegion.dstOffset = { 0, 0, 0 };
    copyRegion.extent = { imageSize.width, imageSize.height, 1 };

    vkCmdCopyImage(commandBuffer, storageImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
}

void dp::Context::waitForIdle() {
    vkDeviceWaitIdle(device.device);
}

void dp::Context::traceRays(const VkCommandBuffer commandBuffer, const dp::Buffer& raygenSbt, const dp::Buffer& missSbt, const dp::Buffer& hitSbt, const uint32_t stride, const uint32_t width, const uint32_t height, const uint32_t depth) const {
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

    reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device.device, "vkCmdTraceRaysKHR"))(
        commandBuffer,
        &raygenShaderSbtEntry,
        &missShaderSbtEntry,
        &hitShaderSbtEntry,
        &callableShaderSbtEntry,
        width, height, 1
    );
}

void dp::Context::setDebugUtilsName(const VkSemaphore& semaphore, const std::string name) const {
    setDebugUtilsName<VkSemaphore>(semaphore, name, VK_OBJECT_TYPE_SEMAPHORE);
}

void dp::Context::setDebugUtilsName(const VkBuffer& buffer, const std::string name) const {
    setDebugUtilsName<VkBuffer>(buffer, name, VK_OBJECT_TYPE_BUFFER);
}

void dp::Context::setDebugUtilsName(const VkAccelerationStructureKHR& as, const std::string name) const {
    setDebugUtilsName<VkAccelerationStructureKHR>(as, name, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR);
}

void dp::Context::setDebugUtilsName(const VkPipeline& pipeline, const std::string name) const {
    setDebugUtilsName<VkPipeline>(pipeline, name, VK_OBJECT_TYPE_PIPELINE);
}

void dp::Context::setDebugUtilsName(const VkImage& image, const std::string name) const {
    setDebugUtilsName<VkImage>(image, name, VK_OBJECT_TYPE_IMAGE);
}

template <typename T>
void dp::Context::setDebugUtilsName(const T& object, std::string name, VkObjectType objectType) const {
    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.pNext = nullptr;
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = reinterpret_cast<const uint64_t&>(object);
    nameInfo.pObjectName = name.c_str();

    reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetInstanceProcAddr(instance.instance, "vkSetDebugUtilsObjectNameEXT"))
        (device.device, &nameInfo);
}


void ContextBuilder::buildAllocator(Context& ctx) {
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = ctx.physicalDevice.physical_device;
    allocatorInfo.device = ctx.device.device;
    allocatorInfo.instance = ctx.instance.instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &ctx.vmaAllocator);
}

void ContextBuilder::buildSyncStructures(Context& ctx) {
    ctx.renderFence = {};
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(ctx.device.device, &fenceCreateInfo, nullptr, &ctx.renderFence);

    ctx.presentCompleteSemaphore = {};
    ctx.renderCompleteSemaphore = {};
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.flags = 0;
    vkCreateSemaphore(ctx.device.device, &semaphoreCreateInfo, nullptr, &ctx.presentCompleteSemaphore);
    vkCreateSemaphore(ctx.device.device, &semaphoreCreateInfo, nullptr, &ctx.renderCompleteSemaphore);

    ctx.setDebugUtilsName(ctx.renderCompleteSemaphore, "renderCompleteSemaphore");
    ctx.setDebugUtilsName(ctx.presentCompleteSemaphore, "presentCompleteSemaphore");
}

dp::ContextBuilder::ContextBuilder(std::string name) : name(name) {}

dp::ContextBuilder dp::ContextBuilder::create(std::string name) {
    dp::ContextBuilder builder(name);
    return builder;
}

dp::ContextBuilder ContextBuilder::setDimensions(uint32_t width, uint32_t height) {
    this->width = width; this->height = height;
    return *this;
}

void ContextBuilder::setVersion(int version) {
    this->version = version;
}

Context ContextBuilder::build() {
    Context context;
    context.window = new Window(name, width, height);
    context.instance = dp::VulkanInstance::buildInstance(name, version, context.window->getExtensions());
    context.surface = context.window->createSurface(context.instance.instance);
    context.physicalDevice = dp::Device::getPhysicalDevice(context.instance, context.surface);
    context.device = dp::Device::getLogicalDevice(context.instance.instance, context.physicalDevice);
    context.graphicsQueue = getFromVkbResult(context.device.get_queue(vkb::QueueType::graphics));
    // We want the pool to have resettable command buffers.
    context.commandPool = dp::Device::createDefaultCommandPool(context.device, getFromVkbResult(context.device.get_queue_index(vkb::QueueType::graphics)), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    context.drawCommandBuffer = context.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, context.commandPool, false);;
    buildSyncStructures(context);
    buildAllocator(context);
    return context;
}

} // namespace dp
