#include "acceleration_structure.hpp"

#include <utility>

#include "../context.hpp"
#include "../resource/stagingbuffer.hpp"

dp::AccelerationStructure::AccelerationStructure(const dp::Context& context, const AccelerationStructureType type, std::string asName)
        : ctx(context), name(std::move(asName)), type(type),
          resultBuffer(context, name + " resultBuffer"),
          scratchBuffer(context, name + " scratchBuffer") {
}

dp::AccelerationStructure::AccelerationStructure(const dp::AccelerationStructure& structure) = default;

dp::AccelerationStructure& dp::AccelerationStructure::operator=(const dp::AccelerationStructure& structure) {
    this->address = structure.address;
    this->handle = structure.handle;
    this->name = structure.name;
    this->type = structure.type;
    this->resultBuffer = structure.resultBuffer;
    return *this;
}

void dp::AccelerationStructure::createBuildBuffers(const VkAccelerationStructureBuildSizesInfoKHR sizeInfo, const VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties) {
    resultBuffer.create(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
    scratchBuffer.create(
        dp::Buffer::alignedSize(sizeInfo.buildScratchSize, asProperties.minAccelerationStructureScratchOffsetAlignment),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );
}

void dp::AccelerationStructure::setName() {
    ctx.setDebugUtilsName(handle, name);
}

dp::BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(const dp::Context& context, dp::Mesh mesh, const std::string& name)
    : AccelerationStructure(context, AccelerationStructureType::BottomLevel, name),
      mesh(std::move(mesh)),
      vertexBuffer(context, "vertexBuffer"),
      indexBuffer(context, "indexBuffer"),
      transformBuffer(context, "transformBuffer"),
      vertexStagingBuffer(ctx, "vertexStagingBuffer"),
      indexStagingBuffer(ctx, "indexStagingBuffer"),
      transformStagingBuffer(ctx, "transformStagingBuffer") {

}

void dp::BottomLevelAccelerationStructure::createMeshBuffers(const VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties) {
    vertexStagingBuffer.create(mesh.vertices.size() * sizeof(Vertex));
    vertexStagingBuffer.memoryCopy(mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));

    uint64_t maxTriangles = std::min(mesh.indices.size() / 3, asProperties.maxPrimitiveCount);
    indexStagingBuffer.create(maxTriangles * 3 * sizeof(Index));
    indexStagingBuffer.memoryCopy(mesh.indices.data(), maxTriangles * 3 * sizeof(Index));

    size_t transformBufferSize = dp::Buffer::alignedSize(sizeof(VkTransformMatrixKHR), 16); // See VUID-vkCmdBuildAccelerationStructuresKHR-pInfos-03810
    transformStagingBuffer.create(transformBufferSize);
    transformStagingBuffer.memoryCopy(&mesh.transform, sizeof(VkTransformMatrixKHR));

    VkBufferUsageFlags bufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    VmaMemoryUsage usage = VMA_MEMORY_USAGE_GPU_ONLY;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    vertexBuffer.create(mesh.vertices.size() * sizeof(Vertex), bufferUsage, usage, properties);

    indexBuffer.create(maxTriangles * 3 * sizeof(Index), bufferUsage, usage, properties);

    transformBuffer.create(transformBufferSize, bufferUsage, usage, properties);
}

void dp::BottomLevelAccelerationStructure::copyMeshBuffers(VkCommandBuffer cmdBuffer) {
    vertexStagingBuffer.copyToBuffer(cmdBuffer, vertexBuffer);
    indexStagingBuffer.copyToBuffer(cmdBuffer, indexBuffer);
    transformStagingBuffer.copyToBuffer(cmdBuffer, transformBuffer);
}

void dp::BottomLevelAccelerationStructure::destroyMeshStagingBuffers() {
    vertexStagingBuffer.destroy();
    indexStagingBuffer.destroy();
    transformStagingBuffer.destroy();
}

dp::TopLevelAccelerationStructure::TopLevelAccelerationStructure(const dp::Context& context, const std::string& name)
    : AccelerationStructure(context, AccelerationStructureType::TopLevel, name) {

}
