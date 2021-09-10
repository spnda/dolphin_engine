#include "acceleration_structure_builder.hpp"

#include <chrono>

dp::AccelerationStructureBuilder::AccelerationStructureBuilder(const dp::Context& context)
        : context(context) {
    
}

void dp::AccelerationStructureBuilder::createMeshBuffers(dp::Buffer& vertexBuffer, dp::Buffer& indexBuffer, dp::Buffer& transformBuffer, const dp::AccelerationStructureMesh& mesh) {
    vertexBuffer.create(
        mesh.vertices.size() * sizeof(Vertex),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_CPU_ONLY,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vertexBuffer.memoryCopy(mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));

    // Create the index buffer
    indexBuffer.create(
        mesh.indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_CPU_ONLY,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    indexBuffer.memoryCopy(mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));

    // Create the index buffer
    transformBuffer.create(
        sizeof(VkTransformMatrixKHR),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_CPU_ONLY,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    transformBuffer.memoryCopy(&mesh.transformMatrix, sizeof(VkTransformMatrixKHR));
}

void dp::AccelerationStructureBuilder::createBuildBuffers(dp::Buffer& scratchBuffer, dp::Buffer& resultBuffer, const VkAccelerationStructureBuildSizesInfoKHR sizeInfo) const {
    resultBuffer.create(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
        VMA_MEMORY_USAGE_UNKNOWN,
        VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR
    );
    scratchBuffer.create(
        sizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR
    );
}

dp::AccelerationStructureBuilder dp::AccelerationStructureBuilder::create(const dp::Context& context) {
    dp::AccelerationStructureBuilder builder(context);
    return builder;
}

uint32_t dp::AccelerationStructureBuilder::addMesh(dp::AccelerationStructureMesh mesh) {
    this->meshes.emplace_back(mesh);
    return this->meshes.size() - 1;
}

void dp::AccelerationStructureBuilder::addInstance(dp::AccelerationStructureInstance instance) {
    instances.push_back(instance);
}

dp::AccelerationStructure dp::AccelerationStructureBuilder::build() {
    printf("Building acceleration structures!\n");
    std::chrono::steady_clock::time_point beginBuild = std::chrono::steady_clock::now();

    VkCommandBuffer cmdBuffer;

    // Build the BLAS.
    std::vector<AccelerationStructure> blases = {};
    for (const auto& mesh : meshes) {
        cmdBuffer = context.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, context.commandPool, true);

        dp::Buffer vertexBuffer(context, "blasVertexBuffer");
        dp::Buffer indexBuffer(context, "blasIndexBuffer");
        dp::Buffer transformBuffer(context, "blasTransformBuffer");
        this->createMeshBuffers(vertexBuffer, indexBuffer, transformBuffer, mesh);

        VkAccelerationStructureGeometryKHR structureGeometry = {};
        structureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        structureGeometry.pNext = nullptr;
        structureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        structureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        structureGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        structureGeometry.geometry.triangles.vertexData.deviceAddress = vertexBuffer.address;
        structureGeometry.geometry.triangles.maxVertex = mesh.vertices.size();
        structureGeometry.geometry.triangles.vertexStride = sizeof(Vertex);
        structureGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
        structureGeometry.geometry.triangles.indexData.deviceAddress = indexBuffer.address;
        structureGeometry.geometry.triangles.transformData.hostAddress = nullptr;
        structureGeometry.geometry.triangles.transformData.deviceAddress = transformBuffer.address;

        // Get the sizes.
        VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = {};
        buildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildGeometryInfo.geometryCount = 1;
        buildGeometryInfo.pGeometries = &structureGeometry;

        const uint32_t numTriangles = mesh.vertices.size() / 3; // TODO!
        VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo = {};
        buildSizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(context.device.device, "vkGetAccelerationStructureBuildSizesKHR"))
            (context.device.device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildGeometryInfo,
            &numTriangles,
            &buildSizeInfo);

        // Create result and scratch buffers.
        dp::Buffer resultBuffer(context, "blasResultBuffer");
        dp::Buffer scratchBuffer(context, "blasScratchBuffer");
        createBuildBuffers(scratchBuffer, resultBuffer, buildSizeInfo);
        dp::AccelerationStructure blas;

        VkAccelerationStructureCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = resultBuffer.handle;
        createInfo.size = buildSizeInfo.accelerationStructureSize;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(context.device.device, "vkCreateAccelerationStructureKHR"))
            (context.device.device, &createInfo, nullptr, &blas.handle);

        VkAccelerationStructureBuildGeometryInfoKHR buildScratchGeometryInfo = {};
        buildScratchGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildScratchGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildScratchGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildScratchGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildScratchGeometryInfo.dstAccelerationStructure = blas.handle;
        buildScratchGeometryInfo.geometryCount = 1;
        buildScratchGeometryInfo.pGeometries = &structureGeometry;
        buildScratchGeometryInfo.scratchData.deviceAddress = scratchBuffer.address;

        VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo = {};
        accelerationStructureBuildRangeInfo.primitiveCount = numTriangles;
        accelerationStructureBuildRangeInfo.primitiveOffset = 0;
        accelerationStructureBuildRangeInfo.firstVertex = 0;
        accelerationStructureBuildRangeInfo.transformOffset = 0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos = { &accelerationStructureBuildRangeInfo };

        reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(context.device.device, "vkCmdBuildAccelerationStructuresKHR"))(
            cmdBuffer,
            1,
            &buildScratchGeometryInfo,
            buildRangeInfos.data()
        );
        context.flushCommandBuffer(cmdBuffer, context.graphicsQueue);

        // Get AS device address
        VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
        addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addressInfo.accelerationStructure = blas.handle;
        blas.address =
            reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(context.device.device, "vkGetAccelerationStructureDeviceAddressKHR"))
                (context.device.device, &addressInfo);

        context.setDebugUtilsName(blas.handle, "BLAS");
        
        blases.push_back(blas);
    }

    // Flush command buffer to ensure all BLASs are built before building the TLAS.
    cmdBuffer = context.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, context.commandPool, true);

    dp::AccelerationStructure tlas = {};
    {
        std::vector<VkAccelerationStructureInstanceKHR> asInstances = {};

        for (const auto& instance : this->instances) {
            VkAccelerationStructureInstanceKHR accelerationStructureInstance = {};
            accelerationStructureInstance.transform = instance.transformMatrix;
            accelerationStructureInstance.instanceCustomIndex = 0;
            accelerationStructureInstance.mask = 0xFF;
            accelerationStructureInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            accelerationStructureInstance.instanceShaderBindingTableRecordOffset = 0;
            accelerationStructureInstance.accelerationStructureReference = blases[instance.blasIndex].address;

            asInstances.push_back(accelerationStructureInstance);
        }

        uint32_t primitiveCount = asInstances.size();
        dp::Buffer instanceBuffer(context, "tlasInstanceBuffer");
        instanceBuffer.create(
            sizeof(VkAccelerationStructureInstanceKHR) * primitiveCount,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VMA_MEMORY_USAGE_CPU_ONLY,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        instanceBuffer.memoryCopy(asInstances.data(), sizeof(VkAccelerationStructureInstanceKHR) * primitiveCount);

        VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress = {};
        instanceDataDeviceAddress.deviceAddress = instanceBuffer.address;

        VkAccelerationStructureGeometryKHR accelerationStructureGeometry = {};
        accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
        accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

        // Get size info
        VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo = {};
        accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        accelerationStructureBuildGeometryInfo.geometryCount = 1;
        accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

        VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo = {};
        buildSizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(context.device.device, "vkGetAccelerationStructureBuildSizesKHR"))
            (context.device.device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &accelerationStructureBuildGeometryInfo,
            &primitiveCount,
            &buildSizeInfo);
        
        // Create result and scratch buffers.
        dp::Buffer resultBuffer(context, "tlasResultBuffer");
        dp::Buffer scratchBuffer(context, "tlasScratchBuffer");
        createBuildBuffers(scratchBuffer, resultBuffer, buildSizeInfo);

        VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo = {};
        accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accelerationStructureCreateInfo.buffer = resultBuffer.handle;
        accelerationStructureCreateInfo.size = buildSizeInfo.accelerationStructureSize;
        accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(context.device.device, "vkCreateAccelerationStructureKHR"))
            (context.device.device, &accelerationStructureCreateInfo, nullptr, &tlas.handle);

        VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo = {};
        accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        accelerationBuildGeometryInfo.dstAccelerationStructure = tlas.handle;
        accelerationBuildGeometryInfo.geometryCount = 1;
        accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
        accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.address;

        VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo = {};
        accelerationStructureBuildRangeInfo.primitiveCount = primitiveCount;
        accelerationStructureBuildRangeInfo.primitiveOffset = 0;
        accelerationStructureBuildRangeInfo.firstVertex = 0;
        accelerationStructureBuildRangeInfo.transformOffset = 0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

        reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(context.device.device, "vkCmdBuildAccelerationStructuresKHR"))(
            cmdBuffer,
            1,
            &accelerationBuildGeometryInfo,
            accelerationBuildStructureRangeInfos.data());
        context.flushCommandBuffer(cmdBuffer, context.graphicsQueue);

        VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo = {};
        accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        accelerationDeviceAddressInfo.accelerationStructure = tlas.handle;
        tlas.address = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(context.device.device, "vkGetAccelerationStructureDeviceAddressKHR"))
                (context.device.device, &accelerationDeviceAddressInfo);

        context.setDebugUtilsName(tlas.handle, "TLAS");
    }

    std::chrono::steady_clock::time_point endBuild = std::chrono::steady_clock::now();
    printf("Finished building acceleration structures. Took %zu[ms].\n", std::chrono::duration_cast<std::chrono::milliseconds>(endBuild - beginBuild).count());

    return tlas;
}

