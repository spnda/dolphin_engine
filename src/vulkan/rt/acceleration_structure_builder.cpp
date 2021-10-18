#include "acceleration_structure_builder.hpp"

#include <chrono>
#include <fmt/core.h>

#include "../context.hpp"
#include "acceleration_structure.hpp"

dp::AccelerationStructureBuilder::AccelerationStructureBuilder(const dp::Context& context, const VkCommandPool commandPool)
        : ctx(context), commandPool(commandPool) {
    
}

void dp::AccelerationStructureBuilder::createBuildBuffers(dp::Buffer& scratchBuffer, dp::Buffer& resultBuffer, const VkAccelerationStructureBuildSizesInfoKHR sizeInfo) const {
    resultBuffer.create(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    scratchBuffer.create(
        dp::Buffer::alignedSize(sizeInfo.buildScratchSize, asProperties.minAccelerationStructureScratchOffsetAlignment),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU // Don't ever set this to GPU_ONLY or make the memory DEVICE_LOCAL
    );
}

dp::AccelerationStructureBuilder dp::AccelerationStructureBuilder::create(const dp::Context& context, const VkCommandPool commandPool) {
    dp::AccelerationStructureBuilder builder(context, commandPool);
    return builder;
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
        fmt::print("Building AS: {}\n", mesh.name);
        dp::BottomLevelAccelerationStructure blas(ctx, mesh, mesh.name);

        blas.createMeshBuffers(asProperties);

        VkAccelerationStructureGeometryKHR structureGeometry = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .pNext = nullptr,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry = {
                .triangles = {
                    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                    .vertexFormat = mesh.vertexFormat,
                    .vertexData = { .deviceAddress = blas.vertexBuffer.getDeviceAddress(), },
                    .vertexStride = dp::Mesh::vertexStride,
                    .maxVertex = static_cast<uint32_t>(mesh.vertices.size()),
                    .indexType = mesh.indexType,
                    .indexData = { .deviceAddress = blas.indexBuffer.getDeviceAddress(), },
                    .transformData = { .deviceAddress = blas.transformBuffer.getDeviceAddress(), },
                }
            }
        };

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
        buildGeometryInfo.scratchData.deviceAddress = scratchBuffer.getDeviceAddress();

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
            std::vector<VkBufferMemoryBarrier> barriers = {
                blas.vertexBuffer.getMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                                   VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR),
                blas.indexBuffer.getMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                                   VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR),
                blas.transformBuffer.getMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                                   VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR),
            };
            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0,
                                 0, nullptr,
                                 barriers.size(), barriers.data(),
                                 0, nullptr);
            ctx.setCheckpoint(cmdBuffer, "Building BLAS!");
            ctx.buildAccelerationStructures(cmdBuffer, 1, &buildGeometryInfo, buildRangeInfos);
        });

        // Get AS device address
        blas.address = ctx.getAccelerationStructureDeviceAddress(blas.handle);
        blas.setName();
        blas.destroyMeshStagingBuffers();
        scratchBuffer.destroy();
        fmt::print("Finished building AS: {}\n", mesh.name);

        blases.push_back(blas);
    }

    dp::TopLevelAccelerationStructure tlas(ctx);
    {
        std::vector<VkAccelerationStructureInstanceKHR> asInstances(std::min(instances.size(), asProperties.maxInstanceCount));

        for (const auto& instance : instances) {
            size_t index = &instance - &instances[0];
            if (index == asInstances.size() - 1) break;
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
                VMA_MEMORY_USAGE_GPU_ONLY);

            instanceDataDeviceAddress = instanceBuffer.getDeviceOrHostConstAddress();
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
        accelerationBuildGeometryInfo.scratchData = scratchBuffer.getDeviceOrHostAddress();

        VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo = {};
        accelerationStructureBuildRangeInfo.primitiveCount = primitiveCount;
        accelerationStructureBuildRangeInfo.primitiveOffset = 0;
        accelerationStructureBuildRangeInfo.firstVertex = 0;
        accelerationStructureBuildRangeInfo.transformOffset = 0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR*> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

        ctx.oneTimeSubmit(ctx.graphicsQueue, commandPool, [&](VkCommandBuffer cmdBuffer) {
            instanceStagingBuffer.copyToBuffer(cmdBuffer, instanceBuffer);
            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 0, nullptr, 0, nullptr, 0, nullptr);
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

        scratchBuffer.destroy();
        if (primitiveCount > 0) instanceBuffer.destroy();
    }

    std::chrono::steady_clock::time_point endBuild = std::chrono::steady_clock::now();
    fmt::print("Finished building acceleration structures. Took {}ms.\n",
               std::chrono::duration_cast<std::chrono::milliseconds>(endBuild - beginBuild).count());

    return tlas;
}
