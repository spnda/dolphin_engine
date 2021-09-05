#include <chrono>
#include <thread>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "sdl/window.hpp"
#include "vulkan/base/instance.hpp"
#include "vulkan/base/device.hpp"
#include "vulkan/base/swapchain.hpp"
#include "vulkan/resource/buffer.hpp"
#include "vulkan/resource/scratch_buffer.hpp"
#include "vulkan/resource/uniform_data.hpp"
#include "vulkan/rt/acceleration_structure_builder.hpp"
#include "vulkan/rt/acceleration_structure.hpp"
#include "vulkan/rt/rt_pipeline.hpp"
#include "vulkan/rt/top_level_acceleration_structure.hpp"
#include "vulkan/context.hpp"

static const std::vector<Vertex> cubeVertices = {
    // Triangle from https://github.com/SaschaWillems/Vulkan/blob/master/examples/raytracingbasic/raytracingbasic.cpp#L264
	{ {  1.0f,  1.0f, 0.0f } },
	{ { -1.0f,  1.0f, 0.0f } },
	{ {  0.0f, -1.0f, 0.0f } }

    /*{ { -1.0f, -1.0f, -1.0f, } },
    { { -1.0f, -1.0f,  1.0f, } },
    { { -1.0f,  1.0f,  1.0f, } },

    { {  1.0f,  1.0f, -1.0f, } },
    { { -1.0f, -1.0f, -1.0f, } },
    { { -1.0f,  1.0f, -1.0f, } },

    { {  1.0f, -1.0f,  1.0f, } },
    { { -1.0f, -1.0f, -1.0f, } },
    { { 1.0f, -1.0f, -1.0f, } },

    { 1.0f, 1.0f,-1.0f, },
    { 1.0f,-1.0f,-1.0f, },
    { -1.0f,-1.0f,-1.0f, },
    { -1.0f,-1.0f,-1.0f, },
    { -1.0f, 1.0f, 1.0f, },
    { -1.0f, 1.0f,-1.0f, },
    { 1.0f,-1.0f, 1.0f, },
    { -1.0f,-1.0f, 1.0f, },
    { -1.0f,-1.0f,-1.0f, },
    { -1.0f, 1.0f, 1.0f, },
    { -1.0f,-1.0f, 1.0f, },
    { 1.0f,-1.0f, 1.0f, },
    { 1.0f, 1.0f, 1.0f, },
    { 1.0f,-1.0f,-1.0f, },
    { 1.0f, 1.0f,-1.0f, },
    { 1.0f,-1.0f,-1.0f, },
    { 1.0f, 1.0f, 1.0f, },
    { 1.0f,-1.0f, 1.0f, },
    { 1.0f, 1.0f, 1.0f, },
    { 1.0f, 1.0f,-1.0f, },
    { -1.0f, 1.0f,-1.0f, },
    { 1.0f, 1.0f, 1.0f, },
    { -1.0f, 1.0f,-1.0f, },
    { -1.0f, 1.0f, 1.0f, },
    { 1.0f, 1.0f, 1.0f, },
    { -1.0f, 1.0f, 1.0f, },
    { 1.0f,-1.0f, 1.0f },*/
};

static const std::vector<uint32_t> cubeIndices = { 0, 1, 2 };
//static const std::vector<uint32_t> cubeIndices = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

dp::RayTracingPipeline buildRTPipeline(dp::Context& ctx, const dp::Image& storageImage, const dp::Buffer& uboBuffer, const dp::TopLevelAccelerationStructure topLevelAS) {
    dp::ShaderModule raygenShader = ctx.createShader("shaders/raygen.rgen.spv", dp::ShaderStage::RayGeneration);
    dp::ShaderModule raymissShader = ctx.createShader("shaders/miss.rmiss.spv", dp::ShaderStage::RayMiss);
    dp::ShaderModule rayhitShader = ctx.createShader("shaders/closesthit.rchit.spv", dp::ShaderStage::ClosestHit);

    return dp::RayTracingPipelineBuilder::create(ctx, "rt_pipeline")
        .useDefaultDescriptorLayout()
        .createDefaultDescriptorSets(storageImage, uboBuffer, topLevelAS)
        .addShader(raygenShader)
        .addShader(raymissShader)
        .addShader(rayhitShader)
        .build();
}

dp::TopLevelAccelerationStructure buildAccelerationStructures(dp::Context& context) {
    auto builder = dp::AccelerationStructureBuilder::create(context);
    uint32_t meshDeviceAddress = builder.addMesh(dp::AccelerationStructureMesh {
        .vertices = cubeVertices,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .indices = cubeIndices,
        .indexType = VK_INDEX_TYPE_UINT32,
        .stride = sizeof(Vertex),
        .transformMatrix = {1.0f, 0.0f, 0.0f, 0.0f,
                            0.0f, 1.0f, 0.0f, 0.0f,
                            0.0f, 0.0f, 1.0f, 0.0f},
    });
    builder.setInstance(dp::AccelerationStructureInstance {
        .transformMatrix = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f
        },
        .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
        .blasAddress = meshDeviceAddress,
    });
    return builder.build();
}

