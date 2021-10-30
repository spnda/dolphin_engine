#include "modelmanager.hpp"

#include <future>
#include <thread>
#include <fmt/core.h>

#include "../vulkan/resource/stagingbuffer.hpp"
#include "../vulkan/resource/texture.hpp"
#include "../vulkan/context.hpp"
#include "../engine.hpp"

dp::ModelManager::ModelManager(const dp::Context& context, dp::Engine& engine)
    : ctx(context), engine(engine), tlas(ctx),
      materialBuffer(ctx, "materialBuffer"), descriptionBuffer(ctx, "descriptionBuffer") {
}

void dp::ModelManager::createDescriptionBuffers() {
    materialBuffer.destroy();
    descriptionBuffer.destroy();

    for (auto& mat : fileLoader.materials)
        if (mat.textureIndex > 0)
            mat.textureIndex++; // Because we always have an initial empty image, increment the index by 1.
    auto materialSize = fileLoader.materials.size() * sizeof(Material);
    materialBuffer.create(
        std::max(materialSize, static_cast<uint64_t>(1)),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    if (materialSize != 0)
        materialBuffer.memoryCopy(fileLoader.materials.data(), materialSize);

    descriptions.resize(blases.size());
    for (const auto& blas : blases) {
        ObjectDescription desc = {
            .vertexBufferAddress = blas.meshBuffer.getDeviceAddress() + blas.vertexOffset,
            .indexBufferAddress = blas.meshBuffer.getDeviceAddress() + blas.indexOffset,
        };
        //if (blas.mesh.materialIndex < fileLoader.materials.size()) {
            desc.materialIndex = blas.mesh.materialIndex;
        //}
        descriptions[&blas - &blases[0]] = desc;
    }

    auto descriptionSize = descriptions.size() * sizeof(ObjectDescription);
    descriptionBuffer.create(
        std::max(descriptionSize, static_cast<uint64_t>(1)),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    if (descriptionSize != 0)
        descriptionBuffer.memoryCopy(descriptions.data(), descriptionSize);
}

void dp::ModelManager::buildBlas(const dp::Mesh& mesh) {
    fmt::print("Building BLAS {}\n", mesh.name);
    if (mesh.name == "vase_round") return;

    VkPhysicalDeviceProperties2 deviceProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &this->asProperties,
    };
    vkGetPhysicalDeviceProperties2(ctx.physicalDevice, &deviceProperties);

    const uint64_t primitiveCount = std::min(mesh.indices.size() / 3, asProperties.maxPrimitiveCount);

    dp::BottomLevelAccelerationStructure blas(ctx, mesh);
    blas.createMeshBuffers(asProperties);

    VkAccelerationStructureGeometryKHR accelerationStructureGeometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = {
            .triangles = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                .vertexFormat = mesh.vertexFormat,
                .vertexData {
                    .deviceAddress = blas.meshBuffer.getDeviceAddress() + blas.vertexOffset,
                },
                .vertexStride = dp::Mesh::vertexStride,
                .maxVertex = static_cast<uint32_t>(mesh.vertices.size() - 1),
                .indexType = mesh.indexType,
                .indexData = {
                    .deviceAddress = blas.meshBuffer.getDeviceAddress() + blas.indexOffset,
                },
                .transformData = {
                    .deviceAddress = blas.transformBuffer.getDeviceAddress(),
                },
            },
        },
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &accelerationStructureGeometry,
    };
    auto sizes = blas.getBuildSizes(primitiveCount, &buildGeometryInfo, asProperties);
    blas.createScratchBuffer(sizes);
    blas.createResultBuffer(sizes);
    blas.createStructure(sizes);
    ctx.setDebugUtilsName(blas.handle, mesh.name);

    buildGeometryInfo.dstAccelerationStructure = blas.handle;
    buildGeometryInfo.scratchData.deviceAddress = blas.scratchBuffer.getDeviceAddress();

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo = {
        .primitiveCount = static_cast<uint32_t>(primitiveCount),
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0,
    };
    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos = { &buildRangeInfo };

    ctx.oneTimeSubmit(ctx.graphicsQueue, ctx.commandPool, [&](auto cmdBuffer) {
        blas.copyMeshBuffers(cmdBuffer);
        VkMemoryBarrier memBarrier = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
        };
        vkCmdPipelineBarrier(cmdBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0,
                             1, &memBarrier, 0, nullptr, 0, nullptr);
        ctx.setCheckpoint(cmdBuffer, "Building BLAS!");
        ctx.buildAccelerationStructures(cmdBuffer, 1, buildGeometryInfo, buildRangeInfos.data());
    });

    blas.destroyMeshBuffers();
    blas.scratchBuffer.destroy();
    blases.emplace_back(blas);
}