/*dp::TopLevelAccelerationStructure dp::AccelerationStructureBuilder::build() {
    auto cmdBuffer = context.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, context.commandPool, true);

    // BLAS.
    auto mesh = meshes[0];
    dp::Buffer vertexBuffer(context);
    dp::Buffer indexBuffer(context);
    dp::Buffer transformBuffer(context);
    this->createMeshBuffers(vertexBuffer, indexBuffer, transformBuffer, mesh);

    BottomLevelGeometry geometry;
    geometry.addGeometryTriangles(vertexBuffer, mesh.vertices.size(), indexBuffer, mesh.indices.size(), false);

    // Note to future self: We can't use the ret value of emplace_back!
    dp::BottomLevelAccelerationStructure blas(context, geometry);
    dp::Buffer resultBuffer(context);
    dp::Buffer scratchBuffer(context);
    createBuildBuffers(scratchBuffer, resultBuffer, blas.getBuildSizes());
    context.setDebugUtilsName(scratchBuffer.handle, "blasScratchBuffer");
    context.setDebugUtilsName(resultBuffer.handle, "blasResultBuffer");
        
    printf("Generating BLAS.\n");
    blas.generate(cmdBuffer, scratchBuffer, resultBuffer, 0);

    context.flushCommandBuffer(cmdBuffer, context.graphicsQueue);

    cmdBuffer = context.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, context.commandPool, true);

    // TLAS.
    auto asInstance = TopLevelAccelerationStructure::createInstance(context, blas, instance.transformMatrix, 0, 0);
    
    dp::Buffer instanceBuffer(context);
    instanceBuffer.create(
        sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_CPU_ONLY,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    context.setDebugUtilsName(instanceBuffer.handle, "instanceBuffer");
    instanceBuffer.memoryCopy(&asInstance, sizeof(VkAccelerationStructureInstanceKHR));

    // We need a memory barrier, as we have to properly wait for the BLASs
    // to have been built.
    dp::AccelerationStructure::memoryBarrier(cmdBuffer);

    dp::TopLevelAccelerationStructure tlas(context, instanceBuffer.address, 1);

    dp::Buffer tlasResultBuffer(context);
    dp::Buffer tlasScratchBuffer(context);
    const auto total = tlas.getBuildSizes();
    createBuildBuffers(tlasScratchBuffer, tlasResultBuffer, total);
    context.setDebugUtilsName(tlasScratchBuffer.handle, "tlasScratchBuffer");
    context.setDebugUtilsName(tlasResultBuffer.handle, "tlasResultBuffer");

    printf("Generating TLAS.\n");
    tlas.generate(cmdBuffer, tlasScratchBuffer, tlasResultBuffer, 0);
    context.setDebugUtilsName(tlas.getAccelerationStructure(), "TLAS");

    context.flushCommandBuffer(cmdBuffer, context.graphicsQueue);

    return tlas;
}*/

