#include "engine.hpp"

#include "sdl/window.hpp"
#include "vulkan/utils.hpp"

dp::Engine::Engine(dp::Context& context)
        : ctx(context), modelLoader(ctx), swapchain(ctx, ctx.surface),
          camera(ctx), ui(ctx, swapchain), storageImage(ctx), topLevelAccelerationStructure(ctx),
          shaderBindingTable(ctx, "shaderBindingTable") {
    this->getProperties();

    camera.setPerspective(70.0f, 0.01f, 512.0f);
    camera.setRotation(glm::vec3(0.0f));
    camera.setPosition(glm::vec3(0.0f, 0.0f, -1.0f));

    ui.init();

    storageImage.create();
    ctx.oneTimeSubmit(ctx.graphicsQueue, ctx.commandPool, [&](VkCommandBuffer cmdBuffer) {
       storageImage.changeLayout(
           cmdBuffer, VK_IMAGE_LAYOUT_GENERAL,
           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    });

    // Load the models and create the pipeline.
    modelLoader.loadFile("models/CornellBox/CornellBox-Original.obj");
    topLevelAccelerationStructure = modelLoader.buildAccelerationStructure(ctx);
    this->buildPipeline();

    this->buildSBT();
}

void dp::Engine::getProperties() {
    VkPhysicalDeviceProperties2 deviceProperties = { .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, };
    deviceProperties.pNext = &this->rtProperties;
    vkGetPhysicalDeviceProperties2(ctx.physicalDevice, &deviceProperties);
}

void dp::Engine::buildPipeline() {
    dp::ShaderModule rayGenShader(ctx, "raygen", dp::ShaderStage::RayGeneration);
    dp::ShaderModule rayMissShader(ctx, "raymiss", dp::ShaderStage::RayMiss);
    dp::ShaderModule closestHitShader(ctx, "closestHit", dp::ShaderStage::ClosestHit);
    dp::ShaderModule shadowMissShader(ctx, "shadowMiss", dp::ShaderStage::RayMiss);
    rayGenShader.createShader("shaders/raygen.rgen");
    rayMissShader.createShader("shaders/miss.rmiss");
    closestHitShader.createShader("shaders/closesthit.rchit");
    shadowMissShader.createShader("shaders/shadow.rmiss");

    // These need to be inputted in order.
    auto builder = dp::RayTracingPipelineBuilder::create(ctx, "rt_pipeline")
        .addShader(rayGenShader)
        .addShader(rayMissShader)
        .addShader(shadowMissShader)
        .addShader(closestHitShader);

    builder.addPushConstants(sizeof(PushConstants), dp::ShaderStage::ClosestHit);

    VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo = {};
    descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    descriptorAccelerationStructureInfo.pAccelerationStructures = &topLevelAccelerationStructure.handle;
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

    modelLoader.createMaterialBuffer();
    VkDescriptorBufferInfo materialBufferInfo = modelLoader.materialBuffer.getDescriptorInfo(VK_WHOLE_SIZE);
    builder.addBufferDescriptor(
        3, &materialBufferInfo,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, dp::ShaderStage::ClosestHit
    );

    modelLoader.createDescriptionsBuffer(topLevelAccelerationStructure);
    VkDescriptorBufferInfo descriptionsBufferInfo = modelLoader.descriptionsBuffer.getDescriptorInfo(VK_WHOLE_SIZE);
    builder.addBufferDescriptor(
        4, &descriptionsBufferInfo,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, dp::ShaderStage::ClosestHit
    );

    std::vector<VkDescriptorImageInfo> textureInfos = modelLoader.getTextureDescriptorInfos();
    builder.addImageDescriptor(
        5, textureInfos.data(),
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, dp::ShaderStage::ClosestHit,
        textureInfos.size()
    );

    pipeline = builder.build();
}

void dp::Engine::buildSBT() {
    uint32_t missCount = 2;
    uint32_t chitCount = 1;
    auto handleCount = 1 + missCount + chitCount;
    const uint32_t handleSize = rtProperties.shaderGroupHandleSize;
    sbtStride = dp::Buffer::alignedSize(handleSize, rtProperties.shaderGroupHandleAlignment);

    raygenRegion.stride = dp::Buffer::alignedSize(sbtStride, rtProperties.shaderGroupBaseAlignment);
    raygenRegion.size = raygenRegion.stride; // RayGen size must be equal to the stride.
    missRegion.stride = sbtStride;
    missRegion.size = dp::Buffer::alignedSize(missCount * sbtStride, rtProperties.shaderGroupBaseAlignment);
    chitRegion.stride = sbtStride;
    chitRegion.size = dp::Buffer::alignedSize(chitCount * sbtStride, rtProperties.shaderGroupBaseAlignment);

    const uint32_t sbtSize = raygenRegion.size + missRegion.size + chitRegion.size + callableRegion.size;
    std::vector<uint8_t> handleStorage(sbtSize);
    ctx.getRayTracingShaderGroupHandles(pipeline.pipeline, handleCount, sbtSize, handleStorage);

    shaderBindingTable.create(sbtSize,
                              VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
                              VMA_MEMORY_USAGE_CPU_TO_GPU);
    auto sbtAddress = shaderBindingTable.getDeviceAddress();
    raygenRegion.deviceAddress = sbtAddress;
    missRegion.deviceAddress = sbtAddress + raygenRegion.size;
    chitRegion.deviceAddress = missRegion.deviceAddress + missRegion.size;

    shaderBindingTable.memoryCopy(handleStorage.data(), sbtSize);

    auto getHandleOffset = [&](uint32_t i) {
        return handleStorage.data() + i * handleSize;
    };
    uint32_t curHandleIndex = 0;

    // Write raygen shader
    shaderBindingTable.memoryCopy(getHandleOffset(curHandleIndex++), handleSize, 0);

    // Write miss shaders
    for (uint32_t i = 0; i < missCount; i++) {
        shaderBindingTable.memoryCopy(
            getHandleOffset(curHandleIndex++),
            handleSize,
            raygenRegion.size + missRegion.stride * i);
    }

    // Write chit shaders
    for (uint32_t i = 0; i < chitCount; i++) {
        shaderBindingTable.memoryCopy(
            getHandleOffset(curHandleIndex++),
            handleSize,
            raygenRegion.size + missRegion.size + chitRegion.stride * i);
    }
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

        ctx.beginCommandBuffer(ctx.drawCommandBuffer, 0);
        auto image = swapchain.images[ctx.currentImageIndex];
        ctx.setCheckpoint(ctx.drawCommandBuffer, "Beginning.");

        vkCmdBindPipeline(ctx.drawCommandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.pipeline);
        vkCmdBindDescriptorSets(ctx.drawCommandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline.pipelineLayout, 0, 1, &pipeline.descriptorSet, 0, nullptr);

        vkCmdPushConstants(ctx.drawCommandBuffer, pipeline.pipelineLayout,
                           static_cast<VkShaderStageFlags>(dp::ShaderStage::ClosestHit),
                           0, sizeof(PushConstants), &pushConstants);

        ctx.setCheckpoint(ctx.drawCommandBuffer, "Tracing rays.");
        ctx.traceRays(
            ctx.drawCommandBuffer,
            &raygenRegion,
            &missRegion,
            &chitRegion,
            &callableRegion,
            storageImage.getImageSize3d()
        );

        ctx.setCheckpoint(ctx.drawCommandBuffer, "Changing image layout.");
        // Move storage image to swapchain image.
        storageImage.changeLayout(
            ctx.drawCommandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT);

        dp::Image::changeLayout(image, ctx.drawCommandBuffer,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                defaultSubresourceRange);

        ctx.setCheckpoint(ctx.drawCommandBuffer, "Copying storage image.");
        storageImage.copyImage(ctx.drawCommandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        storageImage.changeLayout(
            ctx.drawCommandBuffer, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        dp::Image::changeLayout(image, ctx.drawCommandBuffer,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                // Fragment shader here, as the next stage will be imgui drawing.
                                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                defaultSubresourceRange);

        ctx.setCheckpoint(ctx.drawCommandBuffer, "Drawing UI.");
        // Draw the UI.
        ui.prepare();
        ui.draw(*this, ctx.drawCommandBuffer);

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
    auto result = ctx.device.waitIdle();
    checkResult(ctx, result, "Failed to wait on device idle");

    ctx.width = width;
    ctx.height = height;

    // Re-create the swapchain.
    swapchain.create(ctx.device);
    
    // Creates a new image and image view.
    storageImage.recreateImage();

    // Change the layout of the new image.
    ctx.oneTimeSubmit(ctx.graphicsQueue, ctx.commandPool, [&](VkCommandBuffer cmdBuffer) {
        storageImage.changeLayout(
            cmdBuffer, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    });

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

dp::Engine::PushConstants& dp::Engine::getConstants() {
    return this->pushConstants;
}
