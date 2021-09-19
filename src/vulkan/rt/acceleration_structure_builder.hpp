#pragma once

#include <string>
#include <tuple>
#include <vector>

#include "../resource/buffer.hpp"
#include "../../render/mesh.hpp"

namespace dp {
    // fwd
    class Context;

    struct AccelerationStructureInstance {
        VkTransformMatrixKHR transformMatrix;
        VkGeometryInstanceFlagBitsKHR flags;
        uint32_t blasIndex; // TODO: Have a better system of referencing the mesh.
    };

    class AccelerationStructure {
        const dp::Context& ctx;
    
    public:
        std::string name;
        VkAccelerationStructureKHR handle;
        VkDeviceAddress address;
        dp::Buffer resultBuffer;

        AccelerationStructure(const dp::Context& context, const std::string asName = "blas");

        AccelerationStructure& operator=(const dp::AccelerationStructure &);

        void setName();
    };

    class AccelerationStructureBuilder {
        AccelerationStructureBuilder(const dp::Context& context);

        const Context& context;

        // Static vector used to delete all structures.
        inline static std::vector<AccelerationStructure> structures;

        std::vector<Mesh> meshes;
        std::vector<AccelerationStructureInstance> instances;

        void createMeshBuffers(dp::Buffer& vertexBuffer, dp::Buffer& indexBuffer, dp::Buffer& transformBuffer, const Mesh& mesh);

        void createBuildBuffers(dp::Buffer& scratchBuffer, dp::Buffer& resultBuffer, const VkAccelerationStructureBuildSizesInfoKHR sizeInfo) const;

    public:
        static AccelerationStructureBuilder create(const Context& context);
        static void destroyAllStructures(const dp::Context& ctx);

        uint32_t addMesh(Mesh mesh);

        void addInstance(AccelerationStructureInstance instance);

        AccelerationStructure build();
    };
}
