#pragma once

#include "acceleration_structure.hpp"

namespace dp {
    // fwd
    class BottomLevelAccelerationStructure;

    class TopLevelAccelerationStructure final : public AccelerationStructure {
    public:
        TopLevelAccelerationStructure(const dp::Context& context, const VkDeviceAddress instanceAddress, const uint32_t instanceCount);

        void generate(VkCommandBuffer buffer, dp::Buffer& scratchBuffer, dp::Buffer& resultBuffer, const VkDeviceSize resultOffset);

        static VkAccelerationStructureInstanceKHR createInstance(
            const dp::Context& context,
            const BottomLevelAccelerationStructure& bottomLevelStructure,
            const VkTransformMatrixKHR transformMatrix,
            uint32_t instanceId,
            uint32_t hitGroupId);

    private:
        const uint32_t instancesCount;
        VkAccelerationStructureGeometryInstancesDataKHR instancesData = {};
        VkAccelerationStructureGeometryKHR topLevelGeometry = {};
    };
}
