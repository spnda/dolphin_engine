#include "acceleration_structure_builder.hpp"

#include "bottom_level_acceleration_structure.hpp"
#include "top_level_acceleration_structure.hpp"

dp::AccelerationStructureBuilder::AccelerationStructureBuilder(const dp::Context& context)
        : context(context) {
    
}

std::tuple<dp::Buffer&, dp::Buffer&, dp::Buffer&> dp::AccelerationStructureBuilder::createMeshBuffers(const dp::AccelerationStructureMesh& mesh) {
    dp::Buffer vertexBuffer(context);
    vertexBuffer.create(
        mesh.vertices.size() * sizeof(Vertex),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_CPU_ONLY,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vertexBuffer.memoryCopy(mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));

    // Create the index buffer
    dp::Buffer indexBuffer(context);
    indexBuffer.create(
        mesh.indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_CPU_ONLY,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    indexBuffer.memoryCopy(mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));

    // Create the index buffer
    dp::Buffer transformBuffer(context);
    transformBuffer.create(
        sizeof(VkTransformMatrixKHR),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_CPU_ONLY,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    transformBuffer.memoryCopy(&mesh.transformMatrix, sizeof(VkTransformMatrixKHR));
    return { vertexBuffer, indexBuffer, transformBuffer };
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
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
        VMA_MEMORY_USAGE_UNKNOWN,
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

void dp::AccelerationStructureBuilder::setInstance(dp::AccelerationStructureInstance instance) {
    this->instance = instance;
}

dp::TopLevelAccelerationStructure dp::AccelerationStructureBuilder::build() {
    VkCommandBuffer commandBuffer = context.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, context.commandPool, true);

    // TODO: We should use a single result buffer and scratch buffer for all the BLASs.
    // We would just have to implement the offsets for the blas#generate function call.
    for (const auto& mesh : meshes) {
        static const uint32_t bufferSize = 3;
        auto buffers = this->createMeshBuffers(mesh);

        BottomLevelGeometry geometry;
        geometry.addGeometryTriangles(std::get<0>(buffers), mesh.vertices.size(), std::get<1>(buffers), mesh.indices.size(), false);

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
}
