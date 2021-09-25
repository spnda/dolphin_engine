#include "engine.hpp"

#include "sdl/window.hpp"
#include "vulkan/utils.hpp"

dp::Engine::Engine(dp::Context& context)
        : ctx(context), modelManager(ctx), swapchain(ctx, ctx.surface),
          camera(ctx), ui(ctx, swapchain), storageImage(ctx),
          raygenShaderBindingTable(ctx, "raygenShaderBindingTable"), missShaderBindingTable(ctx, "missShaderBindingTable"), hitShaderBindingTable(ctx, "hitShaderBindingTable") {
    this->getProperties();
    
    camera.setPerspective(70.0f, 0.01f, 512.0f);
    camera.setRotation(glm::vec3(0.0f));
    camera.setPosition(glm::vec3(0.0f, 0.0f, -1.0f));

    ui.init();
    
    VkCommandBuffer commandBuffer = ctx.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, ctx.commandPool, true);
    storageImage.changeLayout(commandBuffer, VK_IMAGE_LAYOUT_GENERAL);
    ctx.flushCommandBuffer(commandBuffer, ctx.graphicsQueue);

    // Load the models and create the pipeline.
    modelManager.loadMeshes({
        "models/Buddha/buddha.obj",
        "models/CornellBox/CornellBox-Original.obj",
        "models/Dragon/dragon.obj"});
    modelManager.buildTopLevelAccelerationStructure();
    this->buildPipeline();

    this->buildSBT();
}

void dp::Engine::getProperties() {
    VkPhysicalDeviceProperties2 deviceProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &this->rtProperties,
    };
    vkGetPhysicalDeviceProperties2(ctx.physicalDevice, &deviceProperties);
}

void dp::Engine::buildPipeline() {
    dp::ShaderModule raygenShader = ctx.createShader("shaders/raygen.rgen.spv", dp::ShaderStage::RayGeneration);
    dp::ShaderModule raymissShader = ctx.createShader("shaders/miss.rmiss.spv", dp::ShaderStage::RayMiss);
    dp::ShaderModule rayhitShader = ctx.createShader("shaders/closesthit.rchit.spv", dp::ShaderStage::ClosestHit);

    auto builder = dp::RayTracingPipelineBuilder::create(ctx, "rt_pipeline")
        .addShader(raygenShader)
        .addShader(raymissShader)
        .addShader(rayhitShader);

    VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo = {};
    descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    descriptorAccelerationStructureInfo.pAccelerationStructures = &modelManager.getTLAS().handle;
    builder.addAccelerationStructureDescriptor(
        0, &descriptorAccelerationStructureInfo,
        VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, dp::ShaderStage::RayGeneration | dp::ShaderStage::ClosestHit
    );

    VkDescriptorImageInfo storageImageDescriptor = storageImage.getDescriptorImageInfo();
    builder.addImageDescriptor(
        1, &storageImageDescriptor,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, dp::ShaderStage::RayGeneration
    );

    VkDescriptorBufferInfo cameraBufferInfo = camera.getDescriptorInfo();
    builder.addBufferDescriptor(
        2, &cameraBufferInfo,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, dp::ShaderStage::RayGeneration
    );

    VkDescriptorBufferInfo materialBufferInfo = modelManager.getMaterialBuffer().getDescriptorInfo(VK_WHOLE_SIZE);
    builder.addBufferDescriptor(
        3, &materialBufferInfo,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, dp::ShaderStage::ClosestHit
    );

    pipeline = builder.build();
}

