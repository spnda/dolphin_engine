#include "acceleration_structure_builder.hpp"

#include <chrono>
#include <iostream>

#include "../context.hpp"
#include "acceleration_structure.hpp"

dp::AccelerationStructureBuilder::AccelerationStructureBuilder(const dp::Context& context, const VkCommandPool commandPool)
        : ctx(context), commandPool(commandPool) {
    
}

void dp::AccelerationStructureBuilder::createBuildBuffers(dp::Buffer& scratchBuffer, dp::Buffer& resultBuffer, const VkAccelerationStructureBuildSizesInfoKHR sizeInfo) const {
    resultBuffer.create(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    scratchBuffer.create(
        dp::Buffer::alignedSize(sizeInfo.buildScratchSize, asProperties.minAccelerationStructureScratchOffsetAlignment),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
}

dp::AccelerationStructureBuilder dp::AccelerationStructureBuilder::create(const dp::Context& context, const VkCommandPool commandPool) {
    dp::AccelerationStructureBuilder builder(context, commandPool);
    return builder;
}

void dp::AccelerationStructureBuilder::destroyAllStructures(const dp::Context& ctx) {
    for (auto& structure : structures) {
        ctx.destroyAccelerationStructure(structure.handle);
        structure.resultBuffer.destroy();
    }
}

uint32_t dp::AccelerationStructureBuilder::addMesh(const dp::Mesh& mesh) {
    this->meshes.emplace_back(mesh);
    return this->meshes.size() - 1;
}

void dp::AccelerationStructureBuilder::addInstance(dp::AccelerationStructureInstance instance) {
    instances.push_back(instance);
}

dp::TopLevelAccelerationStructure dp::AccelerationStructureBuilder::build() {
    std::chrono::steady_clock::time_point beginBuild = std::chrono::steady_clock::now();

    VkPhysicalDeviceProperties2 deviceProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &this->asProperties,
    };
    vkGetPhysicalDeviceProperties2(ctx.physicalDevice, &deviceProperties);

    // Build the BLAS.
    std::vector<dp::BottomLevelAccelerationStructure> blases = {}; // Use sized array in the future.
    for (const auto& mesh : meshes) {
        dp::BottomLevelAccelerationStructure blas(ctx, mesh, mesh.name);

        blas.createMeshBuffers();

        VkAccelerationStructureGeometryKHR structureGeometry = {};
        structureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        structureGeometry.pNext = nullptr;
        structureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        structureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        structureGeometry.geometry.triangles.vertexFormat = mesh.vertexFormat;
        structureGeometry.geometry.triangles.vertexData = blas.vertexBuffer.getHostAddressConst();
        structureGeometry.geometry.triangles.maxVertex = mesh.vertices.size();
        structureGeometry.geometry.triangles.vertexStride = dp::Mesh::vertexStride;
        structureGeometry.geometry.triangles.indexType = mesh.indexType;
        structureGeometry.geometry.triangles.indexData = blas.indexBuffer.getHostAddressConst();
        structureGeometry.geometry.triangles.transformData.hostAddress = nullptr;
        structureGeometry.geometry.triangles.transformData = blas.transformBuffer.getHostAddressConst();

        // Get the sizes.
        VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .geometryCount = 1,
            .pGeometries = &structureGeometry
        };

        const uint64_t numTriangles = std::min(mesh.indices.size() / 3, asProperties.maxPrimitiveCount);
        VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo = ctx.getAccelerationStructureBuildSizes(numTriangles, buildGeometryInfo);

        // Create result and scratch buffers.
        dp::Buffer scratchBuffer(ctx, mesh.name + " scratchBuffer");
        createBuildBuffers(scratchBuffer, blas.resultBuffer, buildSizeInfo);

        VkAccelerationStructureCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = blas.resultBuffer.getHandle();
        createInfo.size = buildSizeInfo.accelerationStructureSize;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        ctx.createAccelerationStructure(createInfo, &blas.handle);

        buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildGeometryInfo.dstAccelerationStructure = blas.handle;
        buildGeometryInfo.scratchData = scratchBuffer.getHostAddress();

        VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo = {};
        accelerationStructureBuildRangeInfo.primitiveCount = numTriangles;
        accelerationStructureBuildRangeInfo.primitiveOffset = 0;
        accelerationStructureBuildRangeInfo.firstVertex = 0;
        accelerationStructureBuildRangeInfo.transformOffset = 0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos = { &accelerationStructureBuildRangeInfo };

        // Submit the buffer copy and build commands.
        ctx.oneTimeSubmit(ctx.graphicsQueue, commandPool, [&](VkCommandBuffer cmdBuffer) {
            ctx.setCheckpoint(cmdBuffer, "Copying mesh buffers!");
            blas.copyMeshBuffers(cmdBuffer);
            VkMemoryBarrier memoryBarrier = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            };
            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0,
                                 1, &memoryBarrier,
                                 0, nullptr,
                                 0, nullptr);
            ctx.setCheckpoint(cmdBuffer, "Building BLAS!");
            ctx.buildAccelerationStructures(cmdBuffer, 1, &buildGeometryInfo, buildRangeInfos);
        });

        // Get AS device address
        blas.address = ctx.getAccelerationStructureDeviceAddress(blas.handle);
        blas.setName();
        blas.destroyMeshStagingBuffers();

        blases.push_back(blas);
        structures.push_back(blas);

        scratchBuffer.destroy();
    }

    dp::TopLevelAccelerationStructure tlas(ctx);
    {
        std::vector<VkAccelerationStructureInstanceKHR> asInstances(std::min(instances.size(), asProperties.maxInstanceCount));

        for (const auto& instance : instances) {
            size_t index = &instance - &instances[0];
            asInstances[index].transform = instance.transformMatrix;
            asInstances[index].instanceCustomIndex = instance.blasIndex;
            asInstances[index].mask = 0xFF;
            asInstances[index].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            asInstances[index].instanceShaderBindingTableRecordOffset = 0;
            asInstances[index].accelerationStructureReference = blases[instance.blasIndex].address;
        }

        uint32_t primitiveCount = asInstances.size();
        dp::StagingBuffer instanceStagingBuffer(ctx, "tlasInstanceStagingBuffer");
        dp::Buffer instanceBuffer(ctx, "tlasInstanceBuffer");
        VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress = { };

        // Only copy instance data if there is any.
        if (primitiveCount > 0) {
            size_t instanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR) * primitiveCount;
            instanceStagingBuffer.create(instanceBufferSize);
            instanceStagingBuffer.memoryCopy(asInstances.data(), instanceBufferSize);

            // Copy instance staging buffer into real instance buffer.
            instanceBuffer.create(
                instanceBufferSize,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                VMA_MEMORY_USAGE_GPU_ONLY,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            instanceDataDeviceAddress = instanceBuffer.getHostAddressConst();
        }

        VkAccelerationStructureGeometryKHR accelerationStructureGeometry = {};
        accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
        accelerationStructureGeometry.geometry.instances.data = instanceDataDeviceAddress;

        // Get size info
        VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .geometryCount = 1, // This has to be at least 1 for a TLAS.
            .pGeometries = &accelerationStructureGeometry,
        };

        VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo = ctx.getAccelerationStructureBuildSizes(primitiveCount, accelerationBuildGeometryInfo);

        // Create result and scratch buffers.
        dp::Buffer scratchBuffer(ctx, "tlasScratchBuffer");
        createBuildBuffers(scratchBuffer, tlas.resultBuffer, buildSizeInfo);

        VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo = {};
        accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accelerationStructureCreateInfo.buffer = tlas.resultBuffer.getHandle();
        accelerationStructureCreateInfo.size = buildSizeInfo.accelerationStructureSize;
        accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        ctx.createAccelerationStructure(accelerationStructureCreateInfo, &tlas.handle);

        accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        accelerationBuildGeometryInfo.dstAccelerationStructure = tlas.handle;
        accelerationBuildGeometryInfo.scratchData = scratchBuffer.getHostAddress();

        VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo = {};
        accelerationStructureBuildRangeInfo.primitiveCount = primitiveCount;
        accelerationStructureBuildRangeInfo.primitiveOffset = 0;
        accelerationStructureBuildRangeInfo.firstVertex = 0;
        accelerationStructureBuildRangeInfo.transformOffset = 0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

        ctx.oneTimeSubmit(ctx.graphicsQueue, commandPool, [&](VkCommandBuffer cmdBuffer) {
            instanceStagingBuffer.copyToBuffer(cmdBuffer, instanceBuffer);
            ctx.setCheckpoint(cmdBuffer, "Building TLAS!");
            ctx.buildAccelerationStructures(
                cmdBuffer,
                1, &accelerationBuildGeometryInfo,
                accelerationBuildStructureRangeInfos);
        });
        instanceStagingBuffer.destroy();

        tlas.address = ctx.getAccelerationStructureDeviceAddress(tlas.handle);
        tlas.setName();
        tlas.blases.insert(tlas.blases.end(), blases.begin(), blases.end());
        
        structures.push_back(tlas);

        scratchBuffer.destroy();
        if (primitiveCount > 0) instanceBuffer.destroy();
    }

    std::chrono::steady_clock::time_point endBuild = std::chrono::steady_clock::now();
    std::cout << "Finished building acceleration structures. Took "
        << std::chrono::duration_cast<std::chrono::milliseconds>(endBuild - beginBuild).count()
        << "ms."
        << std::endl;

    return tlas;
}