/*dp::TopLevelAccelerationStructure dp::AccelerationStructureBuilder::build() {
    VkCommandBuffer commandBuffer = context.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, context.commandPool, true);

    // TODO: We should use a single result buffer and scratch buffer for all the BLASs.
    // We would just have to implement the offsets for the blas#generate function call.
    for (const auto& mesh : meshes) {
        static const uint32_t bufferSize = 3;
        dp::Buffer vertexBuffer(context);
        dp::Buffer indexBuffer(context);
        dp::Buffer transformBuffer(context);
        this->createMeshBuffers(vertexBuffer, indexBuffer, transformBuffer, mesh);

        BottomLevelGeometry geometry;
        geometry.addGeometryTriangles(vertexBuffer, mesh.vertices.size(), indexBuffer, mesh.indices.size(), false);

        // Note to future self: We can't use the ret value of emplace_back!
        bottomAccelerationStructures.emplace_back(context, geometry); // Implicit ctor call
        dp::Buffer resultBuffer(context);
        dp::Buffer scratchBuffer(context);
        createBuildBuffers(scratchBuffer, resultBuffer, bottomAccelerationStructures[0].getBuildSizes());
        
        printf("Generating BLAS.\n");
        bottomAccelerationStructures[0].generate(commandBuffer, scratchBuffer, resultBuffer, 0);
    }

    // We only have one TLAS for now.
    // TODO: Make it work with more than one BLAS.
    auto asInstance = TopLevelAccelerationStructure::createInstance(context, bottomAccelerationStructures[0], instance.transformMatrix, 0, 0);
    
    dp::Buffer instanceBuffer(context);
    instanceBuffer.create(
        sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_CPU_ONLY,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    // Note to future self: We can't use the ret value of emplace_back!
    topAccelerationStructures.emplace_back(context, instanceBuffer.address, 1);

    dp::Buffer resultBuffer(context);
    dp::Buffer scratchBuffer(context);
    const auto total = topAccelerationStructures[0].getBuildSizes();
    createBuildBuffers(scratchBuffer, resultBuffer, total);

    printf("Generating TLAS.\n");
    topAccelerationStructures[0].generate(commandBuffer, scratchBuffer, resultBuffer, 0);

    context.flushCommandBuffer(commandBuffer, context.graphicsQueue);

    return topAccelerationStructures[0];
}*/
