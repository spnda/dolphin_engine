#pragma once

#include <string>
#include <tuple>
#include <vector>

#include "../resource/buffer.hpp"
#include "../../render/mesh.hpp"

namespace dp {
    class AccelerationStructureInstance;
    class AccelerationStructure;
    class TopLevelAccelerationStructure;

    class AccelerationStructureBuilder {
        AccelerationStructureBuilder(const dp::Context& context, VkCommandPool commandPool);

        const Context& ctx;
        const VkCommandPool commandPool;

        std::vector<Mesh> meshes;
        std::vector<AccelerationStructureInstance> instances;

        VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
        };

        void createBuildBuffers(dp::Buffer& scratchBuffer, dp::Buffer& resultBuffer, VkAccelerationStructureBuildSizesInfoKHR sizeInfo) const ;

    public:
        static AccelerationStructureBuilder create(const Context& context, VkCommandPool commandPool);

        uint32_t addMesh(const Mesh& mesh);

        void addInstance(AccelerationStructureInstance instance);

        TopLevelAccelerationStructure build();
    };
}