void dp::ModelManager::buildTlas() {
    VkPhysicalDeviceProperties2 deviceProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &this->asProperties,
    };
    vkGetPhysicalDeviceProperties2(ctx.physicalDevice, &deviceProperties);

    const uint32_t primitiveCount = std::min(static_cast<uint32_t>(blases.size()), static_cast<uint32_t>(asProperties.maxInstanceCount));

    std::vector<VkAccelerationStructureInstanceKHR> instances(primitiveCount, VkAccelerationStructureInstanceKHR {});
    for (auto& instance : instances) {
        size_t index = &instance - &instances[0];
        instance.transform = {
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0
        };
        instance.instanceCustomIndex = index;
        instance.mask = 0xFF;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.accelerationStructureReference = blases[index].address;
    }

    dp::StagingBuffer instanceStagingBuffer(ctx, "tlasInstanceStagingBuffer");
    dp::Buffer instanceBuffer(ctx, "tlasInstanceBuffer");
    VkDeviceAddress instanceDataDeviceAddress = {};

    if (primitiveCount > 0) {
        size_t instanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR) * primitiveCount;
        instanceStagingBuffer.create(instanceBufferSize);
        instanceStagingBuffer.memoryCopy(instances.data(), instanceBufferSize);

        instanceBuffer.create(instanceBufferSize,
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
                              VMA_MEMORY_USAGE_GPU_ONLY);
        instanceDataDeviceAddress = instanceBuffer.getDeviceAddress();
    }

    VkAccelerationStructureGeometryKHR accelerationStructureGeometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {
            .instances = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                .arrayOfPointers = VK_FALSE,
                .data = {
                    .deviceAddress = instanceDataDeviceAddress,
                },
            },
        },
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1, // Has to be exactly one for a TLAS
        .pGeometries = &accelerationStructureGeometry,
    };
    auto sizes = tlas.getBuildSizes(primitiveCount, &buildGeometryInfo, asProperties);
    tlas.createScratchBuffer(sizes);
    tlas.createResultBuffer(sizes);
    tlas.createStructure(sizes);
    ctx.setDebugUtilsName(tlas.handle, "TLAS");

    // Update buildGeometryInfo
    buildGeometryInfo.dstAccelerationStructure = tlas.handle;
    buildGeometryInfo.scratchData.deviceAddress = tlas.scratchBuffer.getDeviceAddress();

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo = {
        .primitiveCount = primitiveCount,
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0,
    };
    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos = { &buildRangeInfo };

    ctx.oneTimeSubmit(ctx.graphicsQueue, ctx.commandPool, [&](auto cmdBuffer) {
        instanceStagingBuffer.copyToBuffer(cmdBuffer, instanceBuffer);
        VkMemoryBarrier memBarrier = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR,
        };
        vkCmdPipelineBarrier(cmdBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0,
                             1, &memBarrier, 0, nullptr, 0, nullptr);
        ctx.setCheckpoint(cmdBuffer, "Building TLAS!");
        tlas.buildStructure(cmdBuffer, 1, buildGeometryInfo, buildRangeInfos.data());
    });

    tlas.scratchBuffer.destroy();
    instanceStagingBuffer.destroy();
    instanceBuffer.destroy();
}

void dp::ModelManager::destroy() {
    materialBuffer.destroy();
    descriptionBuffer.destroy();
    for (auto& blas : blases) {
        blas.meshBuffer.destroy();
        blas.transformBuffer.destroy();
        blas.destroy();
    }
    for (auto& texture : textures) {
        texture.destroy();
    }
    tlas.destroy();
}

