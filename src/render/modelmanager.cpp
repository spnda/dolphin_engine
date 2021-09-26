#include "modelmanager.hpp"

#include <array>
#include <chrono>
#include <iostream>
#include <type_traits>

#include "modelloader.hpp"
#include "../vulkan/context.hpp"
#include "../vulkan/rt/rt_pipeline.hpp"

dp::ModelManager::ModelManager(const dp::Context& context, const dp::RayTracingPipeline& pipeline)
    : ctx(context), tlas(ctx, "TLAS"), materialBuffer(ctx, "materialBuffer"), pipeline(pipeline) {
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
    std::chrono::steady_clock::time_point beginBuild = std::chrono::steady_clock::now();

    // As this usually gets executed from different threads, we'll
    // just get a new queue and command pool.
    auto pool = ctx.createCommandPool(ctx.getQueueIndex(vkb::QueueType::compute), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    // Copy loaded meshes over, after applying the material offset.
    if (materials.size() == 1)
        materials.clear();
    std::vector<Mesh> newMeshes = modelLoader.meshes;
    if (currentMaterialOffset != 0)
        applyMaterialOffsets(newMeshes);
    meshes.insert(meshes.end(), modelLoader.meshes.begin(), modelLoader.meshes.end());
    materials.insert(materials.end(), modelLoader.materials.begin(), modelLoader.materials.end());

    // Create a semaphore for the BLAS and TLAS builds.
    VkSemaphore blasBuildSemaphore;
    VkSemaphoreCreateInfo blasBuildSemaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .flags = 0,
    };
    vkCreateSemaphore(ctx.device, &blasBuildSemaphoreInfo, nullptr, &blasBuildSemaphore);

    // Rebuild the TLAS. We will only create the new BLASes, as
    // tlas::updateBLASes only appends to the already existing list.
    std::vector<BottomLevelAccelerationStructure> blases = {};
    auto commandBuffer = ctx.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, pool, true);
    for (auto it = meshes.begin() + (meshes.size() - newMeshes.size()); it != meshes.end(); ++it) {
        dp::BottomLevelAccelerationStructure blas(ctx, *it, it->name);
        blas.create(Build);
        blas.build(commandBuffer);
        blases.push_back(blas);
    }
    ctx.flushCommandBuffer(commandBuffer, ctx.graphicsQueue, blasBuildSemaphore);

    commandBuffer = ctx.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, pool, true);
    tlas.updateBLASes(blases); // Only adds new BLASes, doesn't remove any!
    tlas.create(Update);
    tlas.build(commandBuffer);
    ctx.flushCommandBuffer(
        commandBuffer, ctx.graphicsQueue, nullptr,
        blasBuildSemaphore, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);

    // TODO: Update the material buffer

    std::chrono::steady_clock::time_point endBuild = std::chrono::steady_clock::now();
    std::cout << "Finished updating TLAS. Took "
        << std::chrono::duration_cast<std::chrono::milliseconds>(endBuild - beginBuild).count()
        << "ms."
        << std::endl;

    currentMaterialOffset += materials.size() - 1;
    tlasMutex.unlock();
}

void dp::ModelManager::buildTopLevelAccelerationStructure() {
    tlasMutex.lock();

    // Cleanup the old acceleration structure if it still exists.
    tlas.destroy();

    // Not updating or creating any BLASes as our initial TLAS is 
    // going to be empty.
    auto commandBuffer = ctx.createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, ctx.commandPool, true);
    tlas.create(Build);
    tlas.build(commandBuffer);
    tlasMutex.unlock();
    ctx.flushCommandBuffer(commandBuffer, ctx.graphicsQueue);

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

void dp::ModelManager::updateDescriptor() {
    tlasMutex.lock();

    // If we just updated the handle the previousHandle will still exist,
    // so we'll update the descriptor sets with the new handle and then destroy
    // the previous one.  
    if (tlas.getHandle() != nullptr && tlas.getPreviousHandle() != nullptr) {
        // Update Acceleration Structure in descriptor
        VkWriteDescriptorSetAccelerationStructureKHR structureDescriptorInfo = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .accelerationStructureCount = 1,
            .pAccelerationStructures = &tlas.getHandle(),
        };
        VkWriteDescriptorSet structureWrite = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &structureDescriptorInfo,
            .dstSet = pipeline.descriptorSet,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        };
        vkUpdateDescriptorSets(ctx.device, 1, &structureWrite, 0, nullptr);

        tlas.cleanup(); // Will delete the previous handle.
    }

    tlasMutex.unlock();
}

auto dp::ModelManager::getTLAS() const -> const dp::TopLevelAccelerationStructure& {
    return this->tlas;
}

auto dp::ModelManager::getMaterialBuffer() const -> const dp::Buffer& {
    return this->materialBuffer;
}
