#include "acceleration_structure.hpp"

#include "../context.hpp"
#include "../resource/stagingbuffer.hpp"

dp::AccelerationStructure::AccelerationStructure(const dp::Context& context, dp::AccelerationStructureType asType, std::string name)
        : ctx(context), type(asType),
        resultBuffer(ctx, std::move(name)), scratchBuffer(ctx, std::move(name)) {
}

void dp::AccelerationStructure::buildStructure(VkCommandBuffer cmdBuffer, uint32_t geometryCount, VkAccelerationStructureBuildGeometryInfoKHR& geometryInfo, VkAccelerationStructureBuildRangeInfoKHR** rangeInfo) {
    ctx.buildAccelerationStructures(cmdBuffer, geometryCount, geometryInfo, rangeInfo);
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

VkAccelerationStructureBuildSizesInfoKHR dp::AccelerationStructure::getBuildSizes(uint64_t primitiveCount,
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

dp::BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(const dp::Context& context, const dp::Mesh& mesh)
        : AccelerationStructure(context, dp::AccelerationStructureType::BottomLevel, mesh.name),
          mesh(mesh),
          meshBuffer(ctx, "meshBuffer"),
          transformBuffer(ctx, "transformBuffer"),
          meshStagingBuffer(ctx, "meshStagingBuffer"),
          transformStagingBuffer(ctx, "transformStagingBufer") {
}

void dp::BottomLevelAccelerationStructure::createMeshBuffers(const VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties) {
    uint64_t vertexSize = mesh.vertices.size() * dp::Mesh::vertexStride;
    uint64_t indexSize = mesh.indices.size() * sizeof(Index);
    uint64_t totalSize = vertexSize + indexSize;
    vertexOffset = 0;
    indexOffset = vertexSize;
    meshStagingBuffer.create(totalSize);
    transformStagingBuffer.create(sizeof(VkTransformMatrixKHR));

    meshStagingBuffer.memoryCopy(mesh.vertices.data(), vertexSize, vertexOffset);
    meshStagingBuffer.memoryCopy(mesh.indices.data(), indexSize, indexOffset);
    transformStagingBuffer.memoryCopy(&mesh.transform, sizeof(VkTransformMatrixKHR));

    VkBufferUsageFlags asInputBufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    meshBuffer.create(totalSize,
                      asInputBufferUsage,
                      VMA_MEMORY_USAGE_GPU_ONLY);

    transformBuffer.create(sizeof(VkTransformMatrixKHR),
                           asInputBufferUsage,
                           VMA_MEMORY_USAGE_GPU_ONLY);
}

void dp::BottomLevelAccelerationStructure::copyMeshBuffers(VkCommandBuffer const cmdBuffer) {
    meshStagingBuffer.copyToBuffer(cmdBuffer, meshBuffer);
    transformStagingBuffer.copyToBuffer(cmdBuffer, transformBuffer);
}

void dp::BottomLevelAccelerationStructure::destroyMeshBuffers() {
    meshStagingBuffer.destroy();
}

dp::TopLevelAccelerationStructure::TopLevelAccelerationStructure(const dp::Context& ctx)
        : AccelerationStructure(ctx, dp::AccelerationStructureType::TopLevel, "tlas") {
}
