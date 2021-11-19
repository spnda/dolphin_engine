#include "acceleration_structure.hpp"

#include "../context.hpp"
#include "../resource/stagingbuffer.hpp"

dp::AccelerationStructure::AccelerationStructure(const dp::Context& context, dp::AccelerationStructureType asType, std::string name)
        : ctx(context), type(asType),
        resultBuffer(ctx, std::move(name)), scratchBuffer(ctx, std::move(name)) {
}

void dp::AccelerationStructure::createScratchBuffer(VkAccelerationStructureBuildSizesInfoKHR buildSizes) {
    scratchBuffer.create(
        buildSizes.buildScratchSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
}

void dp::AccelerationStructure::createResultBuffer(VkAccelerationStructureBuildSizesInfoKHR buildSizes) {
    resultBuffer.create(
        buildSizes.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
}

void dp::AccelerationStructure::createStructure(VkAccelerationStructureBuildSizesInfoKHR buildSizes) {
    VkAccelerationStructureCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .buffer = resultBuffer.getHandle(),
        .size = buildSizes.accelerationStructureSize,
        .type = static_cast<VkAccelerationStructureTypeKHR>(type),
    };
    ctx.createAccelerationStructure(createInfo, &handle);
    address = ctx.getAccelerationStructureDeviceAddress(handle);
}

void dp::AccelerationStructure::destroy() {
    ctx.destroyAccelerationStructure(handle);
    resultBuffer.destroy();
    scratchBuffer.destroy();
}

VkAccelerationStructureBuildSizesInfoKHR dp::AccelerationStructure::getBuildSizes(const uint32_t* primitiveCount,
                                                                                  VkAccelerationStructureBuildGeometryInfoKHR* buildGeometryInfo,
                                                                                  VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties) {
    VkAccelerationStructureBuildSizesInfoKHR buildSizes = ctx.getAccelerationStructureBuildSizes(primitiveCount, buildGeometryInfo);
    buildSizes.accelerationStructureSize = dp::Buffer::alignedSize(buildSizes.accelerationStructureSize, 256); // Apparently, this is part of the Vulkan Spec
    buildSizes.buildScratchSize = dp::Buffer::alignedSize(buildSizes.buildScratchSize, asProperties.minAccelerationStructureScratchOffsetAlignment);
    return buildSizes;
}

VkWriteDescriptorSetAccelerationStructureKHR dp::AccelerationStructure::getDescriptorWrite() const {
    return {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .pNext = nullptr,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &handle,
    };
}

dp::BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(const dp::Context& context, dp::Mesh&& mesh)
        : AccelerationStructure(context, dp::AccelerationStructureType::BottomLevel, mesh.name),
          mesh(mesh),
          vertexBuffer(ctx, "vertexBuffer"),
          indexBuffer(ctx, "indexBuffer"),
          transformBuffer(ctx, "transformBuffer"),
          vertexStagingBuffer(ctx, "vertexStagingBuffer"),
          indexStagingBuffer(ctx, "indexStagingBuffer"),
          transformStagingBuffer(ctx, "transformStagingBuffer"),
          geometryDescriptionBuffer(ctx, "geometryDescriptionBuffer") {
}

void dp::BottomLevelAccelerationStructure::createGeometryDescriptionBuffer() {
    geometryDescriptionBuffer.destroy();

    geometryDescriptions.clear();
    for (const auto& prim : mesh.primitives) {
        geometryDescriptions.push_back({
            .meshBufferVertexOffset = prim.meshBufferVertexOffset,
            .meshBufferIndexOffset = prim.meshBufferIndexOffset,
            .materialIndex = prim.materialIndex,
        });
    }

    auto descriptionSize = geometryDescriptions.size() * sizeof(dp::GeometryDescription);
    geometryDescriptionBuffer.create(
        descriptionSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    if (descriptionSize != 0)
        geometryDescriptionBuffer.memoryCopy(geometryDescriptions.data(), descriptionSize);
}

void dp::BottomLevelAccelerationStructure::createMeshBuffers() {
    // We want to squash every primitive of the mesh into a single long buffer.
    // This will make the buffer quite convoluted, as there are vertices and indices
    // over and over again, but it should help for simplicity’s sake.

    // First, we compute the total size of our mesh buffer.
    uint64_t totalVertexSize = 0, totalIndexSize = 0;
    for (const auto& prim : mesh.primitives) {
        totalVertexSize += prim.vertices.size() * dp::Primitive::vertexStride;
        totalIndexSize += prim.indices.size() * sizeof(dp::Index);
    }

    vertexStagingBuffer.create(totalVertexSize);
    indexStagingBuffer.create(totalIndexSize);
    transformStagingBuffer.create(sizeof(VkTransformMatrixKHR));

    // Copy the data into the big buffer at an offset.
    uint64_t currentVertexOffset = 0, currentIndexOffset = 0;
    for (auto& prim : mesh.primitives) {
        uint64_t vertexSize = prim.vertices.size() * dp::Primitive::vertexStride;
        uint64_t indexSize = prim.indices.size() * sizeof(dp::Index);

        prim.meshBufferVertexOffset = currentVertexOffset;
        prim.meshBufferIndexOffset = currentIndexOffset;

        vertexStagingBuffer.memoryCopy(prim.vertices.data(), vertexSize, prim.meshBufferVertexOffset);
        indexStagingBuffer.memoryCopy(prim.indices.data(), indexSize, prim.meshBufferIndexOffset);

        currentVertexOffset += vertexSize;
        currentIndexOffset += indexSize;
    }

    transformStagingBuffer.memoryCopy(&mesh.transform, sizeof(VkTransformMatrixKHR));

    // At last, we create the real buffers that reside on the GPU.
    VkBufferUsageFlags asInputBufferUsage =
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    vertexBuffer.create(totalVertexSize, asInputBufferUsage, VMA_MEMORY_USAGE_GPU_ONLY);
    indexBuffer.create(totalIndexSize, asInputBufferUsage, VMA_MEMORY_USAGE_GPU_ONLY);
    transformBuffer.create(sizeof(VkTransformMatrixKHR), asInputBufferUsage, VMA_MEMORY_USAGE_GPU_ONLY);
}

void dp::BottomLevelAccelerationStructure::copyMeshBuffers(VkCommandBuffer const cmdBuffer) {
    vertexStagingBuffer.copyToBuffer(cmdBuffer, vertexBuffer);
    indexStagingBuffer.copyToBuffer(cmdBuffer, indexBuffer);
    transformStagingBuffer.copyToBuffer(cmdBuffer, transformBuffer);
}

void dp::BottomLevelAccelerationStructure::destroyMeshBuffers() {
    vertexStagingBuffer.destroy();
    indexStagingBuffer.destroy();
}

dp::TopLevelAccelerationStructure::TopLevelAccelerationStructure(const dp::Context& ctx)
        : AccelerationStructure(ctx, dp::AccelerationStructureType::TopLevel, "tlas") {
}
