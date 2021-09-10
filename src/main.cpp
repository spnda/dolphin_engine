#include <chrono>
#include <thread>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "render/camera.hpp"
#include "render/modelloader.hpp"
#include "sdl/window.hpp"
#include "vulkan/base/instance.hpp"
#include "vulkan/base/device.hpp"
#include "vulkan/base/swapchain.hpp"
#include "vulkan/resource/buffer.hpp"
#include "vulkan/resource/scratch_buffer.hpp"
#include "vulkan/resource/storageimage.hpp"
#include "vulkan/resource/uniform_data.hpp"
#include "vulkan/rt/acceleration_structure_builder.hpp"
#include "vulkan/rt/rt_pipeline.hpp"
#include "vulkan/context.hpp"

dp::RayTracingPipeline buildRTPipeline(dp::Context& ctx, const dp::Image& storageImage, const dp::Camera& camera, const dp::AccelerationStructure& topLevelAS) {
    dp::ShaderModule raygenShader = ctx.createShader("shaders/raygen.rgen.spv", dp::ShaderStage::RayGeneration);
    dp::ShaderModule raymissShader = ctx.createShader("shaders/miss.rmiss.spv", dp::ShaderStage::RayMiss);
    dp::ShaderModule rayhitShader = ctx.createShader("shaders/closesthit.rchit.spv", dp::ShaderStage::ClosestHit);

    return dp::RayTracingPipelineBuilder::create(ctx, "rt_pipeline")
        .useDefaultDescriptorLayout()
        .createDefaultDescriptorSets(storageImage, camera, topLevelAS)
        .addShader(raygenShader)
        .addShader(raymissShader)
        .addShader(rayhitShader)
        .build();
}

// Creates a SBT. Warn: Needs updating if any new shaders are added.
uint32_t createShaderBindingTable(const dp::Context& context, const VkPipeline pipeline, dp::Buffer& raygenBuffer, dp::Buffer& missBuffer, dp::Buffer& hitBuffer) {
    const uint32_t handleSize = 32; // Might be specific to my RTX 2080.
    const uint32_t handleSizeAligned = dp::Buffer::alignedSize(handleSize, 32); // Might be specific to my RTX 2080.
    const uint32_t groupCount = 3; // fix
    const uint32_t sbtSize = groupCount * handleSizeAligned;

    std::vector<uint8_t> shaderHandleStorage(sbtSize);
    reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(context.device.device, "vkGetRayTracingShaderGroupHandlesKHR"))
        (context.device.device, pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data());

    const VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    const VmaMemoryUsage memoryUsageFlags = VMA_MEMORY_USAGE_CPU_ONLY;
    raygenBuffer.create(handleSize, bufferUsageFlags, memoryUsageFlags, 0);
    missBuffer.create(handleSize, bufferUsageFlags, memoryUsageFlags, 0);
    hitBuffer.create(handleSize, bufferUsageFlags, memoryUsageFlags, 0);

    raygenBuffer.memoryCopy(shaderHandleStorage.data(), handleSize);
    missBuffer.memoryCopy(shaderHandleStorage.data() + handleSizeAligned, handleSize);
    hitBuffer.memoryCopy(shaderHandleStorage.data() + handleSizeAligned * 2, handleSize);
    return handleSizeAligned;
}

int main(int argc, char* argv[]) {
    uint32_t width = 1920, height = 1080;

    // VkInstance, VkDevice and Window handled.
    dp::Context ctx = dp::ContextBuilder
        ::create("Dolphin")
        .setDimensions(width, height)
        .build();
        
    dp::ModelLoader modelLoader;
    modelLoader.loadFile("models/lost_empire.obj");
    // modelLoader.loadFile("models/Bistro/BistroExterior.fbx");
    // modelLoader.loadFile("models/Bistro/BistroInterior.fbx");
    dp::AccelerationStructure as = modelLoader.buildAccelerationStructure(ctx);

    dp::VulkanSwapchain swapchain(ctx, ctx.surface);

    dp::Camera camera(ctx);
    camera.setPerspective(70.0f, 0.01f, 512.0f);
    camera.setRotation(glm::vec3(0.0f));
    camera.setPosition(glm::vec3(0.0f, 0.0f, -1.0f));

    dp::StorageImage storageImage(ctx, { width, height });
    VkCommandBuffer commandBuffer = ctx.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, ctx.commandPool, true);
    storageImage.changeLayout(commandBuffer, VK_IMAGE_LAYOUT_GENERAL);
    ctx.flushCommandBuffer(commandBuffer, ctx.graphicsQueue);

    dp::RayTracingPipeline pipeline = buildRTPipeline(ctx, storageImage.image, camera, as);

    dp::Buffer raygenShaderBindingTable(ctx, "raygenShaderBindingTable");
    dp::Buffer missShaderBindingTable(ctx, "missShaderBindingTable");
    dp::Buffer hitShaderBindingTable(ctx, "hitShaderBindingTable");
    uint32_t sbtStride = createShaderBindingTable(ctx, pipeline.pipeline, raygenShaderBindingTable, missShaderBindingTable, hitShaderBindingTable);

    VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    uint32_t imageIndex = 0;
    while(!ctx.window->shouldClose()) {
        ctx.window->handleEvents(camera);
        ctx.waitForFrame(swapchain);

        // vkResetCommandBuffer(ctx.drawCommandBuffer, 0);

        // Update UBO
        camera.updateBuffer();

        // Command buffer begin
        VkCommandBufferBeginInfo bufferBeginInfo = {};
        bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(ctx.drawCommandBuffer, &bufferBeginInfo);

        auto image = swapchain.images[ctx.currentImageIndex];

        vkCmdBindPipeline(ctx.drawCommandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.pipeline);
        vkCmdBindDescriptorSets(ctx.drawCommandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.pipelineLayout, 0, 1, &pipeline.descriptorSet, 0, 0);

        ctx.traceRays(
            ctx.drawCommandBuffer,
            raygenShaderBindingTable,
            missShaderBindingTable,
            hitShaderBindingTable,
            sbtStride,
            width,
            height,
            1
        );

        // Move storage image to swapchain image.
        storageImage.changeLayout(ctx.drawCommandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        dp::Image::changeLayout(image, ctx.drawCommandBuffer,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                0, VK_ACCESS_TRANSFER_WRITE_BIT,
                                subresourceRange);

        ctx.copyStorageImage(ctx.drawCommandBuffer, storageImage.image.imageExtent, storageImage.image, image);

        storageImage.changeLayout(ctx.drawCommandBuffer, VK_IMAGE_LAYOUT_GENERAL);
        dp::Image::changeLayout(image, ctx.drawCommandBuffer,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                VK_ACCESS_TRANSFER_WRITE_BIT, 0,
                                subresourceRange);

        vkEndCommandBuffer(ctx.drawCommandBuffer);

        ctx.submitFrame(swapchain);

        vkQueueWaitIdle(ctx.graphicsQueue);
        // ctx.waitForIdle();
    }

    return 0;
}
