#include "acceleration_structure_builder.hpp"

#include <chrono>
#include <iostream>

#include "../context.hpp"
#include "acceleration_structure.hpp"

dp::AccelerationStructureBuilder::AccelerationStructureBuilder(const dp::Context& context, const VkCommandPool commandPool)
        : context(context), commandPool(commandPool) {
    
}

void dp::AccelerationStructureBuilder::createBuildBuffers(dp::Buffer& scratchBuffer, dp::Buffer& resultBuffer, const VkAccelerationStructureBuildSizesInfoKHR sizeInfo) {
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

    VkCommandBuffer cmdBuffer;

    // Build the BLAS.
    std::vector<dp::BottomLevelAccelerationStructure> blases = {};
    for (const auto& mesh : meshes) {
        cmdBuffer = context.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, commandPool, true);
        dp::BottomLevelAccelerationStructure blas(context, mesh, mesh.name);

        blas.createMeshBuffers();

        VkAccelerationStructureGeometryKHR structureGeometry = {};
        structureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        structureGeometry.pNext = nullptr;
        structureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        structureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        structureGeometry.geometry.triangles.vertexFormat = mesh.vertexFormat;
        structureGeometry.geometry.triangles.vertexData.deviceAddress = blas.vertexBuffer.address;
        structureGeometry.geometry.triangles.maxVertex = mesh.vertices.size();
        structureGeometry.geometry.triangles.vertexStride = dp::Mesh::vertexStride;
        structureGeometry.geometry.triangles.indexType = mesh.indexType;
        structureGeometry.geometry.triangles.indexData.deviceAddress = blas.indexBuffer.address;
        structureGeometry.geometry.triangles.transformData.hostAddress = nullptr;
        structureGeometry.geometry.triangles.transformData.deviceAddress = blas.transformBuffer.address;

        // Get the sizes.
        VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .geometryCount = 1,
            .pGeometries = &structureGeometry
        };

        const uint32_t numTriangles = mesh.indices.size() / 3;
        VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo = context.getAccelerationStructureBuildSizes(numTriangles, buildGeometryInfo);

        // Create result and scratch buffers.
        dp::Buffer scratchBuffer(context, mesh.name + " scratchBuffer");
        createBuildBuffers(scratchBuffer, blas.resultBuffer, buildSizeInfo);

        VkAccelerationStructureCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = blas.resultBuffer.getHandle();
        createInfo.size = buildSizeInfo.accelerationStructureSize;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        context.createAccelerationStructure(createInfo, &blas.handle);

        buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildGeometryInfo.dstAccelerationStructure = blas.handle;
        buildGeometryInfo.scratchData.deviceAddress = scratchBuffer.address;

        VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo = {};
        accelerationStructureBuildRangeInfo.primitiveCount = numTriangles;
        accelerationStructureBuildRangeInfo.primitiveOffset = 0;
        accelerationStructureBuildRangeInfo.firstVertex = 0;
        accelerationStructureBuildRangeInfo.transformOffset = 0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos = { &accelerationStructureBuildRangeInfo };

        context.buildAccelerationStructures(cmdBuffer, { buildGeometryInfo }, buildRangeInfos);
        context.flushCommandBuffer(cmdBuffer, context.graphicsQueue);

        // Get AS device address
        blas.address = context.getAccelerationStructureDeviceAddress(blas.handle);

        blas.setName();

        blases.push_back(blas);
        structures.push_back(blas);

        scratchBuffer.destroy();
    }

    dp::TopLevelAccelerationStructure tlas(context);
    {
        cmdBuffer = context.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, commandPool, true);

        std::vector<VkAccelerationStructureInstanceKHR> asInstances = {};

        for (const auto& instance : this->instances) {
            VkAccelerationStructureInstanceKHR accelerationStructureInstance = {};
            accelerationStructureInstance.transform = instance.transformMatrix;
            accelerationStructureInstance.instanceCustomIndex = instance.blasIndex;
            accelerationStructureInstance.mask = 0xFF;
            accelerationStructureInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            accelerationStructureInstance.instanceShaderBindingTableRecordOffset = 0;
            accelerationStructureInstance.accelerationStructureReference = blases[instance.blasIndex].address;

            asInstances.push_back(accelerationStructureInstance);
        }

        uint32_t primitiveCount = asInstances.size();
        dp::Buffer instanceBuffer(context, "tlasInstanceBuffer");
        VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress = {};

        // Only copy instance data if there is any.
        if (primitiveCount > 0) {
            instanceBuffer.create(
                sizeof(VkAccelerationStructureInstanceKHR) * primitiveCount,
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                VMA_MEMORY_USAGE_CPU_TO_GPU,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            instanceBuffer.memoryCopy(asInstances.data(), sizeof(VkAccelerationStructureInstanceKHR) * primitiveCount);

            instanceDataDeviceAddress.deviceAddress = instanceBuffer.address;
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

        VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo = context.getAccelerationStructureBuildSizes(primitiveCount, accelerationBuildGeometryInfo);

        // Create result and scratch buffers.
        dp::Buffer scratchBuffer(context, "tlasScratchBuffer");
        createBuildBuffers(scratchBuffer, tlas.resultBuffer, buildSizeInfo);

        VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo = {};
        accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accelerationStructureCreateInfo.buffer = tlas.resultBuffer.getHandle();
        accelerationStructureCreateInfo.size = buildSizeInfo.accelerationStructureSize;
        accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        context.createAccelerationStructure(accelerationStructureCreateInfo, &tlas.handle);

        accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        accelerationBuildGeometryInfo.dstAccelerationStructure = tlas.handle;
        accelerationBuildGeometryInfo.scratchData = {
            .deviceAddress = scratchBuffer.address,
        };

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
