#include "engine.hpp"

#include "sdl/window.hpp"
#include "vulkan/utils.hpp"

dp::Engine::Engine(dp::Context& context)
        : ctx(context), modelLoader(ctx), swapchain(ctx, ctx.surface),
          camera(ctx), ui(ctx, swapchain), storageImage(ctx), topLevelAccelerationStructure(ctx),
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
    modelLoader.loadFile("models/CornellBox-Original.obj");
    topLevelAccelerationStructure = modelLoader.buildAccelerationStructure(ctx);
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
    dp::ShaderModule rayGenShader(ctx, "raygen", dp::ShaderStage::RayGeneration);
    dp::ShaderModule rayMissShader(ctx, "raymiss", dp::ShaderStage::RayMiss);
    dp::ShaderModule closestHitShader(ctx, "closestHit", dp::ShaderStage::ClosestHit);
    rayGenShader.createShader("shaders/raygen.rgen");
    rayMissShader.createShader("shaders/miss.rmiss");
    closestHitShader.createShader("shaders/closesthit.rchit");

    auto builder = dp::RayTracingPipelineBuilder::create(ctx, "rt_pipeline")
        .addShader(rayGenShader)
        .addShader(rayMissShader)
        .addShader(closestHitShader);

    VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo = {};
    descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    descriptorAccelerationStructureInfo.pAccelerationStructures = &topLevelAccelerationStructure.handle;
    builder.addAccelerationStructureDescriptor(
        0, &descriptorAccelerationStructureInfo,
        VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR
    );

    VkDescriptorImageInfo storageImageDescriptor = storageImage.getDescriptorImageInfo();
    builder.addImageDescriptor(
        1, &storageImageDescriptor,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR
    );

    VkDescriptorBufferInfo cameraBufferInfo = camera.getDescriptorInfo();
    builder.addBufferDescriptor(
        2, &cameraBufferInfo,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR
    );

    modelLoader.createMaterialBuffer();
    VkDescriptorBufferInfo materialBufferInfo = modelLoader.materialBuffer.getDescriptorInfo(VK_WHOLE_SIZE);
    builder.addBufferDescriptor(
        3, &materialBufferInfo,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
    );

    modelLoader.createDescriptionsBuffer(topLevelAccelerationStructure);
    VkDescriptorBufferInfo descriptionsBufferInfo = modelLoader.descriptionsBuffer.getDescriptorInfo(VK_WHOLE_SIZE);
    builder.addBufferDescriptor(
        4, &descriptionsBufferInfo,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
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

    while (!ctx.window->shouldClose()) {
        // Handle SDL events and wait for when we can render the next frame.
        ctx.window->handleEvents(*this);
        if (!needsResize) {
            result = ctx.waitForFrame(swapchain);
            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                needsResize = true;
            } else {
                checkResult(ctx, result, "Failed to present queue");
            }
        }
        if (needsResize) break;

        // Update the camera buffer.
        camera.updateBuffer();

        ctx.beginCommandBuffer(ctx.drawCommandBuffer);
        auto image = swapchain.images[ctx.currentImageIndex];
        ctx.setCheckpoint(ctx.drawCommandBuffer, "Beginning.");

        vkCmdBindPipeline(ctx.drawCommandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.pipeline);
        vkCmdBindDescriptorSets(ctx.drawCommandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.pipelineLayout, 0, 1, &pipeline.descriptorSet, 0, nullptr);

        ctx.setCheckpoint(ctx.drawCommandBuffer, "Tracing rays.");
        ctx.traceRays(
            ctx.drawCommandBuffer,
            raygenShaderBindingTable,
            missShaderBindingTable,
            hitShaderBindingTable,
            sbtStride,
            storageImage.getImageSize3d()
        );

        ctx.setCheckpoint(ctx.drawCommandBuffer, "Changing image layout.");
        // Move storage image to swapchain image.
        storageImage.changeLayout(ctx.drawCommandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        dp::Image::changeLayout(image, ctx.drawCommandBuffer,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                0, VK_ACCESS_TRANSFER_WRITE_BIT,
                                defaultSubresourceRange);

        ctx.setCheckpoint(ctx.drawCommandBuffer, "Copying storage image.");
        ctx.copyStorageImage(ctx.drawCommandBuffer, storageImage.getImageSize(), storageImage, image);

        storageImage.changeLayout(ctx.drawCommandBuffer, VK_IMAGE_LAYOUT_GENERAL);
        dp::Image::changeLayout(image, ctx.drawCommandBuffer,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                VK_ACCESS_TRANSFER_WRITE_BIT, 0,
                                defaultSubresourceRange);

        ctx.setCheckpoint(ctx.drawCommandBuffer, "Drawing UI.");
        // Draw the UI.
        ui.prepare();
        ui.draw(ctx.drawCommandBuffer);

        // End the command buffer and submit.
        ctx.setCheckpoint(ctx.drawCommandBuffer, "Ending.");
        vkEndCommandBuffer(ctx.drawCommandBuffer);

        auto guard = std::move(ctx.graphicsQueue.getLock());
        result = ctx.submitFrame(swapchain);
        guard.unlock();
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            needsResize = true;
        } else {
            checkResult(ctx, result, "Failed to submit queue");
        }
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