// Creates a SBT. Warn: Needs updating if any new shaders are added.
uint32_t createShaderBindingTable(const dp::Context& context, const VkPipeline pipeline, dp::Buffer& raygenBuffer, dp::Buffer& missBuffer, dp::Buffer& hitBuffer) {
    const uint32_t handleSize = 32; // Might be specific to my RTX 2080.
    const uint32_t handleSizeAligned = dp::Buffer::alignedSize(handleSize, 64); // Might be specific to my RTX 2080.
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

void initCamera(const dp::Context& ctx, UniformData* data) {
    auto perspective = glm::perspective(glm::radians(70.0f), ctx.window->getAspectRatio(), 0.1f, 512.0f);
    data->projInverse = glm::inverse(perspective);

    auto rotation = glm::vec3(0.0f, 0.0f, 0.0f);
    auto position = glm::vec3(0.0f, 0.0f, -2.5f);
    auto rotM = glm::mat4(1.0f);
    glm::mat4 transM;
    rotM = glm::rotate(rotM, glm::radians(rotation.x * (-1.0f)), glm::vec3(1.0f, 0.0f, 0.0f));
	rotM = glm::rotate(rotM, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	rotM = glm::rotate(rotM, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    transM = glm::translate(glm::mat4(1.0f), position);
    data->viewInverse = glm::inverse(transM * rotM);
}

int main(int argc, char* argv[]) {
    uint32_t width = 1920, height = 1080;

    // VkInstance, VkDevice and Window handled.
    dp::Context ctx = dp::ContextBuilder
        ::create("Dolphin")
        .setDimensions(width, height)
        .build();

    dp::TopLevelAccelerationStructure as = buildAccelerationStructures(ctx);

    dp::VulkanSwapchain swapchain(ctx, ctx.surface);

    UniformData uniformData;
    initCamera(ctx, &uniformData);
    dp::Buffer uboBuffer(ctx);
    uboBuffer.create(
        sizeof(UniformData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    dp::Image storageImage(
        ctx,
        { width, height },
        VK_FORMAT_B8G8R8A8_UNORM,
        // swapchain.getFormat(),
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    VkCommandBuffer commandBuffer = ctx.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, ctx.commandPool, true);
    dp::Image::changeLayout(
        storageImage.image, commandBuffer,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        0, 0,
        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
    ctx.flushCommandBuffer(commandBuffer, ctx.graphicsQueue);

    dp::RayTracingPipeline pipeline = buildRTPipeline(ctx, storageImage, uboBuffer, as);

    dp::Buffer raygenShaderBindingTable(ctx);
    dp::Buffer missShaderBindingTable(ctx);
    dp::Buffer hitShaderBindingTable(ctx);
    uint32_t sbtStride = createShaderBindingTable(ctx, pipeline.pipeline, raygenShaderBindingTable, missShaderBindingTable, hitShaderBindingTable);

    VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    uint32_t imageIndex = 0;
    while(!ctx.window->shouldClose()) {
        ctx.window->handleEvents();
        ctx.waitForFrame(swapchain);

        // vkResetCommandBuffer(ctx.drawCommandBuffer, 0);

        // Command buffer begin
        VkCommandBufferBeginInfo bufferBeginInfo = {};
        bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(ctx.drawCommandBuffer, &bufferBeginInfo);

        auto image = swapchain.images[ctx.currentImageIndex];

        vkCmdBindPipeline(ctx.drawCommandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.pipeline);
        vkCmdBindDescriptorSets(ctx.drawCommandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.pipelineLayout, 0, 1, &pipeline.descriptorSet, 0, 0);

        // Update UBO
        // TODO: Update camera matrices.
        uboBuffer.memoryCopy(&uniformData, sizeof(UniformData));

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
        dp::Image::changeLayout(storageImage.image, ctx.drawCommandBuffer,
                                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                0, VK_ACCESS_TRANSFER_READ_BIT,
                                subresourceRange);
        dp::Image::changeLayout(image, ctx.drawCommandBuffer,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                0, VK_ACCESS_TRANSFER_WRITE_BIT,
                                subresourceRange);

        ctx.copyStorageImage(ctx.drawCommandBuffer, storageImage.imageExtent, storageImage.image, image);

        dp::Image::changeLayout(storageImage.image, ctx.drawCommandBuffer,
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                                VK_ACCESS_TRANSFER_READ_BIT, 0,
                                subresourceRange);
        dp::Image::changeLayout(image, ctx.drawCommandBuffer,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                VK_ACCESS_TRANSFER_WRITE_BIT, 0,
                                subresourceRange);

        vkEndCommandBuffer(ctx.drawCommandBuffer);

        // std::this_thread::sleep_for(std::chrono::milliseconds(10));

        ctx.submitFrame(swapchain);

        vkQueueWaitIdle(ctx.graphicsQueue);
        // ctx.waitForIdle();
    }

    return 0;
}
