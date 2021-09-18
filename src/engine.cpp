#include "engine.hpp"

dp::Engine::Engine(dp::Context& context)
        : ctx(context), modelLoader(ctx), swapchain(ctx, ctx.surface),
          camera(ctx), ui(ctx, swapchain), storageImage(ctx, { ctx.width, ctx.height }), topLevelAccelerationStructure(ctx),
          raygenShaderBindingTable(ctx, "raygenShaderBindingTable"), missShaderBindingTable(ctx, "missShaderBindingTable"), hitShaderBindingTable(ctx, "hitShaderBindingTable") {
    getProperties();
    
    camera.setPerspective(70.0f, 0.01f, 512.0f);
    camera.setRotation(glm::vec3(0.0f));
    camera.setPosition(glm::vec3(0.0f, 0.0f, -1.0f));

    ui.init();
    
    VkCommandBuffer commandBuffer = ctx.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, ctx.commandPool, true);
    storageImage.changeLayout(commandBuffer, VK_IMAGE_LAYOUT_GENERAL);
    ctx.flushCommandBuffer(commandBuffer, ctx.graphicsQueue);

    // Load the models and create the pipeline.
    modelLoader.loadFile("models/pflanzenzelle.fbx");
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
    descriptorAccelerationStructureInfo.pAccelerationStructures = &topLevelAccelerationStructure.handle;
    builder.addAccelerationStructureDescriptor(
        0, &descriptorAccelerationStructureInfo,
        VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR
    );

    VkDescriptorImageInfo storageImageDescriptor = {
        .imageView = storageImage.image.imageView,
        .imageLayout = storageImage.getCurrentLayout()
    };
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
    uint32_t imageIndex = 0;
    while (!ctx.window->shouldClose()) {
        // Handle SDL events and wait for when we can render the next frame.
        ctx.window->handleEvents(camera);
        ctx.waitForFrame(swapchain);

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
        ctx.submitFrame(swapchain);
        vkQueueWaitIdle(ctx.graphicsQueue);
    }
}
