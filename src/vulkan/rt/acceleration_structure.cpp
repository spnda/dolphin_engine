#include "acceleration_structure.hpp"

#include "../context.hpp"
#include "../resource/buffer.hpp"
#include "../utils.hpp"

dp::AccelerationStructure::AccelerationStructure(const dp::Context& context, const AccelerationStructureType type, const std::string asName)
        : ctx(context), name(asName), type(type),
          resultBuffer(context, name + " resultBuffer"), scratchBuffer(context, name + " resultBuffer") {
}

dp::AccelerationStructure::AccelerationStructure(const dp::AccelerationStructure& structure)
        : ctx(structure.ctx), name(structure.name),
          resultBuffer(structure.resultBuffer), scratchBuffer(structure.scratchBuffer),
          handle(structure.handle), address(structure.address), type(structure.type) {
}

dp::AccelerationStructure& dp::AccelerationStructure::operator=(const dp::AccelerationStructure& structure) {
    this->address = structure.address;
    this->handle = structure.handle;
    this->name = structure.name;
    this->type = structure.type;
    this->resultBuffer = structure.resultBuffer;
    return *this;
}

void dp::AccelerationStructure::createBuildBuffers(dp::Buffer& scratchBuffer, dp::Buffer& resultBuffer) const {
    resultBuffer.create(
        buildSizes.accelerationStructureSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        0
    );
    scratchBuffer.create(
        buildSizes.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR
    );
}

auto dp::AccelerationStructure::getAccelerationStructureProperties() const -> VkPhysicalDeviceAccelerationStructurePropertiesKHR {
    static VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
    };
    if (asProperties.maxPrimitiveCount == 0) {
        VkPhysicalDeviceProperties2 deviceProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            .pNext = &asProperties,
        };
        vkGetPhysicalDeviceProperties2(ctx.physicalDevice, &deviceProperties);
    }
    return asProperties;
}

void dp::AccelerationStructure::setName() {
    assert(handle != nullptr);
    ctx.setDebugUtilsName(handle, name);
}

auto dp::AccelerationStructure::getHandle() const -> const VkAccelerationStructureKHR& {
    return this->handle;
}

auto dp::AccelerationStructure::getPreviousHandle() const -> const VkAccelerationStructureKHR& {
    return this->previousHandle;
}

void dp::AccelerationStructure::build(const VkCommandBuffer commandBuffer) {
    writeMutex.lock();

    VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo = {
        .primitiveCount = primitiveCount, // everything else is 0 already.
    };
    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos = { &accelerationStructureBuildRangeInfo };

    ctx.buildAccelerationStructures(commandBuffer, { buildGeometryInfo }, buildRangeInfos);
    writeMutex.unlock();
}

void dp::AccelerationStructure::destroy() {
    writeMutex.lock();
    if (previousHandle != nullptr) {
        ctx.destroyAccelerationStructure(previousHandle);
        previousHandle = nullptr;
    }
    if (handle != nullptr) {
        ctx.destroyAccelerationStructure(handle);
        handle = nullptr;
    }
    writeMutex.unlock();
}

// BOTTOM LEVEL
dp::BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(const dp::Context& context, dp::Mesh mesh, const std::string name)
    : AccelerationStructure(context, dp::AccelerationStructureType::BottomLevel, name), mesh(mesh),
      vertexBuffer(ctx, "vertexBuffer"), indexBuffer(ctx, "indexBuffer"), transformBuffer(ctx, "transformBuffer") {
}

void dp::BottomLevelAccelerationStructure::createMeshBuffers() {
    VkBufferUsageFlags bufferUsage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaMemoryUsage usage = VMA_MEMORY_USAGE_GPU_ONLY;
    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    // Create the vertex buffer
    vertexBuffer.create(mesh.vertices.size() * sizeof(Vertex), bufferUsage, usage, properties);
    vertexBuffer.memoryCopy(mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));

    // Create the index buffer
    indexBuffer.create(mesh.indices.size() * sizeof(Index), bufferUsage, usage, properties);
    indexBuffer.memoryCopy(mesh.indices.data(), mesh.indices.size() * sizeof(Index));

    // Create the transform buffer
    transformBuffer.create(sizeof(VkTransformMatrixKHR), bufferUsage, usage, properties);
    transformBuffer.memoryCopy(&mesh.transform, sizeof(VkTransformMatrixKHR));
}

auto dp::BottomLevelAccelerationStructure::getMesh() const -> const dp::Mesh& {
    return this->mesh;
}

