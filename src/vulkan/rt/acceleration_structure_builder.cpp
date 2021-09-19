#include "acceleration_structure_builder.hpp"

#include <chrono>

#include "../context.hpp"

dp::AccelerationStructure::AccelerationStructure(const dp::Context& context, const std::string asName)
        : ctx(context), name(asName),
          resultBuffer(context, name + " resultBuffer") {
}

dp::AccelerationStructure& dp::AccelerationStructure::operator=(const dp::AccelerationStructure& structure) {
    this->address = structure.address;
    this->handle = structure.handle;
    this->name = structure.name;
    this->resultBuffer = structure.resultBuffer;
    return *this;
}

void dp::AccelerationStructure::setName() {
    ctx.setDebugUtilsName(handle, name);
}

dp::AccelerationStructureBuilder::AccelerationStructureBuilder(const dp::Context& context)
        : context(context) {
    
}

void dp::AccelerationStructureBuilder::createMeshBuffers(dp::Buffer& vertexBuffer, dp::Buffer& indexBuffer, dp::Buffer& transformBuffer, const dp::Mesh& mesh) {
    VkBufferUsageFlags bufferUsage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    VmaMemoryUsage usage = VMA_MEMORY_USAGE_CPU_ONLY;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vertexBuffer.create(mesh.vertices.size() * sizeof(Vertex), bufferUsage, usage, properties);
    vertexBuffer.memoryCopy(mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));

    // Create the index buffer
    indexBuffer.create(mesh.indices.size() * sizeof(uint32_t), bufferUsage, usage, properties);
    indexBuffer.memoryCopy(mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));

    // Create the index buffer
    transformBuffer.create(sizeof(VkTransformMatrixKHR), bufferUsage, usage, properties);
    transformBuffer.memoryCopy(&mesh.transform, sizeof(VkTransformMatrixKHR));
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

void dp::AccelerationStructureBuilder::destroyAllStructures(const dp::Context& ctx) {
    for (auto& structure : structures) {
        ctx.destroyAccelerationStructure(structure.handle);
        structure.resultBuffer.destroy();
    }
}

uint32_t dp::AccelerationStructureBuilder::addMesh(dp::Mesh mesh) {
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
        VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo = context.getAccelerationStructureBuildSizes(numTriangles, buildGeometryInfo);

        // Create result and scratch buffers.
        dp::AccelerationStructure blas(context, mesh.name);
        dp::Buffer scratchBuffer(context, mesh.name + " scratchBuffer");
        createBuildBuffers(scratchBuffer, blas.resultBuffer, buildSizeInfo);

        VkAccelerationStructureCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = blas.resultBuffer.handle;
        createInfo.size = buildSizeInfo.accelerationStructureSize;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        context.createAccelerationStructure(createInfo, &blas.handle);

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

        context.buildAccelerationStructures(cmdBuffer, { buildScratchGeometryInfo }, buildRangeInfos);
        context.flushCommandBuffer(cmdBuffer, context.graphicsQueue);

        // Get AS device address
        blas.address = context.getAccelerationStructureDeviceAddress(blas.handle);

        blas.setName();

        blases.push_back(blas);
        structures.push_back(blas);

        vertexBuffer.destroy();
        indexBuffer.destroy();
        transformBuffer.destroy();
        scratchBuffer.destroy();
    }

    // Flush command buffer to ensure all BLASs are built before building the TLAS.
    cmdBuffer = context.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, context.commandPool, true);

    dp::AccelerationStructure tlas(context);
    {
        std::vector<VkAccelerationStructureInstanceKHR> asInstances = {};

        for (const auto& instance : this->instances) {
            VkAccelerationStructureInstanceKHR accelerationStructureInstance = {};
            accelerationStructureInstance.transform = instance.transformMatrix;
            accelerationStructureInstance.instanceCustomIndex = this->meshes[instance.blasIndex].materialIndex;
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

        VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo = context.getAccelerationStructureBuildSizes(primitiveCount, accelerationStructureBuildGeometryInfo);
        
        // Create result and scratch buffers.
        dp::Buffer scratchBuffer(context, "tlasScratchBuffer");
        createBuildBuffers(scratchBuffer, tlas.resultBuffer, buildSizeInfo);

        VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo = {};
        accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accelerationStructureCreateInfo.buffer = tlas.resultBuffer.handle;
        accelerationStructureCreateInfo.size = buildSizeInfo.accelerationStructureSize;
        accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        context.createAccelerationStructure(accelerationStructureCreateInfo, &tlas.handle);

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

        context.buildAccelerationStructures(
            cmdBuffer,
            { accelerationBuildGeometryInfo },
            accelerationBuildStructureRangeInfos);
        context.flushCommandBuffer(cmdBuffer, context.graphicsQueue);

        tlas.address = context.getAccelerationStructureDeviceAddress(tlas.handle);

        context.setDebugUtilsName(tlas.handle, "TLAS");
        
        structures.push_back(tlas);

        scratchBuffer.destroy();
        instanceBuffer.destroy();
    }

    std::chrono::steady_clock::time_point endBuild = std::chrono::steady_clock::now();
    printf("Finished building acceleration structures. Took %zu[ms].\n", std::chrono::duration_cast<std::chrono::milliseconds>(endBuild - beginBuild).count());

    return tlas;
}