void dp::Engine::buildSBT() {
    const uint32_t handleSize = rtProperties.shaderGroupHandleSize;
    sbtStride = dp::Buffer::alignedSize(handleSize, rtProperties.shaderGroupHandleAlignment);
    const uint32_t groupCount = 3; // fix
    const uint32_t sbtSize = groupCount * sbtStride;

    std::vector<uint8_t> shaderHandleStorage(sbtSize);
    ctx.getRayTracingShaderGroupHandles(pipeline.pipeline, groupCount, sbtSize, shaderHandleStorage);

    const VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    const VmaMemoryUsage memoryUsageFlags = VMA_MEMORY_USAGE_CPU_ONLY;
    raygenShaderBindingTable.create(handleSize, bufferUsageFlags, memoryUsageFlags, 0);
    missShaderBindingTable.create(handleSize, bufferUsageFlags, memoryUsageFlags, 0);
    hitShaderBindingTable.create(handleSize, bufferUsageFlags, memoryUsageFlags, 0);

    raygenShaderBindingTable.memoryCopy(shaderHandleStorage.data(), handleSize);
    missShaderBindingTable.memoryCopy(shaderHandleStorage.data() + sbtStride, handleSize);
    hitShaderBindingTable.memoryCopy(shaderHandleStorage.data() + sbtStride * 2, handleSize);
}

void dp::Engine::renderLoop() {
    VkResult result;
    uint32_t imageIndex = 0;
    while (!ctx.window->shouldClose()) {
        // Handle SDL events and wait for when we can render the next frame.
        ctx.window->handleEvents(*this);
        if (!needsResize) {
            result = ctx.waitForFrame(swapchain);
            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                needsResize = true;
            } else {
                checkResult(result, "Failed to present queue");
            }
        }
        if (needsResize) break;

        // Update the camera buffer.
        camera.updateBuffer();

        ctx.beginCommandBuffer(ctx.drawCommandBuffer);
        auto image = swapchain.images[ctx.currentImageIndex];

        vkCmdBindPipeline(ctx.drawCommandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.pipeline);
        vkCmdBindDescriptorSets(ctx.drawCommandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.pipelineLayout, 0, 1, &pipeline.descriptorSet, 0, 0);

        ctx.traceRays(
            ctx.drawCommandBuffer,
            raygenShaderBindingTable,
            missShaderBindingTable,
            hitShaderBindingTable,
            sbtStride,
            { ctx.width, ctx.height, 1 }
        );

        // Move storage image to swapchain image.
        storageImage.changeLayout(ctx.drawCommandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        dp::Image::changeLayout(image, ctx.drawCommandBuffer,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                0, VK_ACCESS_TRANSFER_WRITE_BIT,
                                defaultSubresourceRange);

        ctx.copyStorageImage(ctx.drawCommandBuffer, storageImage.image.imageExtent, storageImage.image, image);

        storageImage.changeLayout(ctx.drawCommandBuffer, VK_IMAGE_LAYOUT_GENERAL);
        dp::Image::changeLayout(image, ctx.drawCommandBuffer,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                VK_ACCESS_TRANSFER_WRITE_BIT, 0,
                                defaultSubresourceRange);

        // Draw the UI.
        ui.prepare();
        ui.draw(ctx.drawCommandBuffer);

        // End the command buffer and submit.
        vkEndCommandBuffer(ctx.drawCommandBuffer);

        ctx.graphicsQueue.lock();
        result = ctx.submitFrame(swapchain);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            needsResize = true;
        } else {
            checkResult(result, "Failed to submit queue");
        }
        ctx.graphicsQueue.waitIdle();
        ctx.graphicsQueue.unlock();
    }
}

void dp::Engine::resize(const uint32_t width, const uint32_t height) {
    ctx.waitForIdle();
    ctx.width = width;
    ctx.height = height;

    // Re-create the swapchain.
    swapchain.create(ctx.device);
    
    // Creates a new image and image view.
    storageImage.recreateImage();

    // Change the layout of the new image.
    VkCommandBuffer commandBuffer = ctx.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, ctx.commandPool, true);
    storageImage.changeLayout(commandBuffer, VK_IMAGE_LAYOUT_GENERAL);
    ctx.flushCommandBuffer(commandBuffer, ctx.graphicsQueue);

    // Re-bind the storage image.
    VkDescriptorImageInfo storageImageDescriptor = storageImage.getDescriptorImageInfo();
    VkWriteDescriptorSet resultImageWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = pipeline.descriptorSet,
        .dstBinding = 1,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &storageImageDescriptor,
    };
    vkUpdateDescriptorSets(ctx.device, 1, &resultImageWrite, 0, nullptr);

    // Let the UI resize.
    ui.resize();

    needsResize = false;
}
