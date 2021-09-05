#include "acceleration_structure_builder.hpp"

#include "bottom_level_acceleration_structure.hpp"
#include "top_level_acceleration_structure.hpp"

dp::AccelerationStructureBuilder::AccelerationStructureBuilder(const dp::Context& context)
        : context(context) {
    
}

void dp::AccelerationStructureBuilder::createMeshBuffers(dp::Buffer& vertexBuffer, dp::Buffer& indexBuffer, dp::Buffer& transformBuffer, const dp::AccelerationStructureMesh& mesh) {
    vertexBuffer.create(
        mesh.vertices.size() * sizeof(Vertex),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_CPU_ONLY,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vertexBuffer.memoryCopy(mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));

    // Create the index buffer
    indexBuffer.create(
        mesh.indices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_CPU_ONLY,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    indexBuffer.memoryCopy(mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));

    // Create the index buffer
    transformBuffer.create(
        sizeof(VkTransformMatrixKHR),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_CPU_ONLY,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    transformBuffer.memoryCopy(&mesh.transformMatrix, sizeof(VkTransformMatrixKHR));
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
    auto cmdBuffer = context.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, context.commandPool, true);

    // BLAS.
    auto mesh = meshes[0];
    dp::Buffer vertexBuffer(context);
    dp::Buffer indexBuffer(context);
    dp::Buffer transformBuffer(context);
    this->createMeshBuffers(vertexBuffer, indexBuffer, transformBuffer, mesh);

    BottomLevelGeometry geometry;
    geometry.addGeometryTriangles(vertexBuffer, mesh.vertices.size(), indexBuffer, mesh.indices.size(), false);

    // Note to future self: We can't use the ret value of emplace_back!
    dp::BottomLevelAccelerationStructure blas(context, geometry);
    dp::Buffer resultBuffer(context);
    dp::Buffer scratchBuffer(context);
    createBuildBuffers(scratchBuffer, resultBuffer, blas.getBuildSizes());
    context.setDebugUtilsName(scratchBuffer.handle, "blasScratchBuffer");
    context.setDebugUtilsName(resultBuffer.handle, "blasResultBuffer");
        
    printf("Generating BLAS.\n");
    blas.generate(cmdBuffer, scratchBuffer, resultBuffer, 0);

    // TLAS.
    auto asInstance = TopLevelAccelerationStructure::createInstance(context, blas, instance.transformMatrix, 0, 0);
    
    dp::Buffer instanceBuffer(context);
    instanceBuffer.create(
        sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
        VMA_MEMORY_USAGE_CPU_ONLY,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    context.setDebugUtilsName(instanceBuffer.handle, "instanceBuffer");
    instanceBuffer.memoryCopy(&asInstance, sizeof(VkAccelerationStructureInstanceKHR));

    // We need a memory barrier, as we have to properly wait for the BLASs
    // to have been built.
    dp::AccelerationStructure::memoryBarrier(cmdBuffer);

    dp::TopLevelAccelerationStructure tlas(context, instanceBuffer.address, 1);

    dp::Buffer tlasResultBuffer(context);
    dp::Buffer tlasScratchBuffer(context);
    const auto total = tlas.getBuildSizes();
    createBuildBuffers(tlasScratchBuffer, tlasResultBuffer, total);
    context.setDebugUtilsName(tlasScratchBuffer.handle, "tlasScratchBuffer");
    context.setDebugUtilsName(tlasResultBuffer.handle, "tlasResultBuffer");

    printf("Generating TLAS.\n");
    tlas.generate(cmdBuffer, tlasScratchBuffer, tlasResultBuffer, 0);
    context.setDebugUtilsName(tlas.getAccelerationStructure(), "TLAS");

    context.flushCommandBuffer(cmdBuffer, context.graphicsQueue);

    return tlas;
}

/*dp::TopLevelAccelerationStructure dp::AccelerationStructureBuilder::build() {
    VkCommandBuffer commandBuffer = context.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, context.commandPool, true);

    // TODO: We should use a single result buffer and scratch buffer for all the BLASs.
    // We would just have to implement the offsets for the blas#generate function call.
    for (const auto& mesh : meshes) {
        static const uint32_t bufferSize = 3;
        dp::Buffer vertexBuffer(context);
        dp::Buffer indexBuffer(context);
        dp::Buffer transformBuffer(context);
        this->createMeshBuffers(vertexBuffer, indexBuffer, transformBuffer, mesh);

        BottomLevelGeometry geometry;
        geometry.addGeometryTriangles(vertexBuffer, mesh.vertices.size(), indexBuffer, mesh.indices.size(), false);

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
}*/
