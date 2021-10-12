#include "acceleration_structure.hpp"

#include <utility>

#include "../context.hpp"
#include "../resource/stagingbuffer.hpp"

dp::AccelerationStructure::AccelerationStructure(const dp::Context& context, const AccelerationStructureType type, std::string asName)
        : ctx(context), name(std::move(asName)), type(type),
          resultBuffer(context, name + " resultBuffer") {
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

void dp::BottomLevelAccelerationStructure::createMeshBuffers() {
    vertexStagingBuffer.create(mesh.vertices.size() * sizeof(Vertex));
    vertexStagingBuffer.memoryCopy(mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));

    indexStagingBuffer.create(mesh.indices.size() * sizeof(Index));
    indexStagingBuffer.memoryCopy(mesh.indices.data(), mesh.indices.size() * sizeof(Index));

    transformStagingBuffer.create(sizeof(VkTransformMatrixKHR));
    transformStagingBuffer.memoryCopy(&mesh.transform, sizeof(VkTransformMatrixKHR));

    VkBufferUsageFlags bufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    VmaMemoryUsage usage = VMA_MEMORY_USAGE_GPU_ONLY;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    vertexBuffer.create(mesh.vertices.size() * sizeof(Vertex), bufferUsage, usage, properties);
    indexBuffer.create(mesh.indices.size() * sizeof(Index), bufferUsage, usage, properties);
    transformBuffer.create(sizeof(VkTransformMatrixKHR), bufferUsage, usage, properties);
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
