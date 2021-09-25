#include "modelmanager.hpp"

#include <array>
#include <chrono>
#include <iostream>
#include <type_traits>

#include "modelloader.hpp"
#include "../vulkan/context.hpp"
#include "../vulkan/rt/acceleration_structure_builder.hpp"

dp::ModelManager::ModelManager(const dp::Context& context)
    : ctx(context), topLevelAccelerationStructure(ctx, AccelerationStructureType::TopLevel, "TLAS"), materialBuffer(ctx, "materialBuffer") {

}

void dp::ModelManager::loadMesh(const std::string fileName) {
    std::cout << "Importing file " << fileName << std::endl;
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    dp::ModelLoader modelLoader(ctx);
    modelLoader.loadFile(fileName);

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::cout << "Finished importing "
        << fileName
        << ". Took "
        << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()
        << "ms."
        << std::endl;

    this->updateTLAS(modelLoader);
}

void dp::ModelManager::applyMaterialOffsets(std::vector<Mesh>& newMeshes) {
    for (auto& newMesh : newMeshes) {
        newMesh.materialIndex += currentMaterialOffset;
    }
}

void dp::ModelManager::updateTLAS(const dp::ModelLoader& modelLoader) {
    tlasMutex.lock();

    // As this usually gets executed from different threads, we'll
    // just get a new queue and command pool.
    VkCommandPool pool = ctx.createCommandPool(ctx.getQueueIndex(vkb::QueueType::compute), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    // Copy loaded meshes over, after applying the material offset.
    if (materials.size() == 1)
        materials.clear();
    std::vector<Mesh> newMeshes = modelLoader.meshes;
    if (currentMaterialOffset != 0)
        applyMaterialOffsets(newMeshes);
    meshes.insert(meshes.end(), modelLoader.meshes.begin(), modelLoader.meshes.end());
    materials.insert(materials.end(), modelLoader.materials.begin(), modelLoader.materials.end());

    auto builder = dp::AccelerationStructureBuilder::create(ctx, pool);
    for (const auto& mesh : meshes) {
        uint32_t meshIndex = builder.addMesh(mesh);
        builder.addInstance(dp::AccelerationStructureInstance {
            .transformMatrix = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f
            },
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .blasIndex = meshIndex,
        });
    }
    this->topLevelAccelerationStructure = builder.build();

    currentMaterialOffset += materials.size() - 1;
    tlasMutex.unlock();
}

void dp::ModelManager::buildTopLevelAccelerationStructure() {
    // Cleanup the old acceleration structure if it still exists.
    if (this->topLevelAccelerationStructure.handle != nullptr) {
        ctx.destroyAccelerationStructure(topLevelAccelerationStructure.handle);
        topLevelAccelerationStructure.handle = nullptr;
        topLevelAccelerationStructure.address = 0;
        topLevelAccelerationStructure.resultBuffer.destroy();
    }

    auto builder = dp::AccelerationStructureBuilder::create(ctx, ctx.commandPool);
    this->topLevelAccelerationStructure = builder.build();

    materialBuffer.create(
        materials.size() * sizeof(Material),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 
    );
    materialBuffer.memoryCopy(materials.data(), materials.size() * sizeof(Material));
}

void dp::ModelManager::loadMeshes(std::initializer_list<std::string> files) {
    for (const auto& file : files) {
        std::thread thread(&ModelManager::loadMesh, this, file);
        currentThreads.push_back(std::move(thread));
    }
}

dp::AccelerationStructure& dp::ModelManager::getTLAS() {
    return this->topLevelAccelerationStructure;
}

dp::Buffer& dp::ModelManager::getMaterialBuffer() {
    return this->materialBuffer;
}