void dp::BottomLevelAccelerationStructure::create(const dp::AccelerationStructureBuildMode mode) {
    writeMutex.lock();
    // Create mesh buffers.
    this->createMeshBuffers();

    geometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .pNext = nullptr,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = {
            .triangles = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                .vertexFormat = mesh.vertexFormat,
                .vertexData = { .deviceAddress = vertexBuffer.address, },
                .vertexStride = mesh.stride,
                .maxVertex = static_cast<uint32_t>(mesh.vertices.size()),
                .indexType = mesh.indexType,
                .indexData = { .deviceAddress = indexBuffer.address, },
                .transformData = { .deviceAddress = transformBuffer.address, }
            }
        }
    };

    buildGeometryInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = static_cast<VkAccelerationStructureTypeKHR>(type),
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    primitiveCount = mesh.indices.size() / 3;
    buildSizes = ctx.getAccelerationStructureBuildSizes(primitiveCount, buildGeometryInfo);
    createBuildBuffers(scratchBuffer, resultBuffer);

    // Create the acceleration structure.
    VkAccelerationStructureCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = resultBuffer.getHandle(),
        .size = buildSizes.accelerationStructureSize,
        .type = static_cast<VkAccelerationStructureTypeKHR>(type),
    };
    auto result = ctx.createAccelerationStructure(createInfo, &handle);
    checkResult(result, "Failed to create acceleration structure");
    address = ctx.getAccelerationStructureDeviceAddress(handle);

    buildGeometryInfo.mode = static_cast<VkBuildAccelerationStructureModeKHR>(mode);
    buildGeometryInfo.dstAccelerationStructure = handle;
    buildGeometryInfo.scratchData.deviceAddress = scratchBuffer.address;
    
    this->setName();
    writeMutex.unlock();
}

void dp::BottomLevelAccelerationStructure::cleanup() {
    writeMutex.lock();
    this->vertexBuffer.destroy();
    this->indexBuffer.destroy();
    this->transformBuffer.destroy();
    writeMutex.unlock();
}

// TOP LEVEL
dp::TopLevelAccelerationStructure::TopLevelAccelerationStructure(const dp::Context& context, const std::string name)
    : AccelerationStructure(context, dp::AccelerationStructureType::TopLevel, name), instanceBuffer(ctx, "tlasInstanceBuffer") {
}

void dp::TopLevelAccelerationStructure::create(const dp::AccelerationStructureBuildMode mode) {
    writeMutex.lock();
    instances.clear();
    for (const auto& blas : this->blases) {
        VkAccelerationStructureInstanceKHR accelerationStructureInstance = {
            .transform = blas.getMesh().transform,
            .instanceCustomIndex = blas.getMesh().materialIndex,
            .mask = 0xFF,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference = blas.address,
        };

        instances.push_back(accelerationStructureInstance);
    }
    primitiveCount = instances.size();

    VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress = {};

    // Only copy instance data if there is any.
    const uint32_t increasedInstanceSize = std::max((uint32_t) 1000, primitiveCount);
    if (primitiveCount > 0) {
        if (instanceBuffer.getHandle() != nullptr) {
            instanceBuffer.destroy();
        }

        static const auto instanceSize = sizeof(VkAccelerationStructureInstanceKHR);
        instanceBuffer.create(
            instanceSize * increasedInstanceSize,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        instanceBuffer.memoryCopy(instances.data(), instanceSize * primitiveCount);

        instanceDataDeviceAddress.deviceAddress = instanceBuffer.address;
    }

    geometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {
            .instances = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                .arrayOfPointers = VK_FALSE,
                .data = instanceDataDeviceAddress,
            }
        },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
    };

    // Get size info
    buildGeometryInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = static_cast<VkAccelerationStructureTypeKHR>(type),
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
        .geometryCount = 1, // This has to be at least 1 for a TLAS.
        .pGeometries = &geometry,
    };

    // We allocate the maximum amount of primitives that we could ever use,
    // so that we don't have to re-create the acceleration structures.
    buildSizes = ctx.getAccelerationStructureBuildSizes(increasedInstanceSize, buildGeometryInfo);

    // Destroy the handle if we don't want to update it.
    if (mode == Build && handle != nullptr) {
        ctx.destroyAccelerationStructure(handle);
        handle = nullptr;
    }

    // Only create the handle if we have to.
    if (handle == nullptr || resultBuffer.getSize() < buildSizes.accelerationStructureSize) {
        createBuildBuffers(scratchBuffer, resultBuffer);

        VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = resultBuffer.getHandle(),
            .size = buildSizes.accelerationStructureSize,
            .type = static_cast<VkAccelerationStructureTypeKHR>(type),
        };
        if (handle != nullptr) previousHandle = handle; // Save current handle before creating a new one.
        auto result = ctx.createAccelerationStructure(accelerationStructureCreateInfo, &handle);
        checkResult(result, "Failed to create acceleration structure");
        address = ctx.getAccelerationStructureDeviceAddress(handle);
    }

    if (mode == Update) {
        buildGeometryInfo.srcAccelerationStructure = previousHandle == nullptr
            ? handle
            : previousHandle;
    }
    buildGeometryInfo.dstAccelerationStructure = handle;
    buildGeometryInfo.mode = static_cast<VkBuildAccelerationStructureModeKHR>(mode);
    buildGeometryInfo.scratchData = { .deviceAddress = scratchBuffer.address, };

    this->setName();
    writeMutex.unlock();
}

void dp::TopLevelAccelerationStructure::cleanup() {
    writeMutex.lock();
    if (previousHandle != handle && previousHandle != nullptr) {
        ctx.destroyAccelerationStructure(previousHandle);
        previousHandle = nullptr;
    }
    writeMutex.unlock();
}

void dp::TopLevelAccelerationStructure::updateBLASes(const std::vector<BottomLevelAccelerationStructure>& newBlases) {
    writeMutex.lock();
    blases.insert(blases.begin(), newBlases.begin(), newBlases.end());
    instances.clear();
    writeMutex.unlock();
}