void dp::ModelManager::init() {
    // Empty texture file as we always need at least 1 texture to exist.
    dp::TextureFile emptyTextureFile;
    emptyTextureFile.width = 1; emptyTextureFile.height = 1;
    emptyTextureFile.pixels = { 0xFF, 0xFF, 0xFF, 0xFF }; // White image
    emptyTextureFile.format = VK_FORMAT_R8G8B8A8_SRGB;
    uploadTexture(emptyTextureFile);

    // Build the basic TLAS with no BLASes.
    buildTlas();
}

std::vector<VkDescriptorImageInfo> dp::ModelManager::getTextureDescriptorInfos() {
    // Create the image infos for each texture
    std::vector<VkDescriptorImageInfo> descriptors(textures.size());
    for (size_t i = 0; i < descriptors.size(); i++) {
        descriptors[i].sampler = textures[i].getSampler();
        descriptors[i].imageLayout = textures[i].getImageLayout();
        descriptors[i].imageView = VkImageView(textures[i]);
    }
    return descriptors;
}

void dp::ModelManager::loadScene(const std::string& path) {
    auto loader = [&](const std::string& path) -> void {
        fileLoader.loadFile(fs::path(path));
        sceneLoadFinished = true;
    };

    std::thread t(loader, path);
    fileLoadThread = std::move(t);
}

void dp::ModelManager::renderTick() {
    if (sceneLoadFinished) {
        sceneLoadFinished = false;
        fileLoadThread.detach();

        // Delete old textures and BLASs and the TLAS.
        // We do not want to delete the first image, as that has to always exist.
        for (uint64_t i = 1; i < textures.size(); i++) {
            textures[i].destroy();
        }
        textures.erase(textures.begin() + 1, textures.end()); // Leaves all but the first.
        for (auto& blas : blases) {
            blas.destroy();
        }
        blases.clear();
        tlas.destroy();

        // Build new BLASes
        for (uint32_t i = 0; i < 350 && i < fileLoader.meshes.size(); i++) {
            buildBlas(fileLoader.meshes[i]);
        }

        // Upload new textures
        for (auto& file : fileLoader.textures) {
            uploadTexture(file);
        }

        buildTlas();
        engine.updateTlas();

        engine.ui.reloadingScene = false;
    }
}

void dp::ModelManager::uploadTexture(dp::TextureFile& textureFile) {
    if (textureFile.pixels.empty()) {
        fmt::print("Empty texture! {}\n", textureFile.filePath.string());
        return;
    }

    dp::StagingBuffer stagingBuffer(ctx, "textureStagingBuffer");
    stagingBuffer.create(textureFile.pixels.size());
    stagingBuffer.memoryCopy(textureFile.pixels.data(), textureFile.pixels.size());

    // Generating mip levels requires blit support
    uint32_t mipLevels = 1;
    if (dp::Texture::formatSupportsBlit(ctx, textureFile.format)) {
        mipLevels = textureFile.mipLevels;
    }

    dp::Texture texture(ctx, { textureFile.width, textureFile.height }, textureFile.filePath.filename().string());
    texture.createTexture(textureFile.format, mipLevels);

    ctx.oneTimeSubmit(ctx.graphicsQueue, ctx.commandPool, [&](VkCommandBuffer cmdBuffer) {
        // Copy the staging buffer into the image.
        texture.changeLayout(
            cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1 },
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
        VkBufferImageCopy copy = {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            .imageExtent = texture.getImageSize3d(),
        };
        stagingBuffer.copyToImage(cmdBuffer, texture, texture.getImageLayout(), &copy);

        if (mipLevels == 1) {
            texture.changeLayout(
                cmdBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1 },
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        } else {
            // Generate mipmaps. Generating mipmaps will automatically transition to SHADER_READ_ONLY_OPTIMAL
            texture.generateMipmaps(cmdBuffer);
        }
    });
    stagingBuffer.destroy();

    textures.emplace_back(texture);

    fmt::print("Finished uploading texture {}!\n", textureFile.filePath.string());
}
