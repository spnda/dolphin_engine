#pragma once

#include <tuple>

#include "../context.hpp"
#include "../resource/buffer.hpp"
#include "../../render/mesh.hpp"

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
        uint32_t blasIndex; // TODO: Have a better system of referencing the mesh.
    };

    struct AccelerationStructure {
        VkAccelerationStructureKHR handle;
        VkDeviceAddress address;
        dp::Buffer resultBuffer;

        AccelerationStructure(const dp::Context& context)
                : resultBuffer(context, "blasResultBuffer") {}
    };

    class AccelerationStructureBuilder {
    private:
        AccelerationStructureBuilder(const dp::Context& context);

        const Context& context;

        // Static vector used to delete all structures.
        inline static std::vector<AccelerationStructure> structures;

        std::vector<AccelerationStructureMesh> meshes;
        std::vector<AccelerationStructureInstance> instances;

        void createMeshBuffers(dp::Buffer& vertexBuffer, dp::Buffer& indexBuffer, dp::Buffer& transformBuffer, const AccelerationStructureMesh& mesh);

        void createBuildBuffers(dp::Buffer& scratchBuffer, dp::Buffer& resultBuffer, const VkAccelerationStructureBuildSizesInfoKHR sizeInfo) const;

    public:
        static AccelerationStructureBuilder create(const Context& context);
        static void destroyAllStructures(const dp::Context& ctx);

        uint32_t addMesh(AccelerationStructureMesh mesh);

        void addInstance(AccelerationStructureInstance instance);

        AccelerationStructure build();
    };
}
