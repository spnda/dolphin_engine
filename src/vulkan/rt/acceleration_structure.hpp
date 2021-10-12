#pragma once

#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "../resource/buffer.hpp"
#include "../resource/stagingbuffer.hpp"
#include "../../render/mesh.hpp"

namespace dp {
    // fwd
    class Context;
    
    enum AccelerationStructureType : uint32_t {
        TopLevel = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        BottomLevel = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        Generic = VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR
    };

    struct AccelerationStructureInstance {
        VkTransformMatrixKHR transformMatrix;
        VkGeometryInstanceFlagBitsKHR flags;
        uint32_t blasIndex; // TODO: Have a better system of referencing the mesh.
    };

    class AccelerationStructure {
    protected:
        const dp::Context& ctx;

    public:
        std::string name;
        dp::AccelerationStructureType type = AccelerationStructureType::Generic;
        VkAccelerationStructureKHR handle = nullptr;
        VkDeviceAddress address = 0;
        dp::Buffer resultBuffer;

        explicit AccelerationStructure(const dp::Context& context, AccelerationStructureType type = AccelerationStructureType::Generic, std::string asName = {});
        AccelerationStructure(const AccelerationStructure& structure);

        AccelerationStructure& operator=(const dp::AccelerationStructure& structure);

        void setName();
    };

    class BottomLevelAccelerationStructure : public AccelerationStructure {
        dp::Mesh mesh;

        dp::StagingBuffer vertexStagingBuffer;
        dp::StagingBuffer indexStagingBuffer;
        dp::StagingBuffer transformStagingBuffer;

    public:
        dp::Buffer vertexBuffer;
        dp::Buffer indexBuffer;
        dp::Buffer transformBuffer;

        explicit BottomLevelAccelerationStructure(const dp::Context& context, dp::Mesh  mesh, const std::string& name = "blas");

        void createMeshBuffers();
        void copyMeshBuffers(VkCommandBuffer cmdBuffer);
        void destroyMeshStagingBuffers();
    };

    class TopLevelAccelerationStructure : public AccelerationStructure {
    public:
        std::vector<BottomLevelAccelerationStructure> blases = {};

        explicit TopLevelAccelerationStructure(const dp::Context& context, const std::string& name = "tlas");
    };
}
