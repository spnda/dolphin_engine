#include "acceleration_structure.hpp"

#include <utility>

#include "../context.hpp"

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
      transformBuffer(context, "transformBuffer") {

}

void dp::BottomLevelAccelerationStructure::createMeshBuffers() {
    VkBufferUsageFlags bufferUsage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    VmaMemoryUsage usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    vertexBuffer.create(mesh.vertices.size() * sizeof(Vertex), bufferUsage, usage, properties);
    vertexBuffer.memoryCopy(mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));

    // Create the index buffer
    indexBuffer.create(mesh.indices.size() * sizeof(Index), bufferUsage, usage, properties);
    indexBuffer.memoryCopy(mesh.indices.data(), mesh.indices.size() * sizeof(Index));

    // Create the index buffer
    transformBuffer.create(sizeof(VkTransformMatrixKHR), bufferUsage, usage, properties);
    transformBuffer.memoryCopy(&mesh.transform, sizeof(VkTransformMatrixKHR));
}

dp::TopLevelAccelerationStructure::TopLevelAccelerationStructure(const dp::Context& context, const std::string& name)
    : AccelerationStructure(context, AccelerationStructureType::TopLevel, name) {

}
