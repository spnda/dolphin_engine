#pragma once

#include <string>
#include <tuple>
#include <vector>

#include "../resource/buffer.hpp"
#include "../../render/mesh.hpp"

namespace dp {
    class AccelerationStructureInstance;
    class AccelerationStructure;

    class AccelerationStructureBuilder {
        AccelerationStructureBuilder(const dp::Context& context, const VkCommandPool commandPool);

        const Context& context;
        const VkCommandPool commandPool;

        // Static vector used to delete all structures.
        inline static std::vector<AccelerationStructure> structures;

        std::vector<Mesh> meshes;
        std::vector<AccelerationStructureInstance> instances;

        void createMeshBuffers(dp::Buffer& vertexBuffer, dp::Buffer& indexBuffer, dp::Buffer& transformBuffer, const Mesh& mesh);

        void createBuildBuffers(dp::Buffer& scratchBuffer, dp::Buffer& resultBuffer, const VkAccelerationStructureBuildSizesInfoKHR sizeInfo) const;

    public:
        static AccelerationStructureBuilder create(const Context& context, const VkCommandPool commandPool);
        static void destroyAllStructures(const dp::Context& ctx);

        uint32_t addMesh(Mesh mesh);

        void addInstance(AccelerationStructureInstance instance);

        AccelerationStructure build();
    };
}
