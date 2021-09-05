#pragma once

#include <tuple>

#include "../context.hpp"
#include "../resource/vertex.hpp"

#include "bottom_level_acceleration_structure.hpp"
#include "top_level_acceleration_structure.hpp"

namespace dp {
    struct AccelerationStructureMesh {
        std::vector<Vertex> vertices;
        VkFormat format;

        std::vector<uint32_t> indices;
        VkIndexType indexType;

        uint32_t stride;

        VkTransformMatrixKHR transformMatrix;
    };

    struct AccelerationStructureInstance {
        VkTransformMatrixKHR transformMatrix;
        VkGeometryInstanceFlagBitsKHR flags;
        uint32_t blasAddress;
    };

    class AccelerationStructureBuilder {
    private:
        AccelerationStructureBuilder(const dp::Context& context);

        const Context& context;

        std::vector<AccelerationStructureMesh> meshes;
        AccelerationStructureInstance instance;

        std::vector<class BottomLevelAccelerationStructure> bottomAccelerationStructures;
        std::vector<class TopLevelAccelerationStructure> topAccelerationStructures;

        void createMeshBuffers(dp::Buffer& vertexBuffer, dp::Buffer& indexBuffer, dp::Buffer& transformBuffer, const AccelerationStructureMesh& mesh);

        void createBuildBuffers(dp::Buffer& scratchBuffer, dp::Buffer& resultBuffer, const VkAccelerationStructureBuildSizesInfoKHR sizeInfo) const;

    public:
        static AccelerationStructureBuilder create(const Context& context);

        uint32_t addMesh(AccelerationStructureMesh mesh);

        void setInstance(AccelerationStructureInstance instance);

        TopLevelAccelerationStructure build();
    };
}
